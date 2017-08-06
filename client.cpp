#include "client.h"
#include <iostream>
#include <functional>
#include <SFML/Graphics.hpp>
#include "world.h"
#include "command.h"
#include "common.h"

#define INTERPOLATION_TIME_MS	300

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
		uint64_t _elapsedTime = 0;

		uint8_t _myID;
		World _world;
		std::list<WorldSnapshot> _snapshots;

		// TODO: Store 'commands', which represent the state of the keyboard
		// at a certain frame.
		// Commands are only stored when it's considered an "eventful" frame, i.e. the player moves.
		// They are also only stored when client-side prediction is turned on
		// The commands are run by the player, and are gived to the player through the world
		std::list<Command> _commands;

		// SEND FUNCTIONS //////////////////////////////////

		DEF_CLIENT_SEND(PACKET_CLIENT_JOIN)
		{
			auto p = InitPacket(PACKET_CLIENT_JOIN);
			p << 0xFFFFFFFF;

			std::cout << "CLIENT: Sent join request to server" << std::endl;
			_socket.send(p);
		}

		DEF_SEND_PARAM(PACKET_CLIENT_CMD)(const Command& cmd)
		{
			auto p = InitPacket(PACKET_CLIENT_CMD);
			p << _myID << cmd;

			_socket.send(p);
		}

		// RECEIVE FUNCTIONS ///////////////////////////////

		DEF_CLIENT_RECV(PACKET_SERVER_WELCOME)
		{
			p >> _myID;

			std::cout << "CLIENT: Server assigned us id #" << (int) _myID << std::endl;
			_window = std::make_unique<sf::RenderWindow>(sf::VideoMode(800, 600), "CMP303");
			_window->setFramerateLimit(60);
			_window->requestFocus();

			return RECEIVE_ERROR_OK;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_FULL)
		{
			std::cout << "CLIENT: Could not join server because it is full" << std::endl;

			return RECEIVE_ERROR_OTHER;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_UPDATE)
		{
			WorldSnapshot snapshot;
			p >> snapshot;

			// TODO: Double-check with inspiration for this part: https://github.com/robinarnesson/game-networking/blob/master/client.hpp#L166
			// Overwrite last snapshot if the clientTime hasn't changed
			if (_snapshots.empty() || _snapshots.back().clientTime != _elapsedTime)
				_snapshots.push_back(snapshot);

			_snapshots.back().clientTime = _elapsedTime;

			// Delete old snapshots
			// renderTime: The the at which the current interpolation would have started
			uint64_t renderTime = (_elapsedTime > INTERPOLATION_TIME_MS) ? _elapsedTime - INTERPOLATION_TIME_MS : 0;

			// Delete old (irrelevant) snapshots
			// I.e. snapshots that are older than renderTime

			for (auto it = _snapshots.begin(); it != _snapshots.end(); ++it)
			{
				// First snapshot that is newer than renderTime,
				// so delete all the ones that came before it
				// EXCEPT the previous.
				if (it->clientTime >= renderTime)
				{
					_snapshots.erase(_snapshots.begin(), (it == _snapshots.begin() ? it : --it));
					break;
				}
			}

			_world = _snapshots.back().snapshot;

			// Prediction + reconciliation
			Player* me = _world.GetPlayer(_myID);
			if (me)
			{
				// Last commandID on server
				int lastCommandID = me->lastCommandID();

				// Remove older commands
				_commands.erase(std::remove_if(_commands.begin(), _commands.end(), [lastCommandID](const auto& cmd) { return cmd.id <= lastCommandID; }), _commands.end());

				for (const auto& cmd : _commands)
					_world.RunCommand(cmd, _myID);
			}

			return RECEIVE_ERROR_OK;
		}

		using ClientReceiveCallback = std::function<NetworkReceiveError(sf::Packet&)>;
		const ClientReceiveCallback _receivePacket[] = {
			nullptr,						// PACKET_CLIENT_JOIN
			RECV(PACKET_SERVER_WELCOME),
			RECV(PACKET_SERVER_FULL),
			nullptr,						// PACKET_CLIENT_CMD
			RECV(PACKET_SERVER_UPDATE),
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
			// TODO: Notify server
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
			_isRunning = true;

			sf::RectangleShape rect({ 128.f, 32.f });

			uint32_t commandID = 0;

			uint64_t dt = 0.f;
			sf::Clock deltaClock;

			// Main loop
			while (_isRunning)
			{
				// Networking
				if (_selector.wait(sf::milliseconds(WAIT_TIME_MS)))
				{
					if (_selector.isReady(_socket))
					{
						if (!ReceiveFromServer())
						{
							_isRunning = false;
							break;
						}
					}
				}

				// The RenderWindow is only created when we receive
				// the PACKET_SERVER_WELCOME packet
				if (!_window)
					continue;

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
					Command cmd;
					cmd.id = commandID++;
					cmd.dt = dt;

					Command::Direction direction = Command::IDLE;

					if (sf::Keyboard::isKeyPressed(Key::A))
						direction = Command::LEFT;
					if (sf::Keyboard::isKeyPressed(Key::D))
						direction = Command::RIGHT;

					if (direction != Command::IDLE)
					{
						cmd.direction = direction;
						// if client-side prediction
						{
							_commands.push_back(cmd);

							_world.RunCommand(cmd, _myID);
						}

						// Send to server
						SEND(PACKET_CLIENT_CMD)(cmd);
					}
				}

				// Interpolation
				uint64_t renderTime = (_elapsedTime > INTERPOLATION_TIME_MS) ? _elapsedTime - INTERPOLATION_TIME_MS : 0;

				// Get the two snapshots between which the position @ renderTime exists
				WorldSnapshot* to = nullptr;
				WorldSnapshot* from = nullptr;
				for (auto& snapshot : _snapshots)
				{
					if (snapshot.clientTime > renderTime)
					{
						to = &snapshot;
						break;
					}
					from = &snapshot;
				}

				if (to && from)
				{
					float alpha = (float) (renderTime - from->clientTime) / (float) (to->clientTime - from->clientTime);

					for (auto& playerFrom : from->snapshot.players())
					{
						for (auto playerTo : to->snapshot.players())
						{
							// If this is the same player in both snapshots, and it's not us
							if (playerFrom.pid() == playerTo.pid() && playerTo.pid() != _myID)
							{
								auto playerReal = _world.GetPlayer(playerFrom.pid());
								if (playerReal)
								{
									sf::Vector2f newPos = (playerTo.position() - playerFrom.position()) * alpha + playerFrom.position();
									playerReal->SetPosition(newPos);
								}
							}
						}
					}
				}

				// Render
				World::RenderWorld(_world, *_window);

				_window->display();
				dt = deltaClock.restart().asMilliseconds();
				_elapsedTime += dt;
			}
		}

		bool StartClient(const sf::IpAddress& address, Port port)
		{
			if (!ConnectToServer(address, port))
				return false;

			RunClient();

			Disconnect();
			return true;
		}
	}
}