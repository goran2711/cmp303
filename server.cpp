#include "server.h"
#include <thread>
#include <atomic>
#include <iostream>
#include <functional>
#include "entity.h"
#include "game.h"
#include "common.h"

// Oh dear
#define	SPAWN_LOC_1		sf::Vector2f(400.f, 32.f)
#define SPAWN_LOC_2		sf::Vector2f(400.f, 568.f)

#define STATE_UPDATE_FREQUENCY	250

namespace Network
{
	namespace Server
	{
		// Threads
		std::thread _serverThread;
		std::atomic<bool> _isServerRunning;

		// Networking
		sf::TcpListener _listener;
		sf::SocketSelector _selector;

		time_point _nextStateUpdate;

		// For assigning ids
		ClientID _clientID = 0;

		// RECODE: Raw pointers
		ClientSocket* _clients[MAX_CLIENTS];
		int _clientCount = 0;

		// List because vector moves around in memory as it is resized
		EntityContainer _entities;

		float _dt = 0.016f;

		void DropClient(ClientSocket& cs);

		// SEND FUNCTIONS //////////////////////////////////

		DEF_SEND_PARAM(PACKET_SERVER_JOIN)(ClientSocket& cs, const ClientSocket& other)
		{
			// Inform a client that other has joined

			auto p = InitPacket(PACKET_SERVER_JOIN);
			p << other.info.id << other.info.entity->position;

			cs.send(p);
		}

		DEF_SEND_PARAM(PACKET_SERVER_PLAYER_INFO)(ClientSocket& cs, ClientID cid, const Entity& entity);

		DEF_SERVER_SEND(PACKET_SERVER_WELCOME)
		{
			ClientID newID = _clientID++;
			cs.info.id = newID;

			// Inform new client of their id
			auto p = InitPacket(PACKET_SERVER_WELCOME);

			// Create their entity
			// TODO: In case of spectatos, this cannot just be here
			_entities.emplace_back(std::make_unique<Entity>());
			cs.info.entity = _entities.back().get();
			// RECODE: uid != cid
			cs.info.entity->uid = newID;
			cs.info.entity->position = (_clientCount % 2) ? SPAWN_LOC_1 : SPAWN_LOC_2;

			p << newID << cs.info.entity->position;
			cs.send(p);

			// Inform old clients of new client,
			// and new client of old clients
			for (int i = 0; i < MAX_CLIENTS; ++i)
			{
				if (!_clients[i])
					continue;

				// New of old
				if (_clients[i] != &cs)
					SEND(PACKET_SERVER_JOIN)(cs, *_clients[i]);
				// New client gets the player info of even themselves
				SEND(PACKET_SERVER_PLAYER_INFO)(cs, _clients[i]->info.id, *_clients[i]->info.entity);

				if (_clients[i] == &cs)
					continue;

				// Old of new
				SEND(PACKET_SERVER_JOIN)(*_clients[i], cs);
				SEND(PACKET_SERVER_PLAYER_INFO)(*_clients[i], cs.info.id, *cs.info.entity);
			}
		}

		DEF_SEND_PARAM(PACKET_SERVER_QUIT)(ClientSocket& cs, const ClientSocket& other)
		{
			// Inform a client that other has quit

			auto p = InitPacket(PACKET_SERVER_QUIT);
			p << other.info.id;
			cs.send(p);
		}

		DEF_SEND_PARAM(PACKET_SERVER_PLAYER_INFO)(ClientSocket& cs, ClientID cid, const Entity& player)
		{
			// Inform a client of cid's entity's current position on the server

			auto p = InitPacket(PACKET_SERVER_PLAYER_INFO);
			p << cid << cs.lastSeqNum << player.position;

			cs.send(p);
		}

		// RECEIVE FUNCTIONS ///////////////////////////////

		DEF_SERVER_RECV(PACKET_CLIENT_JOIN)
		{
			std::cout << "SERVER: New client has joined" << std::endl;

			SEND(PACKET_SERVER_WELCOME)(cs);
		}

		DEF_SERVER_RECV(PACKET_CLIENT_QUIT)
		{
			std::cout << "SERVER: Client #" << (int) cs.info.id << " closed their connection" << std::endl;
			std::cerr << "REM: PACKET_CLIENT_QUIT not handled yet" << std::endl;

			cs.hasQuit = true;
			//DropClient(cs);
		}

