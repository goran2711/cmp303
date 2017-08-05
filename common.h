#pragma once
#include <list>
#include <memory>
#include <algorithm>
#include <chrono>
#include <SFML/Window.hpp>

// SFML Shortcuts
using Key = sf::Keyboard::Key;

// Chrono
using the_clock = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<the_clock>;
using ms = std::chrono::milliseconds;

#define now()	the_clock::now()

// Util
#define clamp(a, lo, hi)		(a > hi) ? hi : (a < lo) ? lo : a

namespace
{
}