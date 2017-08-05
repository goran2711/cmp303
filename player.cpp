#include "player.h"
#include "command.h"
#include "common.h"

void Player::RunCommand(const Command& cmd)
{
	constexpr uint32_t KEY_A = 1 << Key::A;
	constexpr uint32_t KEY_D = 1 << Key::D;

	mLastCommandID = cmd.id;

	// Not pressing any buttons
	if (cmd.direction == Command::IDLE)
		return;
	else
	{
		float distance = MOVE_SPEED * cmd.dt / 1000.f;

		mPosition.x += (cmd.direction == Command::LEFT) ? -distance : distance;
	}
}
