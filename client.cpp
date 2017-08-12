#include "client.h"
#include <list>
#include <functional>
#include <SFML/Graphics.hpp>
#include <iomanip>
#include "world.h"
#include "command.h"
#include "common.h"
#include "debug.h"

namespace Network
{
		namespace Client
		{
				// Global variables ///////
				constexpr int INTERPOLATION_TIME_MS = 150;

				// Networking
				Connection gConnection;
				bool gIsRunning;

				bool gIsPredicting = true;
				bool gIsReconciling = true;
				bool gIsInterpolating = true;
				bool gShowServerBullets = false;

				// Graphics
				std::unique_ptr<sf::RenderWindow> gWindow;

				// Game
				ms gElapsedTime;

				uint8_t gMyID;
				World gWorld;

				std::list<Command> gCommands;
				std::list<WorldSnapshot> gSnapshots;

				bool gViewInverted = false;

				struct FutureBullet
				{
						sf::Uint64 serverTime;
						Bullet bullet;
				};

				// Cache of bullets the server has told us about, but we are not
				// ready to show yet because of interpolation
				std::vector<FutureBullet> gIncomingBullets;

				// Forward declarations
				void InitializeWindow(const char* title);
				sf::Uint64 GetRenderTime();
				void DeleteOldSnapshots(sf::Uint64 renderTime);

				// SEND FUNCTIONS //////////////////////////////////

				DEF_CLIENT_SEND(PACKET_CLIENT_JOIN)
				{
						//// Request to join a server

						if (gConnection.status != STATUS_NONE)
								return;

						auto p = InitPacket(PACKET_CLIENT_JOIN);

						debug << "CLIENT: Sent join request to server" << std::endl;
						gConnection.Send(p);
						gConnection.status = STATUS_JOINING;
				}

				DEF_SEND_PARAM(PACKET_CLIENT_CMD)(const Command& cmd)
				{
						//// Send a movement command to the server
						// cmd: Movement command

						if (gConnection.status != STATUS_PLAYING)
								return;

						auto p = InitPacket(PACKET_CLIENT_CMD);
						p << cmd;

						gConnection.Send(p);
				}

				DEF_SEND_PARAM(PACKET_CLIENT_PING)(sf::Uint64 serverTime)
				{
						//// Respond to the server's ping request
						// serverTime: The server's timestamp
						// gElapsedTime.count(): The client's timestamp (server will be responding to us as well)

						if (gConnection.status != STATUS_PLAYING && gConnection.status != STATUS_SPECTATING)
								return;

						auto p = InitPacket(PACKET_CLIENT_PING);
						p << sf::Uint64(serverTime) << sf::Uint64(gElapsedTime.count());

						gConnection.Send(p);
				}

				DEF_CLIENT_SEND(PACKET_CLIENT_SHOOT)
				{
						//// Request to shoot a bullet

						// A spectator can trigger this, so we make sure we are actually a player
						if (gConnection.status != STATUS_PLAYING)
								return;

						auto p = InitPacket(PACKET_CLIENT_SHOOT);

						gWorld.PlayerShoot(gMyID);
						gConnection.Send(p);
				}

				// RECEIVE FUNCTIONS ///////////////////////////////
				// If any of the receive functions return false,
				// the client will disconnect.

				DEF_CLIENT_RECV(PACKET_SERVER_WELCOME)
				{
						//// The server has accepted us as a player
						// gMyID: The ID we were assigned by the server
						// viewRotation: How many degrees we should rotate our view (player should always see themself on the bottom of the screen)

						if (gConnection.status != STATUS_JOINING)
								return true;

						float viewRotation;
						p >> gMyID >> viewRotation;

						debug << "CLIENT: Server assigned us id #" << (int) gMyID << std::endl;

						char title[32];
						sprintf(title, "Client #%d", gMyID);
						InitializeWindow(title);

						// Rotate our view so that we appear on the bottom
						if (viewRotation != 0.f)
						{
								sf::View view = gWindow->getView();
								view.setRotation(viewRotation);
								gWindow->setView(view);

								gViewInverted = true;
						}

						gConnection.status = STATUS_PLAYING;
						return true;
				}

				DEF_CLIENT_RECV(PACKET_SERVER_SPECTATOR)
				{
						//// The server has accpted us as a spectator, we will not be able to influence the game

						if (gConnection.status != STATUS_JOINING)
								return true;

						debug << "CLIENT: No more available player slots; we are a spectator" << std::endl;

						InitializeWindow("Spectating");

						gConnection.status = STATUS_SPECTATING;
						return true;
				}

				DEF_CLIENT_RECV(PACKET_SERVER_FULL)
				{
						//// The server is full, and we will be disconnected

						if (gConnection.status != STATUS_JOINING)
								return true;

						debug << "CLIENT: Could not join server because it is full" << std::endl;

						return false;
				}

