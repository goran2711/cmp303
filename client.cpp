#include "client.h"
#include <list>
#include <functional>
#include <SFML/Graphics.hpp>
#include "world.h"
#include "command.h"
#include "common.h"
#include "debug.h"

namespace Network
{
	namespace Client
	{
		constexpr int INTERPOLATION_TIME_MS = 250;

		// Networking
		sf::TcpSocket gSocket;
		bool gIsRunning;

		bool gIsPredicting = true;
		bool gIsReconciling = true;
		bool gIsInterpolating = true;
		bool gIsInterpolatingBullets = false;

		enum ClientStatus
		{
			STATUS_NONE,
			STATUS_JOINING,
			STATUS_PLAYING,
			STATUS_SPECTATING,
		} gStatus;

		// Only used to determine the colour this client requests from the server
		bool gIsHost;

		// Graphics
		std::unique_ptr<sf::RenderWindow> gWindow;

		// Game
		uint64_t gElapsedTime = 0;

		uint8_t gMyID;
		World gWorld;

		std::list<Command> gCommands;
		std::list<WorldSnapshot> gSnapshots;

		bool gViewInverted = false;

		void InitializeWindow(const char* title)
		{
			gWindow = std::make_unique<sf::RenderWindow>(sf::VideoMode(VP_WIDTH, VP_HEIGHT), title);
			gWindow->setFramerateLimit(30);
			gWindow->requestFocus();
		}

		uint64_t GetRenderTime()
		{
			return (gElapsedTime > INTERPOLATION_TIME_MS) ? gElapsedTime - INTERPOLATION_TIME_MS : 0;
		}

		void DeleteOldSnapshots(uint64_t renderTime)
		{
			for (auto it = gSnapshots.rbegin(); it != gSnapshots.rend(); ++it)
			{
				// If this snapshot came before our render time
				if (it->clientTime <= renderTime)
				{
					// Start deleting from this point
					gSnapshots.erase(gSnapshots.begin(), (++it).base());
					break;
				}
			}
		}

		// SEND FUNCTIONS //////////////////////////////////

		DEF_CLIENT_SEND(PACKET_CLIENT_JOIN)
		{
			auto p = InitPacket(PACKET_CLIENT_JOIN);
			// Random hex colours
			p << (gIsHost ? 0xA0FFA0FF : 0xFFA0A0FF);

			debug << "CLIENT: Sent join request to server" << std::endl;
			gSocket.send(p);
			gStatus = STATUS_JOINING;
		}

		DEF_SEND_PARAM(PACKET_CLIENT_CMD)(const Command& cmd)
		{
			auto p = InitPacket(PACKET_CLIENT_CMD);
			p << gMyID << cmd;

			gSocket.send(p);
		}

		DEF_CLIENT_SEND(PACKET_CLIENT_SHOOT)
		{
			auto p = InitPacket(PACKET_CLIENT_SHOOT);

			gSocket.send(p);
		}

		// RECEIVE FUNCTIONS ///////////////////////////////
		// If any of the receive functions return false,
		// the client will disconnect.

		DEF_CLIENT_RECV(PACKET_SERVER_WELCOME)
		{
			float viewRotation;
			p >> gMyID >> viewRotation;

			debug << "CLIENT: Server assigned us id #" << (int) gMyID << std::endl;

			char title[32];
			sprintf_s(title, "Client #%d", gMyID);
			InitializeWindow(title);

			if (viewRotation != 0.f)
			{
				sf::View view = gWindow->getView();
				view.setRotation(viewRotation);
				gWindow->setView(view);

				gViewInverted = true;
			}

			gStatus = STATUS_PLAYING;
			return true;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_SPECTATOR)
		{
			debug << "CLIENT: No more available player slots; we are a spectator" << std::endl;

			InitializeWindow("Spectating");

			gStatus = STATUS_SPECTATING;
			return true;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_FULL)
		{
			debug << "CLIENT: Could not join server because it is full" << std::endl;

			return false;
		}

