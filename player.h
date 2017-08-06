#pragma once
#include <SFML/System.hpp>
#include <cstdint>

struct Command;

class Player
{
public:
	static constexpr float MOVE_SPEED = 400.f;

	void RunCommand(const Command& cmd);

	void SetPID(uint8_t pid) { mPID = pid; }
	void SetColour(uint32_t RGBA) { mColour = RGBA; }
	void SetPosition(const sf::Vector2f& position) { mPosition = position; }
	void SetLastCommandID(int id) { mLastCommandID = id; }

	uint8_t pid() const { return mPID; }
	uint32_t colour() const { return mColour; }
	int lastCommandID() const { return mLastCommandID; }
	sf::Vector2f position() const { return mPosition; }

private:
	uint8_t mPID;
	uint32_t mColour;
	int mLastCommandID;
	sf::Vector2f mPosition;
};