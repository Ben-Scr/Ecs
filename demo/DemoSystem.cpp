#include "DemoSystem.hpp"

struct Ticker {
	float TimeLeft = 1.0f;
};

Ecs::Entity startEntity;

Ecs::Entity DemoSystem::CreateEntity() {
	Ecs::Entity ent = m_Registry.Create<Transform2DComponent, Color, Ticker>();
	Color& col = m_Registry.Get<Color>(ent);
	col = Random::NextColor();
	return ent;
}
void DemoSystem::CreateEntityAt(const Vec2& pos) {
	Ecs::Entity ent = CreateEntity();
	auto& tr2D = m_Registry.Get<Transform2DComponent>(ent);
	tr2D.Position = pos;
}
void DemoSystem::DestroyEntity(Ecs::Entity ent) {
	m_Registry.Destroy(ent);
}

void DemoSystem::Start(Scene& scene) {
	CreateEntity();

	startEntity = m_Registry.Create<Transform2DComponent, Color, Ticker>();
	Color& col = m_Registry.Get<Color>(startEntity);
	col = Random::NextColor();
	auto& tr2D = m_Registry.Get<Transform2DComponent>(startEntity);
	tr2D.Position = Vec2(0, 2.0f);
}

Color OppositeColor(const Color& col)
{
	return {
		1.0f - col.r,
		1.0f - col.g,
		1.0f - col.b,
		col.a
	};
}


void DemoSystem::Update(Scene& scene) {
	auto* camera = scene.GetMainCamera();
	if (!camera) return;

	const Vec2 mousePosition = camera->ScreenToWorld(Input::GetMousePosition());

	if (Input::GetMouseDown(MouseButton::Left)) {
		CreateEntityAt(mousePosition);
	}

	if (Input::GetKeyDown(KeyCode::C)) {
		m_Registry.Clear();
	}

	bool ereaseEntity = Input::GetMouseDown(MouseButton::Right);
	std::vector<Ecs::Entity> entities;

	Gizmo::SetColor(Color::White());

	if (m_Registry.IsValid(startEntity)) {
		auto& tr2D = m_Registry.Get<Transform2DComponent>(startEntity);
		tr2D.Position = mousePosition;
		Gizmo::DrawSquare(tr2D.Position, tr2D.Scale, tr2D.Rotation);
	}

	// Draws all entities who have a transform and a color component attached
	for (auto&& [ent, tr2D, col, ticker] : m_Registry.Query<Transform2DComponent, Color, Ticker>())
	{
		ticker.TimeLeft -= Time::GetDeltaTime();

		if (ereaseEntity) {
			AABB entityAABB = AABB::FromTransform(tr2D);

			if (AABB::Contains(entityAABB, mousePosition))
				entities.push_back(ent);
		}

		if (ticker.TimeLeft <= 0) {
			col = Random::NextColor();
			ticker.TimeLeft = 0.25f;
		}

		Gizmo::SetColor(col);
		Gizmo::DrawSquare(tr2D.Position, tr2D.Scale, tr2D.Rotation);
		Gizmo::SetColor(OppositeColor(col));
		Gizmo::DrawTextW(StringHelper::ToString("Entity ", ent.Index, " | ", ent.Generation), tr2D.Position, tr2D.Rotation);
	}

	for (auto ent : entities) {
		DestroyEntity(ent);
	}

	entities.clear();
}
