#include "../src/Ecs.hpp"
#include <Index.hpp>
#include <Project/IndexProject.hpp>
#include <Project/ProjectManager.hpp>
#include "MovingBallsSystem.hpp"

using namespace Index;

constexpr bool UseEnTTForMovingBalls = false;

class DemoApp : public Index::Application {
public:
	Index::ApplicationConfig GetConfiguration() const override {
		auto config = ApplicationConfig::Game();
		return config;
	}

	void ConfigureScenes() override {
		auto& scene = GetSceneManager()->RegisterScene("DemoScene");
		scene.AddSystem<MovingBallsSystem>(UseEnTTForMovingBalls);
		scene.SetAsStartupScene();
	}
};

Index::Application* Index::CreateApplication() {
	auto project = std::make_unique<Index::IndexProject>();
	project->ActiveRenderBackend =
		Index::IndexProject::RenderBackend::Direct3D12;

	Index::ProjectManager::SetCurrentProject(std::move(project));

	return new DemoApp();
}

#include <EntryPoint.hpp>
