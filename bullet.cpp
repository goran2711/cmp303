#include "bullet.h"
#include "network.h"

void Bullet::Update(sf::Uint64 dt)
{
	mPosition += mDirection * BULLET_SPEED * (dt / 1000.f);
}

sf::Packet& operator<<(sf::Packet& p, const Bullet& b)
{
	p << b.mID << b.mColour << b.mDirection << b.mPosition;
	return p;
}

sf::Packet& operator >> (sf::Packet& p, Bullet& b)
{
	p >> b.mID >> b.mColour >> b.mDirection >> b.mPosition;
	return p;
}
