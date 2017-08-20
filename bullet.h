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

	void Update(sf::Uint64 dt);

	void SetID(sf::Uint32 id) { mID = id; }
	void SetColour(sf::Uint32 colour) { mColour = colour; }
	void SetDirection(const sf::Vector2f& direction) { mDirection = direction; }
	void SetPosition(const sf::Vector2f& position) { mPosition = position; }
	
	sf::Uint32 GetID() const { return mID; }
	sf::Uint32 GetColour() const { return mColour; }
	sf::Vector2f GetDirection() const { return mDirection; }
	sf::Vector2f GetPosition() const { return mPosition; }

private:
	sf::Uint32 mID;
	sf::Uint32 mColour;
	sf::Vector2f mDirection;
	sf::Vector2f mPosition;
};

sf::Packet& operator<<(sf::Packet& p, const Bullet& b);
sf::Packet& operator>>(sf::Packet& p, Bullet& b);