		DEF_SERVER_RECV(PACKET_CLIENT_MOVE)
		{
			int seqNum;
			sf::Vector2f delta;
			p >> seqNum >> delta;

			auto player = GetEntityByUID(_entities, cs.info.id);

			if (player == _entities.end())
			{
				std::cerr << "SERVER: Cannot find client #" << (int) cs.info.id << "'s entity" << std::endl;
				return;
			}

			// RECODE: Don't want to execute game logic every time a packet is received(?)
			cs.lastSeqNum = seqNum;
			(*player)->position += delta;
		}

		using ServerReceiveCallback = std::function<void(ClientSocket&, sf::Packet&)>;
		const ServerReceiveCallback _receivePacket[] = {
			RECV(PACKET_CLIENT_JOIN),
			nullptr,					// PACKET_SERVER_WELCOME
			nullptr,					// PACKET_SERVER_JOIN
			nullptr,					// PACKET_SERVER_FULL
			RECV(PACKET_CLIENT_QUIT),
			nullptr,					// PACKET_SERVER_QUIT
			nullptr,					// PACKET_SERVER_PLAYER_INFO
			RECV(PACKET_CLIENT_MOVE),
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
			Socket newClient = GetNewSocket();
			auto ret = _listener.accept(*newClient);

			if (ret != Status::Done || _clientCount >= MAX_CLIENTS)
			{
				std::cout << "SERVER: There was a failed/ignored connection" << std::endl;
				if (newClient)
					newClient->disconnect();
				return;
			}

			std::cout << "SERVER: Accepted a new client" << std::endl;

			newClient->setBlocking(false);
			_selector.add(*newClient);

			// RECODE: Raw pointers
			_clients[_clientCount++] = new ClientSocket(std::move(newClient));
		}

		void DropClient(ClientSocket& cs)
		{
			std::cout << "SERVER: Dropping client " << (int) cs.info.id << std::endl;
			_selector.remove(*cs.socket);

			for (int i = 0; i < _clientCount; ++i)
			{
				if (cs.info.id == _clients[i]->info.id)
				{
					delete _clients[i];
					_clients[i] = nullptr;
					break;
				}
				else
				{
					SEND(PACKET_SERVER_QUIT)(*_clients[i], cs);
				}
			}

			--_clientCount;
		}

		void Receive(ClientSocket& cs)
		{
			while (true)
			{
				sf::Packet p;
				auto ret = cs.receive(p);

				switch (ret)
				{
					case Status::Disconnected:
						DropClient(cs);
						return;
					case Status::Partial:
						std::cerr << "SERVER: Partial receive" << std::endl;
						break;
					case Status::NotReady:
						return;
				}

				uint8_t type;
				p >> type;

				if (type < PACKET_END && _receivePacket[type] != nullptr && !cs.hasQuit)
				{
					_receivePacket[type](cs, p);
				}
			}
		}

		void ServerTask(const sf::IpAddress& address, Port port)
		{
			std::cout << "SERVER: Started on thread " << std::this_thread::get_id() << std::endl;

			if (!StartListening(address, port))
				return;

			sf::Clock deltaClock;
			while (_isServerRunning)
			{
				if (_selector.wait(sf::milliseconds(WAIT_TIME_MS)))
				{
					if (_selector.isReady(_listener))
						AcceptClients();

					for (int i = 0; i < _clientCount; ++i)
					{
						if (_selector.isReady(*_clients[i]->socket))
							Receive(*_clients[i]);
					}
				}

				// STATE UPDATE
				if (_nextStateUpdate <= now())
				{
					for (auto it = _entities.begin(); it != _entities.end(); ++it)
					{
						for (int i = 0; i < MAX_CLIENTS; ++i)
						{
							if (!_clients[i] || _clients[i]->hasQuit)
								continue;

							if (!_clients[i]->info.entity)
								continue;

							ClientSocket& cs = *_clients[i];
							SEND(PACKET_SERVER_PLAYER_INFO)(cs, (*it)->uid, *(*it));
						}
					}

					_nextStateUpdate = now() + ms(STATE_UPDATE_FREQUENCY);
				}
			}

			_dt = deltaClock.restart().asSeconds();
		}

		bool StartServer(const sf::IpAddress& address, Port port)
		{
			_nextStateUpdate = now() + ms(STATE_UPDATE_FREQUENCY);
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
