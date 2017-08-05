#include "world.h"
#include <algorithm>

#define INVALID_ID	0
#define FIRST_ID	1

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

	// TODO: Set position

	mPlayers.push_back(player);

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
