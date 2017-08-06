#include "client.h"
#include <iostream>
#include <functional>
#include <SFML/Graphics.hpp>
#include "world.h"
#include "command.h"
#include "common.h"

#define INTERPOLATION_TIME_MS	250

namespace Network
{
	namespace Client
	{
		// Networking
		sf::TcpSocket _socket;
		bool _isRunning;

		bool _isPredicting = true;
		bool _isReconciling = true;
		bool _isInterpolating = true;

		enum ClientStatus
		{
			STATUS_NONE,
			STATUS_JOINING,
			STATUS_PLAYING,
			STATUS_SPECTATING,
		} _status;

		// Graphics
		std::unique_ptr<sf::RenderWindow> _window;

		// Game
		uint64_t _elapsedTime = 0;

		uint8_t _myID;
		World _world;

		std::list<Command> _commands;
		std::list<WorldSnapshot> _snapshots;

		bool _viewInverted = false;

		void InitializeWindow(const char* title)
		{
			_window = std::make_unique<sf::RenderWindow>(sf::VideoMode(VP_WIDTH, VP_HEIGHT), title);
			_window->setFramerateLimit(30);
			_window->requestFocus();
		}

		uint64_t GetRenderTime()
		{
			return (_elapsedTime > INTERPOLATION_TIME_MS) ? _elapsedTime - INTERPOLATION_TIME_MS : 0;
		}

		void DeleteOldSnapshots(uint64_t renderTime)
		{
			for (auto it = _snapshots.rbegin(); it != _snapshots.rend(); ++it)
			{
				// If this snapshot came before our render time
				if (it->clientTime <= renderTime)
				{
					// Start deleting from this point
					_snapshots.erase(_snapshots.begin(), (++it).base());
					break;
				}
			}
		}

		// SEND FUNCTIONS //////////////////////////////////

		DEF_CLIENT_SEND(PACKET_CLIENT_JOIN)
		{
			auto p = InitPacket(PACKET_CLIENT_JOIN);
			p << 0xA0FFA0FF;

			std::cout << "CLIENT: Sent join request to server" << std::endl;
			_socket.send(p);
			_status = STATUS_JOINING;
		}

		DEF_SEND_PARAM(PACKET_CLIENT_CMD)(const Command& cmd)
		{
			auto p = InitPacket(PACKET_CLIENT_CMD);
			p << _myID << cmd;

			_socket.send(p);
		}

		DEF_CLIENT_SEND(PACKET_CLIENT_QUIT)
		{
			auto p = InitPacket(PACKET_CLIENT_QUIT);

			_socket.send(p);
		}

		// RECEIVE FUNCTIONS ///////////////////////////////
		// If any of the receive functions return false,
		// the client will disconnect.

		DEF_CLIENT_RECV(PACKET_SERVER_WELCOME)
		{
			float viewRotation;
			p >> _myID >> viewRotation;

			std::cout << "CLIENT: Server assigned us id #" << (int) _myID << std::endl;

			char title[32];
			sprintf_s(title, "Client #%d", _myID);
			InitializeWindow(title);

			if (viewRotation != 0.f)
			{
				sf::View view = _window->getView();
				view.setRotation(viewRotation);
				_window->setView(view);

				_viewInverted = true;
			}

			_status = STATUS_PLAYING;
			return true;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_SPECTATOR)
		{
			std::cout << "CLIENT: No more available player slots; we are a spectator" << std::endl;

			InitializeWindow("Spectating");

			_status = STATUS_SPECTATING;
			return true;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_FULL)
		{
			std::cout << "CLIENT: Could not join server because it is full" << std::endl;

			return false;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_UPDATE)
		{
			// We don't care about state updates
			// if we are still waiting to learn if
			// we can join the server
			if (_status == STATUS_JOINING)
				return true;

			WorldSnapshot snapshot;
			p >> snapshot;

			// Overwrite last snapshot if we receive two snapshots in the same frame
			if (_snapshots.empty() || _snapshots.back().clientTime != _elapsedTime)
				_snapshots.push_back(snapshot);

			_snapshots.back().clientTime = _elapsedTime;

			// Delete old snapshots
			uint64_t renderTime = GetRenderTime();

			// Delete old (irrelevant) snapshots
			DeleteOldSnapshots(renderTime);

			// Update world
			_world = _snapshots.back().snapshot;

			// Reconciliation
			if (_isReconciling && !_commands.empty())
			{
				Player* me = _world.GetPlayer(_myID);
				if (me)
				{
					// Last commandID on server
					uint32_t lastCommandID = me->lastCommandID();

					// Remove older commands
					const auto pred = [lastCommandID](const auto& cmd)
					{
						return cmd.id <= lastCommandID;
					};

					auto it = std::remove_if(_commands.begin(), _commands.end(), pred);
					_commands.erase(it, _commands.end());

					// Reapply commands the server had not yet processed
					for (const auto& cmd : _commands)
						_world.RunCommand(cmd, _myID, true);
				}
			}

			return true;
		}

		using ClientReceiveCallback = std::function<bool(sf::Packet&)>;
		const ClientReceiveCallback _receivePacket[] = {
			nullptr,						// PACKET_CLIENT_JOIN
			RECV(PACKET_SERVER_WELCOME),
			RECV(PACKET_SERVER_SPECTATOR),
			RECV(PACKET_SERVER_FULL),
			nullptr,						// PACKET_CLIENT_CMD
			RECV(PACKET_SERVER_UPDATE),
			nullptr,						// PACKET_CLIENT_QUIT
		};