				DEF_CLIENT_RECV(PACKET_SERVER_PING)
				{
						//// The server has requested or responded to a ping request
						// pingBack: if true, we will return the server's timestamp, along with our own
						//			 if false, the timestamp is our own, and we will calculate the latency
						// timestamp: either the server's own gElapsedTime, or the client's gElapsedTime being sent back

						if (gConnection.status != STATUS_PLAYING && gConnection.status != STATUS_SPECTATING)
								return true;

						bool pingBack = false;
						sf::Uint64 timestamp;
						p >> pingBack >> timestamp;

						// We have been asked to ping back, so timestamp is the server's timestamp
						if (pingBack)
								SEND(PACKET_CLIENT_PING)(timestamp);
						// Calculate latency
						else
								gConnection.latency = gElapsedTime - ms(timestamp);

						return true;
				}

				DEF_CLIENT_RECV(PACKET_SERVER_UPDATE)
				{
						//// A state update from the server
						// snapshot: The server's World object, along with timestamps

						// We don't care about state updates
						// if we are still waiting to learn if
						// we can join the server
						if (gConnection.status == STATUS_JOINING)
								return true;

						WorldSnapshot snapshot;
						p >> snapshot;

						// Only create a new snapshot if there is not one there already, or we did not
						// receive multiple snapshots in the same frame
						if (gSnapshots.empty() || gSnapshots.back().clientTime != gElapsedTime.count())
								gSnapshots.push_back(snapshot);

						gSnapshots.back().clientTime = gElapsedTime.count();

						// Delete old (irrelevant) snapshots
						sf::Uint64 renderTime = GetRenderTime();
						DeleteOldSnapshots(renderTime);

						// Set our simulation to be the same as the ser
						gWorld.UpdateWorld(gSnapshots.back().snapshot);

						// Reconciliation
						if (gIsReconciling && !gCommands.empty())
						{
								Player* me = gWorld.GetPlayer(gMyID);
								if (!me)
										return false;

								// ID of the last command the server processed
								uint32_t lastCommandID = me->GetLastCommandID();

								// Remove older commands
								const auto pred = [lastCommandID](const auto& cmd)
								{
										return cmd.id <= lastCommandID;
								};

								gCommands.erase(
												std::remove_if(gCommands.begin(), gCommands.end(), pred),
												gCommands.end()
											   );

								// Reapply commands the server had not yet processed
								for (const auto& cmd : gCommands)
										gWorld.RunCommand(cmd, gMyID, true);
						}

						return true;
				}

				DEF_CLIENT_RECV(PACKET_SERVER_SHOOT)
				{
						//// Another client has fired a bullet
						// newBullet: The bullet object
						// serverTime: The timestamp the bullet was spawned on the server

						debug << "CLIENT: Received SERVER_SHOOT" << std::endl;

						if (gConnection.status != STATUS_PLAYING && gConnection.status != STATUS_SPECTATING)
								return true;

						Bullet newBullet;
						sf::Uint64 serverTime;
						p >> newBullet >> serverTime;

						// Store the bullet along with its timestamp
						// so that we can spawn it when we've interpolated that far
						FutureBullet fBullet;
						fBullet.serverTime = serverTime;
						fBullet.bullet = newBullet;

						gIncomingBullets.push_back(fBullet);

						return true;
				}

				using ClientReceiveCallback = std::function<bool(sf::Packet&)>;
				const ClientReceiveCallback gReceivePacket[] = {
						nullptr,						// PACKET_CLIENT_JOIN
						RECV(PACKET_SERVER_WELCOME),
						RECV(PACKET_SERVER_SPECTATOR),
						RECV(PACKET_SERVER_FULL),
						nullptr,						// PACKET_CLIENT_CMD
						RECV(PACKET_SERVER_PING),
						nullptr,						// PACKET_CLIENT_PING
						RECV(PACKET_SERVER_UPDATE),
						nullptr,						// PACKET_CLIENT_SHOOT
						RECV(PACKET_SERVER_SHOOT),
				};

				// Client logic ///////

				void PrintOptions()
				{
						using std::boolalpha; using std::setw; using std::left;
						constexpr int FILL_W = 32;

						debug << boolalpha << '\n' <<
								setw(FILL_W) << left << "Predicting: " << gIsPredicting << '\n' <<
								setw(FILL_W) << left << "Reconciliating: " << gIsReconciling << '\n' <<
								setw(FILL_W) << left << "Interpolating: " << gIsInterpolating << '\n' <<
								setw(FILL_W) << left << "Showing server bullets: " << gShowServerBullets << std::endl;
				}

				void InitializeWindow(const char* title)
				{
						debug << "CLIENT: Initialising SFML window..." << std::endl;
						gWindow = std::make_unique<sf::RenderWindow>(sf::VideoMode(VP_WIDTH, VP_HEIGHT), title);
						gWindow->setFramerateLimit(30);

						// NOTE: This never works ...
						gWindow->requestFocus();

						PrintOptions();
				}

