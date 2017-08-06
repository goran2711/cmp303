#pragma once
#include <SFML/System.hpp>
#include <cstdint>

namespace sf
{
	class Packet;
}

struct Command;

class Player
{
public:
	friend sf::Packet& operator<<(sf::Packet& p, const Player& player);
	friend sf::Packet& operator>>(sf::Packet& p, Player& player);

	static constexpr float MOVE_SPEED = 400.f;

	void RunCommand(const Command& cmd, bool rec);

	void SetPID(uint8_t pid) { mPID = pid; }
	void SetColour(uint32_t RGBA) { mColour = RGBA; }

	void SetX(float x) { mPosition.x = x; }
	void SetY(float y) { mPosition.y = y; }
	void SetPosition(const sf::Vector2f& position) { mPosition = position; }
	
	void SetLastCommandID(int id) { mLastCommandID = id; }

	uint8_t GetID() const { return mPID; }
	uint32_t GetColour() const { return mColour; }
	int GetLastCommandID() const { return mLastCommandID; }
	sf::Vector2f GetPosition() const { return mPosition; }

private:
	uint8_t mPID;
	uint32_t mColour;
	int mLastCommandID;
	sf::Vector2f mPosition;
};

sf::Packet& operator<<(sf::Packet& p, const Player& player);
sf::Packet& operator>>(sf::Packet& p, Player& player);