		DEF_CLIENT_RECV(PACKET_SERVER_UPDATE)
		{
			// We don't care about state updates
			// if we are still waiting to learn if
			// we can join the server
			if (gStatus == STATUS_JOINING)
				return true;

			WorldSnapshot snapshot;
			p >> snapshot;

			// Overwrite last snapshot if we receive two snapshots in the same frame
			if (gSnapshots.empty() || gSnapshots.back().clientTime != gElapsedTime)
				gSnapshots.push_back(snapshot);

			gSnapshots.back().clientTime = gElapsedTime;

			// Delete old snapshots
			uint64_t renderTime = GetRenderTime();

			// Delete old (irrelevant) snapshots
			DeleteOldSnapshots(renderTime);

			// Update world
			gWorld = gSnapshots.back().snapshot;

			// Reconciliation
			if (gIsReconciling && !gCommands.empty())
			{
				Player* me = gWorld.GetPlayer(gMyID);
				if (me)
				{
					// Last commandID on server
					uint32_t lastCommandID = me->GetLastCommandID();

					// Remove older commands
					const auto pred = [lastCommandID](const auto& cmd)
					{
						return cmd.id <= lastCommandID;
					};

					auto it = std::remove_if(gCommands.begin(), gCommands.end(), pred);
					gCommands.erase(it, gCommands.end());

					// Reapply commands the server had not yet processed
					for (const auto& cmd : gCommands)
						gWorld.RunCommand(cmd, gMyID, true);
				}
			}

			return true;
		}

		using ClientReceiveCallback = std::function<bool(sf::Packet&)>;
		const ClientReceiveCallback gReceivePacket[] = {
			nullptr,						// PACKET_CLIENT_JOIN
			RECV(PACKET_SERVER_WELCOME),
			RECV(PACKET_SERVER_SPECTATOR),
			RECV(PACKET_SERVER_FULL),
			nullptr,						// PACKET_CLIENT_CMD
			RECV(PACKET_SERVER_UPDATE),
			nullptr,						// PACKET_CLIENT_SHOOT
		};

		bool ConnectToServer(const sf::IpAddress& address, Port port)
		{
			debug << "CLIENT: Connecting to " << address.toString() << ':' << port << std::endl;
			if (gSocket.connect(address, port) != Status::Done)
				return false;

			gSocket.setBlocking(false);

			SEND(PACKET_CLIENT_JOIN)();
			return true;
		}

		void Disconnect()
		{
			// TODO: Notify server
			gSocket.disconnect();
		}

