#pragma once
#include <Index.hpp>
#include <Ecs.hpp>

using namespace Index;

class DemoSystem : public SceneSystem {
public:
	void Start(Scene& scene) override;
	void Update(Scene& scene) override;

	Ecs::Entity CreateEntity();
	void CreateEntityAt(const Vec2& pos);
	void DestroyEntity(Ecs::Entity ent);

private:
	Ecs::Registry m_Registry;
};