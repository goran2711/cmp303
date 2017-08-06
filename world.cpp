#include "world.h"
#include "common.h"
#include <algorithm>

#define INVALID_ID	0
#define FIRST_ID	1

/* static */ const sf::Vector2f World::SPAWN_POSITIONS[2] = {
		{ 400.f, 32.f  },
		{ 400.f, 568.f }
};

/* static */ void World::RenderWorld(const World & world, sf::RenderWindow & window)
{
	sf::RectangleShape shape({ PADDLE_W, PADDLE_H });
	shape.setOrigin({ H_PADDLE_W, H_PADDLE_H });

	for (const auto& player : world.mPlayers)
	{
		shape.setFillColor(sf::Color(player.colour()));
		shape.setPosition(player.position());
		window.draw(shape);
	}
}

bool World::AddPlayer(Player& player)
{
	if (mPlayers.size() >= MAX_PLAYERS)
		return false;

	uint8_t newPID = GeneratePlayerID();

	if (!newPID)
		return false;

	player.SetPID(newPID);

	mPlayers.push_back(player);

	mPlayers.back().SetPosition(SPAWN_POSITIONS[mPlayers.size() % 2]);

	return true;
}

void World::RunCommand(const Command& cmd, uint8_t pid, bool rec)
{
	Player* player = GetPlayer(pid);

	if (player)
	{
		player->RunCommand(cmd, rec);

		if (player->position().x - H_PADDLE_W < 0.f)
			player->SetX(H_PADDLE_W);
		if (player->position().x + H_PADDLE_W > VP_WIDTH_F)
			player->SetX(VP_WIDTH_F - H_PADDLE_W);
	}
}

bool World::RemovePlayer(uint8_t pid)
{
	auto hasPID = [pid](const auto& player) { return player.pid() == pid; };

	auto it = mPlayers.erase(std::remove_if(mPlayers.begin(), mPlayers.end(), hasPID));
	return it != mPlayers.end();
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

bool World::PlayerExists(uint8_t pid)
{
	return GetPlayer(pid) != nullptr;
}

uint8_t World::GeneratePlayerID()
{
	return mNewID++;
}