		bool ReceiveFromServer()
		{
			while (true)
			{
				sf::Packet p;
				auto ret = gSocket.receive(p);

				switch (ret)
				{
					case Status::Disconnected:
						debug << "CLIENT: Lost connection to server" << std::endl;
					case Status::Error:
					case Status::Partial:
						return false;
					case Status::NotReady:
						return true;
				}

				uint8_t type;
				p >> type;

				if (type < PACKET_END && gReceivePacket[type] != nullptr)
				{
					if (!gReceivePacket[type](p))
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
			while (gWindow->pollEvent(event))
			{
				switch (event.type)
				{
					case sf::Event::Closed:
						return false;
					case sf::Event::KeyPressed:
						switch (event.key.code)
						{
							case Key::Escape:
								return false;

								// RECODE: Messy and repetitive
								// Control networking strategies
							case Key::F1:
							{
								gIsPredicting = !gIsPredicting;
								debug << "Prediction: " << std::boolalpha << gIsPredicting << std::endl;

								if (gIsReconciling)
								{
									gIsReconciling = false;
									debug << "Reconciliation: " << std::boolalpha << gIsReconciling << std::endl;
								}
							}
							break;
							case Key::F2:
							{
								gIsReconciling = !gIsReconciling;

								if (gIsReconciling && !gIsPredicting)
								{
									gIsPredicting = true;
									debug << "Prediction: " << std::boolalpha << gIsPredicting << std::endl;
								}
								debug << "Reconciliation: " << std::boolalpha << gIsReconciling << std::endl;
							}
							break;
							case Key::F3:
							{
								gIsInterpolating = !gIsInterpolating;
								debug << "Interpolation: " << std::boolalpha << gIsInterpolating << std::endl;

								if (gIsInterpolatingBullets)
								{
									gIsInterpolatingBullets = false;
									debug << "Interpolate bullets (broken): " << std::boolalpha << gIsInterpolatingBullets << std::endl;
								}
							}
							break;
							case Key::F4:
							{
								gIsInterpolatingBullets = !gIsInterpolatingBullets;
								debug << "Interpolate bullets (broken): " << std::boolalpha << gIsInterpolatingBullets << std::endl;

								if (!gIsInterpolating && gIsInterpolatingBullets)
								{
									gIsInterpolating = true;
									debug << "Interpolation: " << std::boolalpha << gIsInterpolating << std::endl;
								}
							}
							break;
							case Key::Space:
								SEND(PACKET_CLIENT_SHOOT)();
								break;
						}
						break;
				}
			}

			return true;
		}

		void BuildCommand(uint64_t dt)
		{
			static uint32_t commandID = 0;

			if (gWindow->hasFocus())
			{
				Command cmd;
				cmd.id = commandID++;
				cmd.dt = dt;

				Command::Direction direction = Command::IDLE;

				if (sf::Keyboard::isKeyPressed(Key::A))
					direction = (gViewInverted) ? Command::RIGHT : Command::LEFT;
				if (sf::Keyboard::isKeyPressed(Key::D))
					direction = (gViewInverted) ? Command::LEFT : Command::RIGHT;

				if (direction != Command::IDLE)
				{
					cmd.direction = direction;

					// Client-side prediction
					if (gIsPredicting)
					{
						gCommands.push_back(cmd);

						gWorld.RunCommand(cmd, gMyID, false);
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
			for (auto& snapshot : gSnapshots)
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
					if (playerFrom.GetID() == playerTo.GetID() && playerFrom.GetID() != gMyID)
					{
						auto playerReal = gWorld.GetPlayer(playerFrom.GetID());
						if (playerReal)
						{
							sf::Vector2f newPos = (playerTo.GetPosition() - playerFrom.GetPosition()) * alpha + playerFrom.GetPosition();
							playerReal->SetPosition(newPos);
						}
					}
				}
			}

			// NOTE: Broken + repetitive
			if (gIsInterpolatingBullets)
			{
				for (const auto& bulletFrom : from.snapshot.GetBullets())
				{
					for (const auto& bulletTo : to.snapshot.GetBullets())
					{
						if (bulletFrom.GetID() == bulletTo.GetID())
						{
							auto bulletReal = gWorld.GetBullet(bulletFrom.GetID());
							if (bulletReal)
							{
								sf::Vector2f newPos = (bulletTo.GetPosition() - bulletFrom.GetPosition()) * alpha + bulletFrom.GetPosition();
								bulletReal->SetPosition(newPos);
							}
						}
					}
				}
			}
		}

		void ClientLoop()
		{
			gIsRunning = true;

			uint64_t dt = 0;
			sf::Clock deltaClock;

			// Main loop
			while (gIsRunning)
			{
				// Networking
				if (!WaitForSelector())
					break;

				switch (gStatus)
				{
					case STATUS_JOINING:
						continue;
					case STATUS_PLAYING:
						// Movement input
						BuildCommand(dt);
					case STATUS_SPECTATING:
						// Debug and 'meta' input
						if (!HandleEvents())
							gIsRunning = false;
				}

				// Interpolation
				if (gIsInterpolating)
				{
					uint64_t renderTime = GetRenderTime();

					// Get the two snapshots between which the.GetPosition @ renderTime exists
					auto snapshots = GetRelevantSnapshots(renderTime);
					WorldSnapshot* to = std::get<0>(snapshots);
					WorldSnapshot* from = std::get<1>(snapshots);

					// If we have enough data
					if (to && from)
						Interpolate(*from, *to, renderTime);
				}

				// Render
				gWindow->clear();
				World::RenderWorld(gWorld, *gWindow);
				gWindow->display();

				// Timing
				dt = deltaClock.restart().asMilliseconds();
				gElapsedTime += dt;
			}

			debug << "CLIENT: Closing..." << std::endl;
			gIsRunning = false;
		}

		bool StartClient(const sf::IpAddress& address, Port port, bool isHost)
		{
			gIsHost = isHost;

			if (!ConnectToServer(address, port))
			{
				debug << "CLIENT: Failed to connect to server" << std::endl;
				return false;
			}

			ClientLoop();

			Disconnect();
			return true;
		}
	}
}