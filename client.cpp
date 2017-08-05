#include "client.h"
#include <iostream>
#include <functional>
#include <SFML/Graphics.hpp>
#include <map>
#include "entity.h"
#include "game.h"
#include "common.h"

namespace Network
{
	namespace Client
	{
		// Networking
		sf::TcpSocket _socket;
		sf::SocketSelector _selector;
		bool _isRunning;

		// Graphics
		std::unique_ptr<sf::RenderWindow> _window;

		// Game
		VisualEntity _player;
		VisualEntity _opponent;

		// Client input history
		int _seqNum = 0;
		std::map<int, sf::Vector2f> _deltaHistory;

		// SEND FUNCTIONS //////////////////////////////////

		DEF_CLIENT_SEND(PACKET_CLIENT_JOIN)
		{
			auto p = InitPacket(PACKET_CLIENT_JOIN);
			_socket.send(p);
		}

		DEF_CLIENT_SEND(PACKET_CLIENT_QUIT)
		{
			auto p = InitPacket(PACKET_CLIENT_QUIT);
			_socket.send(p);
		}

		DEF_SEND_PARAM(PACKET_CLIENT_MOVE)(const sf::Vector2f& delta)
		{
			auto p = InitPacket(PACKET_CLIENT_MOVE);
			// NOTE: May be bad
			p << _seqNum << delta;

			_socket.send(p);
		}

		// RECEIVE FUNCTIONS ///////////////////////////////

		DEF_CLIENT_RECV(PACKET_SERVER_WELCOME)
		{
			ClientID id;
			sf::Vector2f initialPos;
			p >> id >> initialPos;

			std::cout << "CLIENT: Server has given me id #" << (int) id << std::endl;

			_player.isPlayer = true;
			_player.sprite.setFillColor(sf::Color(0x00AEAEFF));
			_player.sprite.setSize({ 64.f, 18.f });
			_player.position = initialPos;
			_player.id = id;

			return RECEIVE_ERROR_OK;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_JOIN)
		{
			// Server letting us know of a client that joined
			ClientID id;
			sf::Vector2f initialPosition;
			p >> id >> initialPosition;

			std::cout << "CLIENT: Learned that client #" << (int) id << " has joined" << std::endl;

			// TODO: If spectators, don't do this
			_opponent.position = initialPosition;
			_opponent.sprite.setFillColor(sf::Color(0xAE00AEFF));
			_opponent.sprite.setSize({ 64.f, 18.f });
			_opponent.id = id;

			return RECEIVE_ERROR_OK;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_FULL)
		{
			return RECEIVE_ERROR_OK;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_QUIT)
		{
			ClientID otherID;
			p >> otherID;

			if (otherID == _player.id)
				std::cout << "CLIENT: Server asked us to leave" << std::endl;
			else if (otherID == _opponent.id)
				std::cout << "CLIENT: Opponent left" << std::endl;
			else
				std::cerr << "CLIENT: Unknown client #" << (int) otherID << " left" << std::endl;

			return RECEIVE_ERROR_OK;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_PLAYER_INFO)
		{
			ClientID cid;
			p >> cid;

			int seqNum;
			sf::Vector2f pos;
			p >> seqNum >> pos;

			// TODO: Move this(?)
			// This is us, do reconcilliation
			if (cid == _player.id)
			{
				auto it = _deltaHistory.find(seqNum);
				if (it != _deltaHistory.end())
				{
					it = _deltaHistory.erase(_deltaHistory.begin(), std::next(it));

					while (it != _deltaHistory.end())
					{
						pos += it->second;
						++it;
					}
				}

				_player.position = pos;
			}
			// This is not us, interpolate
			else
				_opponent.AddSample(pos);

			return RECEIVE_ERROR_OK;
		}

