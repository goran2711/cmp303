#include "server.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <functional>
#include "common.h"
#include "world.h"
#include "command.h"

namespace Network
{
	namespace Server
	{
		constexpr int MAX_CLIENTS = 12;
		
		// Time to wait at socket selector
		constexpr int WAIT_TIME_MS = 10;

		// State update interval
		constexpr int UPDATE_INTERVAL_MS = 50;

		// Threads
		std::thread _serverThread;
		std::atomic<bool> _isServerRunning;
		std::condition_variable _condServerClosed;

		// Networking
		sf::TcpListener _listener;
		sf::SocketSelector _selector;
		std::vector<ConnectionPtr> _connections;

		time_point _nextUpdatePoint;

		// Game
		World _world;
		uint64_t _elapsedTime;

		std::vector<ConnectionPtr>::iterator DropClient(ConnectionPtr connection);

		// SEND FUNCTIONS //////////////////////////////////

		DEF_SERVER_SEND(PACKET_SERVER_WELCOME)
		{
			float rot = (_connections.size() > 1) ? 180.f : 0.f;
			auto p = InitPacket(PACKET_SERVER_WELCOME);
			p << connection->pid << rot;

			connection->Send(p);
		}

		DEF_SERVER_SEND(PACKET_SERVER_SPECTATOR)
		{
			auto p = InitPacket(PACKET_SERVER_SPECTATOR);
			connection->Send(p);
		}

		DEF_SERVER_SEND(PACKET_SERVER_FULL)
		{
			auto p = InitPacket(PACKET_SERVER_FULL);

			std::cout << "SERVER: A client tried to join, but we are full" << std::endl;
			connection->Send(p);
		}

		DEF_SEND_PARAM(PACKET_SERVER_UPDATE)(ConnectionPtr connection, const WorldSnapshot& snapshot)
		{
			auto p = InitPacket(PACKET_SERVER_UPDATE);
			p << snapshot;

			connection->Send(p);
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
			else if (_connections.size() < MAX_CLIENTS)
			{
				std::cout << "SERVER: Reached MAX_PLAYERS, new spectator joined" << std::endl;
				SEND(PACKET_SERVER_SPECTATOR)(connection);
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

			_world.RunCommand(cmd, pid, false);
		}

		DEF_SERVER_RECV(PACKET_CLIENT_QUIT)
		{
			std::cout << "SERVER: Client #" << (int) connection->pid << " has quit" << std::endl;
			connection->active = false;
		}

		using ServerReceiveCallback = std::function<void(ConnectionPtr, sf::Packet&)>;
		const ServerReceiveCallback _receivePacket[] = {
				RECV(PACKET_CLIENT_JOIN),
				nullptr,					// PACKET_SERVER_WELCOME
				nullptr,					// PACKET_SERVER_SPECTATOR
				nullptr,					// PACKET_SERVER_FULL
				RECV(PACKET_CLIENT_CMD),
				nullptr,					// PACKET_SERVER_UPDATE
				RECV(PACKET_CLIENT_QUIT),
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

			std::cout << "SERVER: Accepted a new client -- " << _connections.size() << " clients connected" << std::endl;

			newConnection->active = true;
			newConnection->socket.setBlocking(false);
			_selector.add(newConnection->socket);
		}

		std::vector<ConnectionPtr>::iterator DropClient(ConnectionPtr connection)
		{
			std::cout << "SERVER: Dropping client " << (int) connection->pid << " -- " << _connections.size() - 1 << " clients connected" << std::endl;

			_selector.remove(connection->socket);

			if (_world.PlayerExists(connection->pid))
				_world.RemovePlayer(connection->pid);

			connection->socket.disconnect();

			return _connections.erase(std::remove(_connections.begin(), _connections.end(), connection), _connections.end());
		}

		void Receive(ConnectionPtr connection)
		{
			while (true)
			{
				sf::Packet p;
				if (!connection->Receive(p))
					break;

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

			_isServerRunning = false;
			_condServerClosed.notify_all();
		}

		bool StartServer(const sf::IpAddress& address, Port port)
		{
			_isServerRunning = true;
			_nextUpdatePoint = now() + ms(UPDATE_INTERVAL_MS);
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
