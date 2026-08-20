#include <Index.hpp>

#include <Core/PackageHost.hpp>
#include <Project/ProjectManager.hpp>
#include <Serialization/Path.hpp>
#include <ExamplePackage.hpp>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include <windows.h>

using namespace Index;

namespace {

class MainLayer final : public Layer {
public:
    MainLayer()
        : Layer("Main")
    {
    }

    void OnUpdate(Application&, float) override
    {
        if (Input::GetKeyDown(KeyCode::Esc)) {
            Application::Quit();
        }
    }
};

class MinimalApp final : public Application {
public:
    MinimalApp()
    {
        const CommandLineArgs args = GetCommandLineArgs();
        for (int i = 1; i < args.Count; ++i) {
            if (args[i] && std::string_view(args[i]) == "--smoke-test") {
                m_SmokeTest = true;
            }
        }
    }

    ~MinimalApp() override
    {
        if (m_SmokeTest && !m_SmokeCompleted) {
            std::abort();
        }
    }

    ApplicationConfig GetConfiguration() const override
    {
        auto config = ApplicationConfig::Minimal();
        config.WindowSpecification = WindowSpecification(
            960, 540, "Index Native SDK", true, true, false);
        config.EnableRenderer2D = true;
        config.EnableShaderManager = true;
        config.EnableTextureManager = true;
        config.EnablePackageHost = true;
        config.SetWindowIcon = false;
        config.Vsync = true;
        return config;
    }

    void ConfigureScenes() override
    {
        Application::SetIsPlaying(true);
        SceneManager::RegisterScene("Main").SetAsStartupScene();
    }

    void ConfigureLayers() override { PushLayer<MainLayer>(); }
    void Start() override
    {
        if (m_SmokeTest && !PackageHost::IsPackageLoaded("Example")) {
            std::abort();
        }
        if (m_SmokeTest) {
            IndexExamplePackage_IsLoadedFn isLoaded = nullptr;
            for (const LoadedPackage& package : PackageHost::GetLoaded()) {
                if (package.Name == "Example") {
                    isLoaded = reinterpret_cast<IndexExamplePackage_IsLoadedFn>(
                        ::GetProcAddress(static_cast<HMODULE>(package.ModuleHandle),
                            "IndexExamplePackage_IsLoaded"));
                    break;
                }
            }
            if (!isLoaded || isLoaded() != 1) {
                std::abort();
            }
        }

        SetTargetFramerate(144.0f);
        Camera2DComponent* camera = Camera2DComponent::GetMain();
        Scene* scene = SceneManager::GetActiveScene();
        if (!camera || !scene) {
            if (m_SmokeTest) std::abort();
            return;
        }
        if (m_SmokeTest) {
            const EntityHandle cameraEntity = camera->GetOwnerEntity();
            if (!camera->IsValid()
                || camera->GetOwnerScene() != scene
                || cameraEntity == entt::null
                || !scene->IsValid(cameraEntity)
                || !scene->HasComponent<Camera2DComponent>(cameraEntity)
                || scene->GetEntityOrigin(cameraEntity) != EntityOrigin::Runtime) {
                std::abort();
            }
        }
        camera->SetOrthographicSize(10.0f);
        camera->SetClearColor(Color{ 0.02f, 0.03f, 0.05f, 1.0f });
        camera->SetPostProcessingEnabled(false);

        if (m_SmokeTest) {
            SceneManager::RegisterScene("Additive");
            auto additive = SceneManager::LoadSceneAdditive("Additive").lock();
            if (!additive
                || additive->GetMainCamera() != nullptr
                || !SceneManager::SetActiveScene("Additive")
                || additive->GetMainCamera() == nullptr) {
                std::abort();
            }

            SceneManager::RegisterScene("CameraFree").WithoutDefaultCamera2D();
            auto cameraFree = SceneManager::LoadSceneAdditive("CameraFree").lock();
            if (!cameraFree || !SceneManager::SetActiveScene("CameraFree")
                || cameraFree->GetMainCamera() != nullptr) {
                std::abort();
            }
            if (!SceneManager::SetActiveScene("Main")
                || Camera2DComponent::GetMain() == nullptr) {
                std::abort();
            }
        }
    }
    void Update() override
    {
        if (m_SmokeTest && ++m_Frames >= 120) {
            m_SmokeCompleted = true;
            Quit();
        }
    }
    void FixedUpdate() override {}
    void OnPaused() override {}
    void OnQuit() override {}

private:
    bool m_SmokeTest = false;
    bool m_SmokeCompleted = false;
    int m_Frames = 0;
};

}

Index::Application* Index::CreateApplication()
{
    const std::filesystem::path appRoot(Path::ExecutableDir());
    const Application::CommandLineArgs args = Application::GetCommandLineArgs();
    bool smokeTest = false;
    for (int i = 1; i < args.Count; ++i) {
        if (args[i] && std::string_view(args[i]) == "--smoke-test") {
            smokeTest = true;
        }
    }

    auto project = std::make_unique<IndexProject>();
    project->Name = "Native SDK Example";
    project->RootDirectory = appRoot.string();
    project->AssetsDirectory = (appRoot / "Assets").string();
    project->PackagesDirectory = (appRoot / "Packages").string();
    if (smokeTest) {
        project->Packages = { "Example" };
    }
    ProjectManager::SetCurrentProject(std::move(project));
    return new MinimalApp();
}

#include <EntryPoint.hpp>