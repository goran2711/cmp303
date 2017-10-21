#include "player.h"
#include "command.h"
#include "common.h"
#include "network.h"

void Player::RunCommand(const Command& cmd, bool rec)
{
	constexpr sf::Uint32 KEY_A = 1 << Key::A;
	constexpr sf::Uint32 KEY_D = 1 << Key::D;

	mLastCommandID = cmd.id;

	// Not pressing any buttons
	if (cmd.direction == Command::IDLE)
		return;
	else
	{
		if (rec)
			int x = 0;

		float distance = MOVE_SPEED * cmd.dt / 1000.f;

		mPosition.x += (cmd.direction == Command::LEFT) ? -distance : distance;
	}
}

sf::Packet & operator<<(sf::Packet & p, const Player & player)
{
	p << player.mPID << player.mLastCommandID << player.mColour << player.mPosition;
	return p;
}

sf::Packet& operator >> (sf::Packet& p, Player& player)
{
	p >> player.mPID >> player.mLastCommandID >> player.mColour >> player.mPosition;
	return p;
}

