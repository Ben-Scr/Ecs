#include "MovingBallsSystem.hpp"

#include <chrono>
#include <cmath>

using namespace Index;

namespace {
	constexpr std::size_t BallCapacity = 100000;

	void InitializeBallTransform(
		Transform2DComponent& transform,
		const Vec2& position)
	{
		transform.Position = position;
		transform.LocalPosition = position;
		transform.Rotation = Random::NextFloat(TwoPi<float>());
		transform.LocalRotation = transform.Rotation;
	}
}

MovingBallsSystem::MovingBallsSystem(bool useEnTT) noexcept
	: m_UseEnTT(useEnTT)
{}

void MovingBallsSystem::CreateBall(const Vec2& position) {
	if (m_UseEnTT) {
		const EntityHandle entity = m_EnTTRegistry.create();

		try {
			m_EnTTRegistry.emplace<MovingBall>(entity);
			auto& transform =
				m_EnTTRegistry.emplace<Transform2DComponent>(entity);
			InitializeBallTransform(transform, position);
		}
		catch (...) {
			m_EnTTRegistry.destroy(entity);
			throw;
		}

		return;
	}

	const Ecs::Entity entity = m_Registry.Create<
		Transform2DComponent,
		MovingBall
	>();
	InitializeBallTransform(
		m_Registry.Get<Transform2DComponent>(entity),
		position
	);
}

void MovingBallsSystem::Start(Scene& scene) {
	Gizmo::SetMaxVertices(10000000);

	CreateBall(Vector::Zero());

	if (m_UseEnTT) {
		m_EnTTRegistry.storage<EntityHandle>().reserve(BallCapacity);
		m_EnTTRegistry.storage<Transform2DComponent>().reserve(BallCapacity);
		m_EnTTRegistry.storage<MovingBall>().reserve(BallCapacity);
	}
	else {
		m_Registry.Reserve(BallCapacity);
	}
}

void MovingBallsSystem::Update(Scene& scene) {
	auto* camera = scene.GetMainCamera();
	if (!camera)
		return;

	const Vec2 mousePosition =
		camera->ScreenToWorld(Input::GetMousePosition());

	if (Input::GetKey(KeyCode::E)) {
		CreateBall(mousePosition);
	}

	if (Input::GetKeyDown(KeyCode::B)) {
		for (int i = 0; i < 1000; ++i)
			CreateBall(mousePosition);
	}

	static bool fastTimeScale = false;
	if (Input::GetKeyDown(KeyCode::F1)) {
		Time::SetTimeScale(fastTimeScale ? 1.0f : 10.0f);
		fastTimeScale = !fastTimeScale;
	}

	const float dt = Time::GetDeltaTime();
	camera->UpdateViewport();
	const AABB cameraViewport = camera->GetViewportAABB();
	camera->AddOrthographicSize(-Input::ScrollValue());

	const bool magnet = Input::GetMouse(MouseButton::Middle);

	auto updateAndDraw = [&](Transform2DComponent& transform) {
		if (magnet) {
			const Vec2 direction = mousePosition - transform.Position;
			transform.Rotation =
				std::atan2(direction.y, direction.x) - HalfPi<float>();
			transform.Position += direction * dt;
		}
		else {
			const Vec2 up{
				-std::sin(transform.Rotation),
				 std::cos(transform.Rotation)
			};

			transform.Position += up * dt * 5.0f;

			if (transform.Position.x < cameraViewport.Min.x + 0.5f) {
				transform.Position.x = cameraViewport.Min.x + 0.5f;
				transform.Rotation = -transform.Rotation;
			}
			else if (transform.Position.x > cameraViewport.Max.x - 0.5f) {
				transform.Position.x = cameraViewport.Max.x - 0.5f;
				transform.Rotation = -transform.Rotation;
			}

			if (transform.Position.y < cameraViewport.Min.y + 0.5f) {
				transform.Position.y = cameraViewport.Min.y + 0.5f;
				transform.Rotation = Pi<float>() - transform.Rotation;
			}
			else if (transform.Position.y > cameraViewport.Max.y - 0.5f) {
				transform.Position.y = cameraViewport.Max.y - 0.5f;
				transform.Rotation = Pi<float>() - transform.Rotation;
			}

			transform.Rotation = NormalizeAngle(transform.Rotation);
		}

		transform.LocalPosition = transform.Position;
		transform.LocalRotation = transform.Rotation;
		Gizmo::DrawCircle(transform.Position, 0.5f, 16);
	};

	const auto loopStart = std::chrono::steady_clock::now();
	std::size_t ballCount = 0;

	if (m_UseEnTT) {
		auto view = m_EnTTRegistry.view<
			Transform2DComponent,
			MovingBall
		>();
		ballCount = static_cast<std::size_t>(view.size_hint());

		for (auto&& [entity, transform] : view.each()) {
			(void)entity;
			updateAndDraw(transform);
		}
	}
	else {
		ballCount = m_Registry.GetEntityCount();

		for (auto&& [entity, transform] :
			m_Registry.Query<Transform2DComponent>().With<MovingBall>())
		{
			(void)entity;
			updateAndDraw(transform);
		}
	}

	const double loopMilliseconds =
		std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - loopStart
		).count();

	Gizmo::SetColor(Color::Red());
	Gizmo::DrawTextW(
		StringHelper::ToString("FPS: ", 1.0f / Time::GetDeltaTimeUnscaled()),
		Vec2(0.0f, 6.0f)
	);
	Gizmo::DrawTextW(
		StringHelper::ToString("Graphics: ", Engine::GetGraphicsApiName()),
		Vec2(0.0f, 5.0f)
	);
	Gizmo::DrawTextW(
		StringHelper::ToString("Backend: ", m_UseEnTT ? "EnTT" : "Ecs"),
		Vec2(0.0f, 4.0f)
	);
	Gizmo::DrawTextW(
		StringHelper::ToString("Balls: ", ballCount),
		Vec2(0.0f, 3.0f)
	);
	Gizmo::DrawTextW(
		StringHelper::ToString("Ball loop: ", loopMilliseconds, " ms"),
		Vec2(0.0f, 2.0f)
	);
	Gizmo::SetColor(Color::White());
}
