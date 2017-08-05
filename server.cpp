#include "server.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <functional>
#include "common.h"
#include "world.h"
#include "command.h"

// Oh dear
#define	SPAWN_LOC_1		sf::Vector2f(400.f, 32.f)
#define SPAWN_LOC_2		sf::Vector2f(400.f, 568.f)

namespace Network
{
	namespace Server
	{
		constexpr int UPDATE_INTERVAL_MS = 300;

		// Threads
		std::thread _serverThread;
		std::atomic<bool> _isServerRunning;

		// Networking
		sf::TcpListener _listener;
		sf::SocketSelector _selector;

		time_point _nextUpdatePoint;

		std::vector<ConnectionPtr> _connections;

		// Game
		World _world;
		uint64_t _elapsedTime;

		std::vector<ConnectionPtr>::iterator DropClient(ConnectionPtr connection);

		// SEND FUNCTIONS //////////////////////////////////

		DEF_SERVER_SEND(PACKET_SERVER_WELCOME)
		{
			auto p = InitPacket(PACKET_SERVER_WELCOME);
			p << connection->pid;

			connection->socket.send(p);
		}

		DEF_SERVER_SEND(PACKET_SERVER_FULL)
		{
			auto p = InitPacket(PACKET_SERVER_FULL);
			
			std::cout << "SERVER: A client tried to join, but we are at capacity" << std::endl;
			connection->socket.send(p);
		}

		DEF_SEND_PARAM(PACKET_SERVER_UPDATE)(ConnectionPtr connection, const WorldSnapshot& snapshot)
		{
			auto p = InitPacket(PACKET_SERVER_UPDATE);
			p << snapshot;

			connection->socket.send(p);
		}

		// RECEIVE FUNCTIONS ///////////////////////////////

		DEF_SERVER_RECV(PACKET_CLIENT_JOIN)
		{
			uint32_t colour;
			p >> colour;

			Player player;
			player.SetColour(colour);

			if (_world.AddPlayer(player))
			{
				// There is room for this client
				connection->pid = player.pid();

				std::cout << "SERVER: Sent client #" << (int) player.pid() << " welcome packet" << std::endl;
				SEND(PACKET_SERVER_WELCOME)(connection);
			}
			else
			{
				SEND(PACKET_SERVER_FULL)(connection);
				connection->active = false;
			}
		}

		DEF_SERVER_RECV(PACKET_CLIENT_CMD)
		{
			uint8_t pid;
			Command cmd;
			p >> pid >> cmd;

			_world.RunCommand(cmd, pid);
		}

		using ServerReceiveCallback = std::function<void(ConnectionPtr, sf::Packet&)>;
		const ServerReceiveCallback _receivePacket[] = {
				RECV(PACKET_CLIENT_JOIN),
				nullptr,					// PACKET_SERVER_WELCOME
				nullptr,					// PACKET_SERVER_FULL
				RECV(PACKET_CLIENT_CMD),
				nullptr,					// PACKET_SERVER_UPDATE
		};

		bool StartListening(const sf::IpAddress& address, Port port)
		{
			if (_listener.listen(port, address) != Status::Done)
				return false;

			std::cout << "SERVER: Started listening on " << address.toString() << ':' << port << std::endl;
			_listener.setBlocking(false);
			_selector.add(_listener);
			return true;
		}

		void AcceptClients()
		{
			_connections.emplace_back(std::make_shared<Connection>());
			ConnectionPtr newConnection = _connections.back();

			auto ret = _listener.accept(newConnection->socket);

			if (ret != Status::Done)
			{
				std::cout << "SERVER: There was a failed/ignored connection" << std::endl;
				return;
			}

			std::cout << "SERVER: Accepted a new client" << std::endl;

			newConnection->active = true;
			newConnection->socket.setBlocking(false);
			_selector.add(newConnection->socket);
		}

		std::vector<ConnectionPtr>::iterator DropClient(ConnectionPtr connection)
		{
			std::cout << "SERVER: Dropping client " << (int) connection->pid << std::endl;
			_selector.remove(connection->socket);


			_world.RemovePlayer(connection->pid);
			connection->socket.disconnect();

			return _connections.erase(std::remove(_connections.begin(), _connections.end(), connection), _connections.end());
		}

		void Receive(ConnectionPtr connection)
		{
			while (true)
			{
				sf::Packet p;
				auto ret = connection->socket.receive(p);

				switch (ret)
				{
					case Status::Disconnected:
						connection->active = false;
						return;
					case Status::Partial:
						std::cerr << "SERVER: Partial receive" << std::endl;
						connection->active = false;
						return;
					case Status::NotReady:
						return;
				}

				uint8_t type;
				p >> type;

				if (type < PACKET_END && _receivePacket[type] != nullptr && connection->active)
				{
					_receivePacket[type](connection, p);
				}
			}
		}

		void ServerTask(const sf::IpAddress& address, Port port)
		{
			std::cout << "SERVER: Started on thread " << std::this_thread::get_id() << std::endl;

			if (!StartListening(address, port))
				return;

			while (_isServerRunning)
			{
				if (_selector.wait(sf::milliseconds(WAIT_TIME_MS)))
				{
					if (_selector.isReady(_listener))
						AcceptClients();

					for (auto it = _connections.begin(); it != _connections.end(); )
					{
						ConnectionPtr connection = *it;

						if (_selector.isReady(connection->socket))
						{
							if (connection->active)
								Receive(connection);
							else
							{
								it = DropClient(connection);
								continue;
							}
						}

						++it;
					}
				}

				// STATE UPDATE
				if (now() >= _nextUpdatePoint)
				{
					WorldSnapshot snapshot;
					snapshot.snapshot = _world;
					snapshot.serverTime = _elapsedTime;

					for (auto& connection : _connections)
						SEND(PACKET_SERVER_UPDATE)(connection, snapshot);

					_elapsedTime += UPDATE_INTERVAL_MS;
					_nextUpdatePoint = now() + ms(UPDATE_INTERVAL_MS);
				}
			}
		}

		bool StartServer(const sf::IpAddress& address, Port port)
		{
			_nextUpdatePoint = now() + ms(UPDATE_INTERVAL_MS);
			_isServerRunning = true;
			_serverThread = std::thread([&] { ServerTask(address, port); });

			return true;
		}

		void CloseServer()
		{
			std::cout << "SERVER: Closing server" << std::endl;
			_isServerRunning = false;
			_serverThread.join();
		}
	}
}
