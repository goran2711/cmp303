#pragma once
#include <chrono>
#include <SFML/Window.hpp>

// common.h: Commonly used variables and shorthands

constexpr unsigned int VP_WIDTH = 426;
constexpr unsigned int VP_HEIGHT = 240;

constexpr float H_VP_WIDTH = VP_WIDTH * 0.5f;
constexpr float H_VP_HEIGHT = VP_HEIGHT * 0.5f;

constexpr float PADDLE_W = 80.f;
constexpr float PADDLE_H = 12.f;

constexpr float H_PADDLE_W = PADDLE_W * 0.5f;
constexpr float H_PADDLE_H = PADDLE_H * 0.5f;

constexpr float BULLET_W = 8.f;
constexpr float BULLET_H = 24.f;

constexpr float H_BULLET_W = BULLET_W * 0.5f;
constexpr float H_BULLET_H = BULLET_H * 0.5f;

// SFML Shortcuts
using Key = sf::Keyboard::Key;

// Chrono
using the_clock = std::chrono::high_resolution_clock;
using time_point = std::chrono::time_point<the_clock>;
using ms = std::chrono::milliseconds;

#define to_ms(s, e) std::chrono::duration_cast<ms>(e - s)
