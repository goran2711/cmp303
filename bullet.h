#pragma once
#include <SFML/System.hpp>

// bullet.h: Represents a bullet (a lot in common with Player. Should have used polymorphism)

namespace sf
{
	class Packet;
}

class Bullet
{
public:
	friend sf::Packet& operator<<(sf::Packet& p, const Bullet& b);
	friend sf::Packet& operator>>(sf::Packet& p, Bullet& b);

	constexpr static float BULLET_SPEED = 400.f;

	void Update(uint64_t dt);

	void SetID(uint32_t id) { mID = id; }
	void SetColour(uint32_t colour) { mColour = colour; }
	void SetDirection(const sf::Vector2f& direction) { mDirection = direction; }
	void SetPosition(const sf::Vector2f& position) { mPosition = position; }
	
	uint32_t GetID() const { return mID; }
	uint32_t GetColour() const { return mColour; }
	sf::Vector2f GetDirection() const { return mDirection; }
	sf::Vector2f GetPosition() const { return mPosition; }

private:
	uint32_t mID;
	uint32_t mColour;
	sf::Vector2f mDirection;
	sf::Vector2f mPosition;
};

sf::Packet& operator<<(sf::Packet& p, const Bullet& b);
sf::Packet& operator>>(sf::Packet& p, Bullet& b);