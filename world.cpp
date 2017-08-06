#include "world.h"
#include "common.h"
#include "network.h"
#include "debug.h"
#include <algorithm>

/* static */ void World::RenderWorld(const World & world, sf::RenderWindow & window)
{
	sf::RectangleShape shape({ PADDLE_W, PADDLE_H });
	shape.setOrigin({ H_PADDLE_W, H_PADDLE_H });

	for (const auto& player : world.mPlayers)
	{
		shape.setFillColor(sf::Color(player.GetColour()));
		shape.setPosition(player.GetPosition());
		window.draw(shape);
	}

	// TEMP: Bullet size
	shape.setSize({ BULLET_W, BULLET_H });
	shape.setOrigin({ H_BULLET_W, H_BULLET_H });
	for (const auto& bullet : world.mBullets)
	{
		shape.setFillColor(sf::Color(bullet.GetColour()));
		shape.setPosition(bullet.GetPosition());
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

	float lane = LANE_TOP;
	if (IsLaneOccupied(lane))
		lane = LANE_BOTTOM;

	mPlayers.back().SetPosition({ SPAWN_POS, lane });

	return true;
}

bool World::RemovePlayer(uint8_t id)
{
	auto hasPID = [id](const auto& player) { return player.GetID() == id; };

	auto it = mPlayers.erase(std::remove_if(mPlayers.begin(), mPlayers.end(), hasPID));
	debug << "mPlayers.size() == " << mPlayers.size() << std::endl;
	return it != mPlayers.end();
}

void World::RunCommand(const Command& cmd, uint8_t id, bool rec)
{
	Player* player = GetPlayer(id);

	if (player)
	{
		player->RunCommand(cmd, rec);

		if (player->GetPosition().x - H_PADDLE_W < 0.f)
			player->SetX(H_PADDLE_W);
		if (player->GetPosition().x + H_PADDLE_W > VP_WIDTH)
			player->SetX(VP_WIDTH - H_PADDLE_W);
	}
}

void World::PlayerShoot(uint8_t id)
{
	Player* player = GetPlayer(id);
	if (!player)
		return;

	Bullet newBullet;
	newBullet.SetID(mNewEID++);
	newBullet.SetColour(player->GetColour());
	newBullet.SetPosition(player->GetPosition());

	// If this player is on top, fire down and vice versa
	float direction = IsPlayerTopLane(id) ? 1.f : -1.f;
	newBullet.SetDirection({ 0.f, direction });

	mBullets.push_back(newBullet);
}

void World::Update(float dt)
{
	for (auto it = mBullets.begin(); it != mBullets.end(); )
	{
		Bullet& bullet = *it;

		bullet.Update(dt);

		if (bullet.GetPosition().y + H_BULLET_H < 0.f)
			it = mBullets.erase(it);
		else if (bullet.GetPosition().y - H_BULLET_H >= VP_HEIGHT)
			it = mBullets.erase(it);
		else
			++it;
	}
}

Bullet* World::GetBullet(uint32_t id)
{
	for (auto& bullet : mBullets)
		if (bullet.GetID() == id)
			return &bullet;

	return nullptr;
}

bool World::IsPlayerTopLane(uint8_t id)
{
	Player* player = GetPlayer(id);
	if (!player)
		return false;

	return player->GetPosition().y == LANE_TOP;
}

Player* World::GetPlayer(uint8_t id)
{
	for (auto& player : mPlayers)
	{
		if (player.GetID() == id)
			return &player;
	}

	return nullptr;
}

bool World::PlayerExists(uint8_t id)
{
	return GetPlayer(id) != nullptr;
}

uint8_t World::GeneratePlayerID()
{
	return mNewPID++;
}

bool World::IsLaneOccupied(int lane) const
{
	for (const auto& player : mPlayers)
		if (player.GetPosition().y == lane)
			return true;

	return false;
}

sf::Packet& operator<<(sf::Packet& p, const World& world)
{
	p << world.mPlayers << world.mBullets;

	return p;
}

sf::Packet& operator >> (sf::Packet& p, World& world)
{
	p >> world.mPlayers >> world.mBullets;

	return p;
}

sf::Packet& operator<<(sf::Packet& p, const WorldSnapshot& snapshot)
{
	p << snapshot.snapshot << snapshot.serverTime << snapshot.clientTime;
	return p;
}

sf::Packet& operator >> (sf::Packet& p, WorldSnapshot& snapshot)
{
	p >> snapshot.snapshot >> snapshot.serverTime << snapshot.clientTime;
	return p;
}
