#pragma once
#include <list>
#include <memory>
#include <algorithm>
#include <chrono>
#include <SFML/Window.hpp>

#define VP_WIDTH	800
#define VP_HEIGHT	600

#define VP_WIDTH_F	800.f
#define VP_HEIGHT_F	600.f

#define H_VP_WIDTH	VP_WIDTH * 0.5f
#define H_VP_HEIGHT	VP_HEIGHT * 0.5f

#define PADDLE_W	128.f
#define PADDLE_H	16.f

#define H_PADDLE_W	PADDLE_W * 0.5f
#define H_PADDLE_H	PADDLE_H * 0.5f

// SFML Shortcuts
using Key = sf::Keyboard::Key;

// Chrono
using the_clock = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<the_clock>;
using ms = std::chrono::milliseconds;

#define now()	the_clock::now()

// Util
#define clamp(a, lo, hi)		(a > hi) ? hi : (a < lo) ? lo : a
