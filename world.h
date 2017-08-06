#pragma once
#include "player.h"
#include <vector>
#include <SFML/Graphics.hpp>

class World
{
public:
	static void RenderWorld(const World& world, sf::RenderWindow& window);

	bool AddPlayer(Player& player);
	void RunCommand(const Command& cmd, uint8_t pid);

	bool RemovePlayer(uint8_t pid);

	Player* GetPlayer(uint8_t pid);

	std::vector<Player>& GetPlayers() { return mPlayers; }
	const std::vector<Player>& GetPlayers() const { return mPlayers; }

private:
	static const sf::Vector2f SPAWN_POSITIONS[2];

	uint8_t GeneratePlayerID();

	uint8_t mNewID = 1;
	std::vector<Player> mPlayers;
};

struct WorldSnapshot
{
	World snapshot;
	uint64_t serverTime = 0;
	uint64_t clientTime = 0;
};