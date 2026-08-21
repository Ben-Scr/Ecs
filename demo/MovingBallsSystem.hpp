#pragma once
#include "Ecs.hpp"
#include <Index.hpp>

struct MovingBall {};

class MovingBallsSystem : public Index::SceneSystem {
public:
	explicit MovingBallsSystem(bool useEnTT) noexcept;

	void Start(Index::Scene& scene) override;
	void Update(Index::Scene& scene) override;

private:
	void CreateBall(const Index::Vec2& position);

	const bool m_UseEnTT;
	Ecs::Registry m_Registry;
	entt::registry m_EnTTRegistry;
	Index::ParticleSystem2DComponent* m_EffectPts;
	Index::ParticleSystem2DComponent* m_ExplosionPts;

	Index::Transform2DComponent* m_EffectTr;
	Index::Transform2DComponent* m_ExplosionTr;
};
