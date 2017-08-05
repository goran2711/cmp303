#pragma once
#include <list>
#include <memory>
#include <algorithm>
#include <chrono>
#include "entity.h"

// Chrono
using the_clock = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<the_clock>;
using ms = std::chrono::milliseconds;

#define now()	the_clock::now()

// Game
using EntityContainer = std::list<std::unique_ptr<Entity>>;

// Util
#define clamp(a, lo, hi)		(a > hi) ? hi : (a < lo) ? lo : a

namespace
{
	// RECODE: Shouldn't be template.. probably
	auto GetEntityByUID(const EntityContainer& l, uint8_t uid)
	{
		return std::find_if(l.begin(), l.end(), [&](const auto& e) { return e->uid == uid; });
	}
}