#pragma once
#include <cstdint>

struct Command
{
	uint32_t id;
	// Keyboard state
	enum Direction
	{
		IDLE, LEFT, RIGHT
	} direction;
	uint64_t dt;
};