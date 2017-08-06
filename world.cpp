#include "world.h"
#include <algorithm>

#define INVALID_ID	0
#define FIRST_ID	1

/* static */ const sf::Vector2f World::SPAWN_POSITIONS[2] = {
		{ 400.f, 32.f  },
		{ 400.f, 568.f }
};

/* static */ void World::RenderWorld(const World & world, sf::RenderWindow & window)
{
	sf::RectangleShape shape({ 128.f, 16.f });

	for (const auto& player : world.mPlayers)
	{
		shape.setFillColor(sf::Color(player.colour()));
		shape.setPosition(player.position());
		window.draw(shape);
	}
}

bool World::AddPlayer(Player& player)
{
	if (mPlayers.size() >= 2)
		return false;

	uint8_t newPID = GeneratePlayerID();

	if (!newPID)
		return false;

	player.SetPID(newPID);

	mPlayers.push_back(player);

	mPlayers.back().SetPosition(SPAWN_POSITIONS[mPlayers.size() % 2]);

	return true;
}

void World::RunCommand(const Command& cmd, uint8_t pid)
{
	Player* player = GetPlayer(pid);

	if (player)
		player->RunCommand(cmd);
}

bool World::RemovePlayer(uint8_t pid)
{
	//auto hasPID = [pid](const auto& player) { return player.pid() == pid; };

	//auto it = mPlayers.erase(std::remove_if(mPlayers.begin(), mPlayers.end(), hasPID));
	//return it != mPlayers.end();

	for (auto it = mPlayers.begin(); it != mPlayers.end(); ++it)
	{
		if (it->pid() == pid)
		{
			mPlayers.erase(it);
			return true;
		}
	}
	return false;
}

Player* World::GetPlayer(uint8_t pid)
{
	for (auto& player : mPlayers)
	{
		if (player.pid() == pid)
			return &player;
	}

	return nullptr;
}

uint8_t World::GeneratePlayerID()
{
	return mNewID++;
}
