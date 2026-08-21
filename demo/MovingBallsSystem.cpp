#include "MovingBallsSystem.hpp"

#include <chrono>
#include <cmath>

using namespace Index;

namespace {
	constexpr std::size_t BallCapacity = 100000;

	void InitializeBallTransform(Transform2DComponent& transform, const Vec2& position)
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
			auto& transform = m_EnTTRegistry.emplace<Transform2DComponent>(entity);
			InitializeBallTransform(transform, position);
		}
		catch (...) {
			m_EnTTRegistry.destroy(entity);
			throw;
		}
	}
	else {
		const Ecs::Entity entity = m_Registry.Create<Transform2DComponent, MovingBall>();
		InitializeBallTransform(
			m_Registry.Get<Transform2DComponent>(entity),
			position
		);
	}
}

void MovingBallsSystem::Start(Scene& scene) {
	Gizmo::SetMaxVertices(BallCapacity * 32);
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

	const Vec2 mousePosition = camera->ScreenToWorld(Input::GetMousePosition());

	if (Input::GetKey(KeyCode::E))
		CreateBall(mousePosition);

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

	static float sizeMult = 1.0f;
	float cameraSize = camera->GetOrthographicSize();
	float orthoSize = 2.0f * sizeMult;

	if (Input::GetKey(KeyCode::LeftControl))
		sizeMult += Time::GetDeltaTime();
	else if (Input::GetKey(KeyCode::LeftShift))
		sizeMult -= Time::GetDeltaTime();

	const bool magnet = Input::GetMouse(MouseButton::Left);
	const bool push = Input::GetMouse(MouseButton::Right);

	if (magnet) {
		Gizmo::SetColor(Color::Black());
		Gizmo::DrawCircle(mousePosition, orthoSize);
		Gizmo::SetColor(Color::Black());
		Gizmo::DrawWireCircle(mousePosition, orthoSize * 1.25f);
		Gizmo::SetColor(Color::White());
	}
	else if (push) {
		Gizmo::SetColor(Color::Red().WithAlpha(0.5f));
		Gizmo::DrawCircle(mousePosition, orthoSize);
		Gizmo::SetColor(Color::LightRed());
		Gizmo::DrawWireCircle(mousePosition, orthoSize * 1.25f);
		Gizmo::SetColor(Color::White());
	}

	orthoSize *= 1.25f;

	auto updateAndDraw = [&](Transform2DComponent& transform) {
		if (magnet && Vector::Distance(transform.Position, mousePosition) < orthoSize) {
			const Vec2 direction = Vector::Normalized(mousePosition - transform.Position);
			Gizmo::SetColor(Random::NextColor());
			Gizmo::DrawLine(mousePosition, transform.Position);
			Gizmo::SetColor(Color::White());
			transform.Rotation = std::atan2(direction.y, direction.x) - HalfPi<float>();
			transform.Position += direction * dt * cameraSize * 4.0f;
		}
		else if (push && Vector::Distance(transform.Position, mousePosition) < orthoSize) {
			const Vec2 direction = Vector::Normalized(transform.Position - mousePosition);
			Gizmo::SetColor(Random::NextColor());
			Gizmo::DrawLine(mousePosition, transform.Position);
			Gizmo::SetColor(Color::White());
			transform.Rotation = std::atan2(direction.y, direction.x) - HalfPi<float>();

			const Vec2 up{
				-std::sin(transform.Rotation),
				 std::cos(transform.Rotation)
			};

			transform.Position += up * dt * 5.0f;
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
		auto view = m_EnTTRegistry.view<Transform2DComponent, MovingBall>();
		ballCount = static_cast<std::size_t>(view.size_hint());

		for (auto&& [entity, transform] : view.each()) {
			updateAndDraw(transform);
		}
	}
	else {
		ballCount = m_Registry.GetEntityCount();

		for (auto&& [entity, transform] : m_Registry.Query<Transform2DComponent>().With<MovingBall>())
		{
			updateAndDraw(transform);
		}
	}

	const double loopMilliseconds =
		std::chrono::duration<double, std::milli>(
			std::chrono::steady_clock::now() - loopStart
		).count();

	Gizmo::SetColor(Color::Red());

	Gizmo::DrawText(
		StringHelper::ToString("FPS: ", 1.0f / Time::GetDeltaTimeUnscaled()),
		Vec2(0.0f, 6.0f)
	);
	Gizmo::DrawText(
		StringHelper::ToString("Graphics: ", Engine::GetGraphicsApiName()),
		Vec2(0.0f, 5.0f)
	);
	Gizmo::DrawText(
		StringHelper::ToString("Backend: ", m_UseEnTT ? "EnTT" : "Ecs"),
		Vec2(0.0f, 4.0f)
	);
	Gizmo::DrawText(
		StringHelper::ToString("Balls: ", ballCount),
		Vec2(0.0f, 3.0f)
	);
	Gizmo::DrawText(
		StringHelper::ToString("Ball loop: ", loopMilliseconds, " ms"),
		Vec2(0.0f, 2.0f)
	);
	Gizmo::SetColor(Color::White());
}
