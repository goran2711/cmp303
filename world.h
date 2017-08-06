#pragma once
#include "player.h"
#include "bullet.h"
#include "common.h"
#include <vector>
#include <SFML/Graphics.hpp>

class World
{
public:
	friend sf::Packet& operator<<(sf::Packet& p, const World& world);
	friend sf::Packet& operator >> (sf::Packet& p, World& world);

	constexpr static int MAX_PLAYERS = 2;

	static constexpr float LANE_BOTTOM = 568.f;
	static constexpr float LANE_TOP = 32.f;

	static void RenderWorld(const World& world, sf::RenderWindow& window);

	bool AddPlayer(Player& player);
	bool RemovePlayer(uint8_t id);

	void RunCommand(const Command& cmd, uint8_t id, bool rec);
	void PlayerShoot(uint8_t id);

	void Update(float dt);

	Bullet* GetBullet(uint32_t id);

	bool IsPlayerTopLane(uint8_t id);
	Player* GetPlayer(uint8_t id);
	bool PlayerExists(uint8_t id);

	const std::vector<Bullet>& GetBullets() const { return mBullets; }
	const std::vector<Player>& GetPlayers() const { return mPlayers; }

private:
	static constexpr float SPAWN_POS = { H_VP_WIDTH };

	uint8_t GeneratePlayerID();
	bool IsLaneOccupied(int lane) const;

	// Player ID
	uint8_t mNewPID = 1;
	// Entity ID (bullets)
	uint32_t mNewEID = 1;

	std::vector<Player> mPlayers;
	std::vector<Bullet> mBullets;
};

sf::Packet& operator<<(sf::Packet& p, const World& world);
sf::Packet& operator >> (sf::Packet& p, World& world);

struct WorldSnapshot
{
	World snapshot;
	uint64_t serverTime = 0;
	uint64_t clientTime = 0;
};

sf::Packet& operator<<(sf::Packet& p, const WorldSnapshot& snapshot);
sf::Packet& operator >> (sf::Packet& p, WorldSnapshot& snapshot);