				// The time in the past we are showing
				sf::Uint64 GetRenderTime()
				{
						return (gElapsedTime.count() > INTERPOLATION_TIME_MS) ? gElapsedTime.count() - INTERPOLATION_TIME_MS : 0;
				}

				void DeleteOldSnapshots(sf::Uint64 renderTime)
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

				bool ConnectToServer(const sf::IpAddress& address, Port port)
				{
						debug << "CLIENT: Connecting to " << address.toString() << ':' << port << std::endl;
						if (!gConnection.Connect(address, port))
								return false;

						gConnection.SetBlocking(false);

						SEND(PACKET_CLIENT_JOIN)();
						return true;
				}

				void Disconnect()
				{
						// TODO: Notify server that we disconnected intentionally

						gConnection.Disconnect();
				}

				bool ReceiveFromServer()
				{
						while (true)
						{
								sf::Packet p;
								if (!gConnection.Receive(p))
										break;

								uint8_t type;
								p >> type;

								// Call the appropriate receive function based on the packet-type
								if (type < PACKET_END && gReceivePacket[type] != nullptr)
								{
										if (!gReceivePacket[type](p))
												return false;
								}
						}

						if (!gConnection.active)
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
												{
														bool changedOptions = false;

														switch (event.key.code)
														{
																case Key::Escape:
																		return false;

																		// Control networking strategies
																case Key::F1:
																		{
																				gIsPredicting = !gIsPredicting;

																				if (gIsReconciling)
																						gIsReconciling = false;

																				changedOptions = true;
																		}
																		break;
																case Key::F2:
																		{
																				gIsReconciling = !gIsReconciling;

																				if (gIsReconciling && !gIsPredicting)
																						gIsPredicting = true;

																				changedOptions = true;
																		}
																		break;
																case Key::F3:
																		{
																				gIsInterpolating = !gIsInterpolating;

																				changedOptions = true;
																		}
																		break;
																case Key::F4:
																		{
																				gShowServerBullets = !gShowServerBullets;

																				changedOptions = true;
																		}
																		break;
																case Key::Space:
																		SEND(PACKET_CLIENT_SHOOT)();
																		break;
														}

														if (changedOptions)
																PrintOptions();
												}
												break;
								}
						}

						return true;
				}

				// Create a movement command to send to the server
				// and possibly run locally (prediction)
				void BuildCommand(ms dt)
				{
						static uint32_t commandID = 0;

						if (gWindow->hasFocus())
						{
								Command cmd;
								cmd.id = commandID++;
								cmd.dt = dt.count();

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

				// Get two snapshots from which we can interpolate positions at t == renderTime
				auto GetRelevantSnapshots(sf::Uint64 renderTime)
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

						return std::make_pair(to, from);
				}

				// Linearly interpolate the other player's position based on our delay
				void Interpolate(const WorldSnapshot& from, const WorldSnapshot& to, sf::Uint64 renderTime)
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
				}

				void ClientLoop()
				{
						gIsRunning = true;

						ms dt{ 0 };
						// Main loop
						while (gIsRunning)
						{
								auto startFrame = the_clock::now();

								// Networking
								if (!ReceiveFromServer())
										break;

								switch (gConnection.status)
								{
										// When joining, we are just waiting to receive PACKET_SERVER_{WELCOME,SPECTATOR,FULL}
										case STATUS_JOINING:
												continue;

												// When playing, we want to handle user input
										case STATUS_PLAYING:
												// Movement input
												BuildCommand(dt);

												// When spectating, we want to be able toggle options on and off
										case STATUS_SPECTATING:
												// Debug and 'meta' input
												if (!HandleEvents())
														gIsRunning = false;
								}

								// Bullet prediction
								gWorld.Update(dt.count());

								sf::Uint64 renderTime = GetRenderTime();

								// Get the two snapshots between which the position @ renderTime exists
								auto snapshots = GetRelevantSnapshots(renderTime);
								WorldSnapshot* to = snapshots.first;
								WorldSnapshot* from = snapshots.second;
								if (from)
								{
										// Check if we have passed a snapshot where a new bullet was fired
										for (auto it = gIncomingBullets.begin(); it != gIncomingBullets.end(); )
										{
												if (from->serverTime >= it->serverTime)
												{
														gWorld.AddBullet(it->bullet);
														it = gIncomingBullets.erase(it);
														debug << "Incoming bullets: " << gIncomingBullets.size() << std::endl;
												}
												else
														++it;
										}

										// Interpolation
										if (gIsInterpolating && to)
												Interpolate(*from, *to, renderTime);
								}

								// Render
								gWindow->clear();
								World::RenderWorld(gWorld, *gWindow, gShowServerBullets);
								gWindow->display();

								auto endFrame = the_clock::now();

								// Timing
								dt = std::chrono::duration_cast<ms>(endFrame - startFrame);
								gElapsedTime += dt;
						}

						debug << "CLIENT: Closing..." << std::endl;
						gIsRunning = false;
				}

				bool StartClient(const sf::IpAddress& address, Port port)
				{
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