		bool ConnectToServer(const sf::IpAddress& address, Port port)
		{
			std::cout << "CLIENT: Connecting to " << address.toString() << ':' << port << std::endl;
			if (_socket.connect(address, port) != Status::Done)
				return false;

			_socket.setBlocking(false);

			SEND(PACKET_CLIENT_JOIN)();
			return true;
		}

		void Disconnect()
		{
			// TODO: Notify server
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
					if (!_receivePacket[type](p))
						return false;
				}
			}

			return true;
		}

		bool WaitForSelector()
		{
			if (!ReceiveFromServer())
				return false;

			return true;
		}

		bool HandleEvents()
		{
			sf::Event event;
			while (_window->pollEvent(event))
			{
				switch (event.type)
				{
					case sf::Event::Closed:
						return false;
					case sf::Event::KeyPressed:
						if (event.key.code == Key::Escape)
							return false;

						// Control networking strategies
						else if (event.key.code == Key::F1)
						{
							_isPredicting = !_isPredicting;
							std::cout << "Prediction: " << std::boolalpha << _isPredicting << std::endl;

							if (_isReconciling)
							{
								_isReconciling = false;
								std::cout << "Reconciliation: " << std::boolalpha << _isReconciling << std::endl;
							}
						}
						else if (event.key.code == Key::F2)
						{
							_isReconciling = !_isReconciling;
							if (_isReconciling && !_isPredicting)
							{
								_isPredicting = true;
								std::cout << "Prediction: " << std::boolalpha << _isPredicting << std::endl;
							}
							std::cout << "Reconciliation: " << std::boolalpha << _isReconciling << std::endl;
						}
						else if (event.key.code == Key::F3)
						{
							_isInterpolating = !_isInterpolating;
							std::cout << "Interpolation: " << std::boolalpha << _isInterpolating << std::endl;
						}
						break;
				}
			}

			return true;
		}

		void BuildCommand(uint64_t dt)
		{
			static uint32_t commandID = 0;

			if (_window->hasFocus())
			{
				Command cmd;
				cmd.id = commandID++;
				cmd.dt = dt;

				Command::Direction direction = Command::IDLE;

				if (sf::Keyboard::isKeyPressed(Key::A))
					direction = (_viewInverted) ? Command::RIGHT : Command::LEFT;
				if (sf::Keyboard::isKeyPressed(Key::D))
					direction = (_viewInverted) ? Command::LEFT : Command::RIGHT;

				if (direction != Command::IDLE)
				{
					cmd.direction = direction;

					// Client-side prediction
					if (_isPredicting)
					{
						_commands.push_back(cmd);

						_world.RunCommand(cmd, _myID, false);
					}

					// Send to server
					SEND(PACKET_CLIENT_CMD)(cmd);
				}
			}
		}

		// Get the snapshots that we can interpolate postions at t == renderTime from
		std::tuple<WorldSnapshot*, WorldSnapshot*> GetRelevantSnapshots(uint64_t renderTime)
		{
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

			return std::make_tuple(to, from);
		}

		void Interpolate(const WorldSnapshot& from, const WorldSnapshot& to, uint64_t renderTime)
		{
			float alpha = (float) (renderTime - from.clientTime) / (float) (to.clientTime - from.clientTime);

			for (const auto& playerFrom : from.snapshot.GetPlayers())
			{
				for (const auto& playerTo : to.snapshot.GetPlayers())
				{
					// Only interpolate if this is the same player in both snapshots, and it is not us
					if (playerFrom.pid() == playerTo.pid() && playerFrom.pid() != _myID)
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

		void ClientLoop()
		{
			_isRunning = true;

			uint64_t dt = 0;
			sf::Clock deltaClock;

			// Main loop
			while (_isRunning)
			{
				// Networking
				if (!WaitForSelector())
					break;

				switch (_status)
				{
					case STATUS_JOINING:
						continue;
					case STATUS_PLAYING:
						// Movement input
						BuildCommand(dt);
					case STATUS_SPECTATING:
						// Debug and 'meta' input
						if (!HandleEvents())
							_isRunning = false;
				}

				// Interpolation
				if (_isInterpolating)
				{
					uint64_t renderTime = GetRenderTime();

					// Get the two snapshots between which the position @ renderTime exists
					auto snapshots = GetRelevantSnapshots(renderTime);
					WorldSnapshot* to = std::get<0>(snapshots);
					WorldSnapshot* from = std::get<1>(snapshots);

					// If we have enough data
					if (to && from)
						Interpolate(*from, *to, renderTime);
				}

				// Render
				_window->clear();
				World::RenderWorld(_world, *_window);
				_window->display();

				// Timing
				dt = deltaClock.restart().asMilliseconds();
				_elapsedTime += dt;
			}

			std::cout << "CLIENT: Closing..." << std::endl;
			_isRunning = false;
		}

		bool StartClient(const sf::IpAddress& address, Port port)
		{
			if (!ConnectToServer(address, port))
			{
				std::cerr << "CLIENT: Failed to connect to server" << std::endl;
				return false;
			}

			ClientLoop();

			Disconnect();
			return true;
		}
	}
}