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
		// Global Varaibles //////
		constexpr int MAX_CLIENTS = 12;

		// Time to wait at socket selector
		constexpr int WAIT_TIME_MS = 10;

		// State update interval
		constexpr int UPDATE_INTERVAL_MS = 50;

		constexpr int PING_INTERVAL_MS = 250;

		// How long to hold onto snapshots
		constexpr int SNAPSHOT_RETENTION_MS = 1000;

		// The longest frame time the server is willing to accept from a client
		// NOTE: An SFML window will freeze while it is being moved,
		// which may cause the server to drop that client
		constexpr int COMMAND_FRAME_TIME_TRESHOLD_MS = 1500;

		// Thread stuff
		std::thread gServerThread;
		std::atomic<bool> gIsServerRunning;

		// Networking
		sf::TcpListener gListener;
		sf::SocketSelector gSelector;
		std::vector<ConnectionPtr> gConnections;

		time_point gNextPingPoint;
		time_point gNextUpdatePoint;

		// Game
		World gWorld;
		ms gElapsedTime;

		// Entities' previous positions
		std::vector<WorldSnapshot> gSnapshots;

		// SEND FUNCTIONS //////////////////////////////////

		DEF_SERVER_SEND(PACKET_SERVER_WELCOME)
		{
			//// Lets the client know they have successfully joined the game
			// connection->pid: The client's id
			// rot: How many degrees the client should rotate their view by

			if (connection->status != STATUS_JOINING)
				return;

			// Rotate the client's view 180 degrees if it is playing on the top lane
			// NOTE: Would be better to let the client handle this
			float rot = (gWorld.IsPlayerTopLane(connection->pid)) ? 180.f : 0.f;

			auto p = InitPacket(PACKET_SERVER_WELCOME);
			p << connection->pid << rot;

			connection->status = STATUS_PLAYING;
			connection->Send(p);
		}

		DEF_SERVER_SEND(PACKET_SERVER_SPECTATOR)
		{
			//// Lets the client know they are only going to be a spectator

			if (connection->status != STATUS_JOINING)
				return;

			auto p = InitPacket(PACKET_SERVER_SPECTATOR);

			connection->status = STATUS_SPECTATING;
			connection->Send(p);
		}

		DEF_SERVER_SEND(PACKET_SERVER_FULL)
		{
			//// Lets the client know the server does not accept any more clients

			if (connection->status != STATUS_JOINING)
				return;

			auto p = InitPacket(PACKET_SERVER_FULL);

			debug << "SERVER: A client tried to join, but we are full" << std::endl;
			connection->Send(p);
		}

		DEF_SEND_PARAM(PACKET_SERVER_PING)(ConnectionPtr connection, uint64_t timestamp, bool pingBack)
		{
			//// Respond to a client's ping message by sending back the time they sent their ping
			// pingBack: If we want the client to ping us back
			// timestamp: if pingBack is true, this is the server's timestamp
			//			  if pingBack is false, it is the client's timestamp

			if (connection->status != STATUS_PLAYING)
				return;

			auto p = InitPacket(PACKET_SERVER_PING);
			p << pingBack << timestamp;

			connection->Send(p);
		}

		DEF_SEND_PARAM(PACKET_SERVER_UPDATE)(ConnectionPtr connection, const WorldSnapshot& snapshot)
		{
			//// Send the client the state of the server's simulation
			// snapshot: The state of the server's simulation
			// ping: a flag to say if the client should ping us back so we can measure latency
			//			(we include a timestamp in the snapshot, which the client will send back to us)

			if (connection->status == STATUS_JOINING || connection->status == STATUS_NONE)
				return;

			auto p = InitPacket(PACKET_SERVER_UPDATE);
			p << snapshot;

			connection->Send(p);
		}

		DEF_SEND_PARAM(PACKET_SERVER_SHOOT)(ConnectionPtr connection, const Bullet& bullet)
		{
			//// Inform a client that a bullet has been fired
			// bullet: The bullet in question
			// gElapsedTime.count(): Timestamp of when the bullet was spawned

			auto p = InitPacket(PACKET_SERVER_SHOOT);
			p << bullet << gElapsedTime.count();

			connection->Send(p);
		}

		// RECEIVE FUNCTIONS ///////////////////////////////

		DEF_SERVER_RECV(PACKET_CLIENT_JOIN)
		{
			//// Packet sent by client requesting to join our server

			if (connection->status != STATUS_JOINING)
				return;

			Player player;

			// Arbitrarily decide a colour for the player
			static const uint32_t PADDLE_COLOURS[] = {
				0xA0A0FFFF,
				0xFFA0A0FF,
				0xA0FFA0FF,
				0xA0FFFFFF,
				0xFFA0FFFF,
				0xFFA0A0FF
			}; // Size 6

			// NOTE: Very silly.
			player.SetColour(PADDLE_COLOURS[gConnections.size() % 6]);

			// Try adding a new player to the simulation
			if (gWorld.AddPlayer(player))
			{
				connection->pid = player.GetID();

				debug << "SERVER: Sent client #" << (int) player.GetID() << " welcome packet" << std::endl;
				SEND(PACKET_SERVER_WELCOME)(connection);
			}
			// Try letting the client spectate
			else if (gConnections.size() < MAX_CLIENTS)
			{
				debug << "SERVER: Reached MAX_PLAYERS, new spectator joined" << std::endl;
				SEND(PACKET_SERVER_SPECTATOR)(connection);
			}
			// We do not accept any more clients
			else
			{
				SEND(PACKET_SERVER_FULL)(connection);
				connection->active = false;
			}
		}

		DEF_SERVER_RECV(PACKET_CLIENT_CMD)
		{
			//// Command packet, containing movement information from a client
			// cmd: The command packet

			if (connection->status != STATUS_PLAYING)
				return;

			Command cmd;
			p >> cmd;

			// Do not allow the client to move too far
			if (cmd.dt > COMMAND_FRAME_TIME_TRESHOLD_MS)
			{
				debug << "SERVER: Client #" << (int) connection->pid << " was dropped because of too high frame time (" << cmd.dt << ')' << std::endl;
				// Assume they are trying to cheat and disconnect the client
				// TODO: Send the client an error message, letting them know why they disconnected
				connection->active = false;
				return;
			}

			gWorld.RunCommand(cmd, connection->pid, false);
		}

		DEF_SERVER_RECV(PACKET_CLIENT_PING)
		{
			//// Response ping packet from the client
			// serverTime: Our initial ping-packet's timestamp
			// clientTime: The client's timestamp

			if (connection->status != STATUS_PLAYING)
				return;

			uint64_t serverTime, clientTime;
			p >> serverTime >> clientTime;

			connection->latency = gElapsedTime - ms(serverTime);

			SEND(PACKET_SERVER_PING)(connection, clientTime, false);
		}

		DEF_SERVER_RECV(PACKET_CLIENT_SHOOT)
		{
			//// A request* from a client to fire a bullet
			//// * = As long as the client is a player, the server never says no

			if (connection->status != STATUS_PLAYING)
				return;

			// The time at which the shot was fired by the client
			uint64_t shotFiredTime = gElapsedTime.count() - connection->latency.count();

			auto snapshotIterator = std::find_if(gSnapshots.begin(), gSnapshots.end(), [&](const auto& ss) {
				return ss.serverTime >= shotFiredTime;
			});

			World shotFiredSnapshot;

			// If we could not find the snapshot where the player fired (it's too old)
			if (snapshotIterator == gSnapshots.end())
			{
				// Disconnect the client
				debug << "SERVER: Client #" << (int) connection->pid << " requested to fire a bullet, but the snapshot was lost" << std::endl;
				// TODO: Let the client know why they disconnected
				connection->active = false;
				return;
			}

			shotFiredSnapshot = snapshotIterator->snapshot;

			debug << "latency: " << connection->latency.count() << " shotFiredTime: " << shotFiredTime << " last snapshot: " << gSnapshots.back().serverTime << std::endl;

			// How far the bullet has travelled since it was fired by the client
			float travelledDistance = Bullet::BULLET_SPEED * connection->latency.count() / 1000.f;

			// The place where the bullet was fired from
			auto bulletPosition = shotFiredSnapshot.GetPlayer(connection->pid)->GetPosition();

			bulletPosition.y += (gWorld.IsPlayerTopLane(connection->pid) ? travelledDistance : -travelledDistance);
			Bullet bullet = gWorld.PlayerShoot(connection->pid, bulletPosition);

			// Inform the other clients that a bullet has been fired
			for (auto& otherConnection : gConnections)
			{
				if (otherConnection != connection)
					SEND(PACKET_SERVER_SHOOT)(otherConnection, bullet);
			}
		}

		using ServerReceiveCallback = std::function<void(ConnectionPtr, sf::Packet&)>;
		const ServerReceiveCallback _receivePacket[] = {
				RECV(PACKET_CLIENT_JOIN),
				nullptr,					// PACKET_SERVER_WELCOME
				nullptr,					// PACKET_SERVER_SPECTATOR
				nullptr,					// PACKET_SERVER_FULL
				RECV(PACKET_CLIENT_CMD),
				nullptr,					// PACKET_SERVER_PING
				RECV(PACKET_CLIENT_PING),
				nullptr,					// PACKET_SERVER_UPDATE
				RECV(PACKET_CLIENT_SHOOT),
				nullptr,					// PACKET_SERVER_SHOOT
		};

		// Server logic ///////

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

		void UpdateClients()
		{
			if (the_clock::now() >= gNextUpdatePoint)
			{
				WorldSnapshot snapshot;
				snapshot.snapshot = gWorld;
				snapshot.serverTime = gElapsedTime.count();

				bool ping = (the_clock::now() >= gNextPingPoint);

				if (ping)
					gNextPingPoint = the_clock::now() + ms(PING_INTERVAL_MS);

				for (auto& connection : gConnections)
				{
					SEND(PACKET_SERVER_UPDATE)(connection, snapshot);

					if (ping)
						SEND(PACKET_SERVER_PING)(connection, snapshot.serverTime, true);
				}

				gNextUpdatePoint = the_clock::now() + ms(UPDATE_INTERVAL_MS);
			}
		}

		void DeleteOldSnapshots()
		{
			auto pred = [&](const auto& snapshot)
			{
				return (gElapsedTime.count() - snapshot.serverTime) >= SNAPSHOT_RETENTION_MS;
			};

			gSnapshots.erase(
				std::remove_if(gSnapshots.begin(), gSnapshots.end(), pred),
				gSnapshots.end()
			);
		}

		void ServerTask(const sf::IpAddress& address, Port port)
		{
			gIsServerRunning = true;
			gNextPingPoint = the_clock::now() + ms(PING_INTERVAL_MS);
			gNextUpdatePoint = the_clock::now() + ms(UPDATE_INTERVAL_MS);

			if (!StartListening(address, port))
				return;

			ms dt{ 0 };
			while (gIsServerRunning)
			{
				auto startFrame = the_clock::now();

				ReceiveFromClients();

				gWorld.Update(dt.count());

				DeleteOldSnapshots();

				WorldSnapshot ss;
				ss.snapshot = gWorld;
				ss.serverTime = gElapsedTime.count();
				gSnapshots.push_back(ss);

				UpdateClients();

				auto endFrame = the_clock::now();

				dt = std::chrono::duration_cast<ms>(endFrame - startFrame);
				gElapsedTime += dt;
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
