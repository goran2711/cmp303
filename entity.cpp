#include "entity.h"
#include "common.h"

void VisualEntity::AddSample(const sf::Vector2f & v)
{
	if (serverPositions.empty())
	{
		if (position == v)
			return;
		else
			targetPosition = v;
	}
	else if (v == serverPositions.back())
		return;

	serverPositions.push_back(v);
}

// Interpolation always interpolates between the entity's
// position and the first element of serverPositions 
void VisualEntity::Interpolate(float dt)
{
	// Do not interpolate if there is not enough data to support it
	if (serverPositions.empty())
		return;

	float alpha = clamp(interpolationProgress / (float) (INTERPOLATE_TIME - (serverPositions.size() / 100.f)) , 0.f, 1.f);

	interpolationProgress += dt;

	position = position * (1.f - alpha) + alpha * targetPosition;

	// Target position reached
	if (alpha >= 1.f)
	{
		// pop front
		serverPositions.erase(serverPositions.begin());
		interpolationProgress = 0.f;

		if (!serverPositions.empty())
			targetPosition = serverPositions.front();
	}
}

void VisualEntity::Draw(sf::RenderWindow& window)
{
	sprite.setPosition(position);
	window.draw(sprite);
}