		using ClientReceiveCallback = std::function<NetworkReceiveError(sf::Packet&)>;
		const ClientReceiveCallback _receivePacket[] = {
			nullptr,						// PACKET_CLIENT_JOIN
			RECV(PACKET_SERVER_WELCOME),
			RECV(PACKET_SERVER_JOIN),
			RECV(PACKET_SERVER_FULL),
			nullptr,						// PACKET_CLIENT_QUIT
			RECV(PACKET_SERVER_QUIT),
			RECV(PACKET_SERVER_PLAYER_INFO),
			nullptr,						// PACKET_CLIENT_MOVE
		};

		bool ConnectToServer(const sf::IpAddress& address, Port port)
		{
			std::cout << "CLIENT: Connecting to " << address.toString() << ':' << port << std::endl;
			if (_socket.connect(address, port) != Status::Done)
				return false;

			_socket.setBlocking(false);
			_selector.add(_socket);

			SEND(PACKET_CLIENT_JOIN)();

			return true;
		}

		void Disconnect()
		{
			SEND(PACKET_CLIENT_QUIT)();
			_selector.remove(_socket);
			_socket.disconnect();
		}

		bool ReceiveFromServer()
		{
			while (true)
			{
				sf::Packet p;
				auto ret = _socket.receive(p);

				switch (ret)
				{
					case Status::Disconnected:
						std::cerr << "CLIENT: Lost connection to server" << std::endl;
					case Status::Error:
					case Status::Partial:
						return false;
					case Status::NotReady:
						return true;
				}

				uint8_t type;
				p >> type;

				if (type < PACKET_END && _receivePacket[type] != nullptr)
				{
					NetworkReceiveError ret = _receivePacket[type](p);

					// Unrecoverable error, quit.
					if (ret != RECEIVE_ERROR_OK)
						return false;
				}
			}

			return true;
		}

		void RunClient()
		{
			using Key = sf::Keyboard::Key;
			_isRunning = true;

			float dt = 0.016f;
			sf::Clock deltaClock;
			// Main loop
			while (_isRunning)
			{
				_window->clear();

				// Input
				sf::Event event;
				while (_window->pollEvent(event))
				{
					switch (event.type)
					{
						case sf::Event::Closed:
							_isRunning = false;
							break;
						case sf::Event::KeyPressed:
							if (event.key.code == sf::Keyboard::Escape)
								_isRunning = false;
							break;
					}
				}

				// Movement code
				if (_window->hasFocus())
				{
					float speed = 0.f;

					if (sf::Keyboard::isKeyPressed(Key::D))
						speed = PADDLE_SPEED;
					else if (sf::Keyboard::isKeyPressed(Key::A))
						speed = -PADDLE_SPEED;

					if (speed != 0.f)
					{
						auto delta = Game::MoveEntity(_player, { speed, 0.f }, dt);
						SEND(PACKET_CLIENT_MOVE)(delta);
						_deltaHistory[_seqNum++] = delta;
					}
				}

				// Networking
				if (_selector.wait(sf::milliseconds(WAIT_TIME_MS)))
				{
					if (_selector.isReady(_socket))
					{
						if (!ReceiveFromServer())
							_isRunning = false;
					}
				}

				// RECODE: Gross
				if (_player.id != INVALID_CID)
				{
					_player.Draw(*_window);
				}

				if (_opponent.id != INVALID_CID)
				{
					_opponent.Interpolate(dt);
					_opponent.Draw(*_window);
				}

				_window->display();
				dt = deltaClock.restart().asSeconds();
			}
		}

		bool StartClient(const sf::IpAddress& address, Port port)
		{
			if (!ConnectToServer(address, port))
				return false;

			_window = std::make_unique<sf::RenderWindow>(sf::VideoMode(800, 600), "CMP303");
			_window->setFramerateLimit(60);
			_window->requestFocus();

			//_playerSprite.setSize({ 64, 12 });
			//_playerSprite.setFillColor(sf::Color(0x00AEAEFF));

			//_opponentSprite.setSize({ 64, 12 });
			//_opponentSprite.setFillColor(sf::Color(0xAE00AEFF));

			RunClient();

			Disconnect();

			return true;
		}
	}
}