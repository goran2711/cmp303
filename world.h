#pragma once
#include "player.h"
#include "bullet.h"
#include "common.h"
#include <vector>
#include <SFML/Graphics.hpp>

// world.h: Represents a game simulation. Holds the position of all the entities in the game
//			Also contains movement constraints

class World
{
public:
	friend sf::Packet& operator<<(sf::Packet& p, const World& world);
	friend sf::Packet& operator >> (sf::Packet& p, World& world);

	static constexpr int MAX_PLAYERS = 2;

	static constexpr float LANE_BOTTOM = 228.f;
	static constexpr float LANE_TOP = 12.f;

	static const sf::Vector2f INVALID_POS;

	static void RenderWorld(const World& world, sf::RenderWindow& window, bool showServerBullets = false);

	void AddBullet(const Bullet& bullet);

	bool AddPlayer(Player& player);
	bool RemovePlayer(sf::Uint8 id);

	void UpdateWorld(const World& other);

	void RunCommand(const Command& cmd, sf::Uint8 id, bool rec);
	Bullet PlayerShoot(sf::Uint8 id, sf::Vector2f playerPos = INVALID_POS);

	void Update(sf::Uint64 dt);

	Bullet* GetBullet(sf::Uint32 id);

	bool IsPlayerTopLane(sf::Uint8 id);
	Player* GetPlayer(sf::Uint8 id);
	bool PlayerExists(sf::Uint8 id);

	const std::vector<Bullet>& GetBullets() const { return mBullets; }
	const std::vector<Player>& GetPlayers() const { return mPlayers; }

private:
	static constexpr float SPAWN_POS_X = H_VP_WIDTH;

	sf::Uint8 GeneratePlayerID();
	bool IsLaneOccupied(int lane) const;

	// Player ID
	sf::Uint8 mNewPID = 1;
	// Entity ID (bullets)
	sf::Uint32 mNewEID = 1;

	std::vector<Player> mPlayers;
	std::vector<Bullet> mBullets;
	std::vector<Bullet> mServerBullets;
};

sf::Packet& operator<<(sf::Packet& p, const World& world);
sf::Packet& operator >> (sf::Packet& p, World& world);

struct WorldSnapshot
{
	World snapshot;
	sf::Uint64 serverTime = 0;
	sf::Uint64 clientTime = 0;
};

sf::Packet& operator<<(sf::Packet& p, const WorldSnapshot& snapshot);
sf::Packet& operator >> (sf::Packet& p, WorldSnapshot& snapshot);
