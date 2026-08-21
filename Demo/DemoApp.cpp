#include "../src/Ecs.hpp"
#include <Index.hpp>
#include "MovingBallsSystem.hpp"
#include "DemoSystem.hpp"

using namespace Index;

constexpr bool UseEnTTForMovingBalls = false;

class DemoApp : public Index::Application {
public:
	Index::ApplicationConfig GetConfiguration() const override {
		auto config = ApplicationConfig::Game();
		config.WindowSpecification.Title = "ECS Demo";
		return config;
	}

	void ConfigureScenes() override {
		auto& scene = GetSceneManager()->RegisterScene("DemoScene");
		scene.AddSystem<DemoSystem>();
		scene.SetAsStartupScene();
	}
};

Index::Application* Index::CreateApplication() {
	return new DemoApp();
}

#include <EntryPoint.hpp>
