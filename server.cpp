#include "server.h"
#include <thread>
#include <functional>
#include "common.h"
#include "world.h"
#include "command.h"
#include "debug.h"

namespace Network
{
	namespace Server
	{
		constexpr int MAX_CLIENTS = 12;

		// Time to wait at socket selector
		constexpr int WAIT_TIME_MS = 10;

		// State update interval
		constexpr int UPDATE_INTERVAL_MS = 50;

		// Thread stuff
		std::thread gServerThread;
		std::atomic<bool> gIsServerRunning;

		// Networking
		sf::TcpListener gListener;
		sf::SocketSelector gSelector;
		std::vector<ConnectionPtr> gConnections;

		time_point gNextUpdatePoint;

		// Game
		World gWorld;
		uint64_t gElapsedTime;

		// SEND FUNCTIONS //////////////////////////////////

		DEF_SERVER_SEND(PACKET_SERVER_WELCOME)
		{
			// Rotate the client's view 180 degrees if it is playing on the top lane
			float rot = (gWorld.IsPlayerTopLane(connection->pid)) ? 180.f : 0.f;

			auto p = InitPacket(PACKET_SERVER_WELCOME);
			p << connection->pid << rot;

			connection->status = STATUS_PLAYING;
			connection->Send(p);
		}

		DEF_SERVER_SEND(PACKET_SERVER_SPECTATOR)
		{
			auto p = InitPacket(PACKET_SERVER_SPECTATOR);

			connection->status = STATUS_SPECTATING;
			connection->Send(p);
		}

		DEF_SERVER_SEND(PACKET_SERVER_FULL)
		{
			auto p = InitPacket(PACKET_SERVER_FULL);

			debug << "SERVER: A client tried to join, but we are full" << std::endl;
			connection->Send(p);
		}

		DEF_SEND_PARAM(PACKET_SERVER_UPDATE)(ConnectionPtr connection, const WorldSnapshot& snapshot)
		{
			if (connection->status == STATUS_JOINING || connection->status == STATUS_NONE)
				return;

			auto p = InitPacket(PACKET_SERVER_UPDATE);
			p << snapshot;

			connection->Send(p);
		}

		// RECEIVE FUNCTIONS ///////////////////////////////

		DEF_SERVER_RECV(PACKET_CLIENT_JOIN)
		{
			if (connection->status != STATUS_JOINING)
				return;

			uint32_t colour;
			p >> colour;

			Player player;
			player.SetColour(colour);

			if (gWorld.AddPlayer(player))
			{
				// There is room for this client
				connection->pid = player.GetID();

				debug << "SERVER: Sent client #" << (int) player.GetID() << " welcome packet" << std::endl;
				SEND(PACKET_SERVER_WELCOME)(connection);
			}
			else if (gConnections.size() < MAX_CLIENTS)
			{
				debug << "SERVER: Reached MAX_PLAYERS, new spectator joined" << std::endl;
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
			if (connection->status != STATUS_PLAYING)
				return;

			uint8_t pid;
			Command cmd;
			p >> pid >> cmd;

			gWorld.RunCommand(cmd, pid, false);
		}

		DEF_SERVER_RECV(PACKET_CLIENT_SHOOT)
		{
			if (connection->status != STATUS_PLAYING)
				return;

			gWorld.PlayerShoot(connection->pid);
		}

		using ServerReceiveCallback = std::function<void(ConnectionPtr, sf::Packet&)>;
		const ServerReceiveCallback _receivePacket[] = {
				RECV(PACKET_CLIENT_JOIN),
				nullptr,					// PACKET_SERVER_WELCOME
				nullptr,					// PACKET_SERVER_SPECTATOR
				nullptr,					// PACKET_SERVER_FULL
				RECV(PACKET_CLIENT_CMD),
				nullptr,					// PACKET_SERVER_UPDATE
				RECV(PACKET_CLIENT_SHOOT),	
		};

		bool StartListening(const sf::IpAddress& address, Port port)
		{
			if (gListener.listen(port, address) != Status::Done)
				return false;

			debug << "SERVER: Started listening on " << address.toString() << ':' << port << std::endl;
			gListener.setBlocking(false);
			gSelector.add(gListener);
			return true;
		}

		void AcceptClients()
		{
			gConnections.emplace_back(std::make_shared<Connection>());
			ConnectionPtr newConnection = gConnections.back();

			auto ret = gListener.accept(newConnection->socket);

			if (ret != Status::Done)
			{
				debug << "SERVER: There was a failed connection" << std::endl;
				return;
			}

			debug << "SERVER: Accepted a new client -- " << gConnections.size() << " clients connected" << std::endl;

			// TODO: Set timeout, so connection is dropped if the client does not send a PACKET_CLIENT_JOIN in time

			newConnection->active = true;
			newConnection->status = STATUS_JOINING;
			newConnection->SetBlocking(false);
			gSelector.add(newConnection->socket);
		}

		auto DropConnection(ConnectionPtr connection)
		{
			debug << "SERVER: Dropping client " << (int) connection->pid << " -- " << gConnections.size() - 1 << " clients connected" << std::endl;

			gSelector.remove(connection->socket);

			if (gWorld.PlayerExists(connection->pid))
				gWorld.RemovePlayer(connection->pid);

			connection->Disconnect();

			return gConnections.erase(std::remove(gConnections.begin(), gConnections.end(), connection), gConnections.end());
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
					_receivePacket[type](connection, p);
			}
		}

		void ReceiveFromClients()
		{
			if (gSelector.wait(sf::milliseconds(WAIT_TIME_MS)))
			{
				if (gSelector.isReady(gListener))
					AcceptClients();

				for (auto it = gConnections.begin(); it != gConnections.end(); )
				{
					ConnectionPtr connection = *it;

					if (gSelector.isReady(connection->socket))
					{
						if (connection->active)
							Receive(connection);
						else
						{
							it = DropConnection(connection);
							continue;
						}
					}

					++it;
				}
			}
		}

		void ServerTask(const sf::IpAddress& address, Port port)
		{
			gIsServerRunning = true;
			gNextUpdatePoint = the_clock::now() + ms(UPDATE_INTERVAL_MS);

			if (!StartListening(address, port))
				return;

			float dt = 0.f;
			sf::Clock deltaClock;
			while (gIsServerRunning)
			{
				ReceiveFromClients();

				gWorld.Update(dt);

				// STATE UPDATE
				if (the_clock::now() >= gNextUpdatePoint)
				{
					WorldSnapshot snapshot;
					snapshot.snapshot = gWorld;
					snapshot.serverTime = gElapsedTime;

					for (auto& connection : gConnections)
						SEND(PACKET_SERVER_UPDATE)(connection, snapshot);

					gElapsedTime += UPDATE_INTERVAL_MS;
					gNextUpdatePoint = the_clock::now() + ms(UPDATE_INTERVAL_MS);
				}
				dt = deltaClock.restart().asSeconds();
			}

			gIsServerRunning = false;
		}

		bool StartServer(const sf::IpAddress& address, Port port)
		{
			gServerThread = std::thread([&] { ServerTask(address, port); });

			return true;
		}

		void CloseServer()
		{
			debug << "SERVER: Closing server" << std::endl;
			gIsServerRunning = false;
			gServerThread.join();
		}
	}
}
