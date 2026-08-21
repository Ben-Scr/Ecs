#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Scene/Scene.hpp"
#include "Scene/SceneDefinition.hpp"
#include "Scene/ComponentRegistry.hpp"

namespace Index {
	class PhysicsSystem2D;
}

namespace Index {
	namespace SceneManagerDetail {
		INDEX_API bool TryCreateDefaultCamera2D(Scene& scene,
			bool rendererAvailable,
			bool applicationEnabled,
			bool sceneEnabled,
			bool sceneWillBeActive);
	}

	class INDEX_API SceneManager {
		friend class Window;

	public:
		struct EntityPresetInfo {
			std::string MenuPath;
			std::string Label;
			std::string DefaultName;
			std::function<Entity(Scene&)> Create;
		};

		SceneManager() = default;
		SceneManager(const SceneManager&) = delete;
		SceneManager& operator=(const SceneManager&) = delete;

		template<typename T>
		static void RegisterComponentType(ComponentInfo componentInfo) {
			Get().RegisterComponentTypeImpl<T>(std::move(componentInfo));
		}
		static ComponentRegistry& GetComponentRegistry() {
			return Get().m_ComponentRegistry;
		}
		static void RegisterEntityPreset(EntityPresetInfo preset);
		static const std::vector<EntityPresetInfo>& GetEntityPresets() {
			return Get().m_EntityPresets;
		}
		static SceneDefinition& RegisterScene(const std::string& name);
		static std::weak_ptr<Scene> LoadScene(const std::string& name);

		static std::weak_ptr<Scene> LoadSceneAdditive(const std::string& name);
		static std::weak_ptr<Scene> ReloadScene(const std::string& name);

		static void UnloadScene(const std::string& name);
		static void UnloadAllScenes(bool includePersistent = false);

		// Drops the SceneDefinition for `name`. The scene is unloaded first if
		// currently loaded. After this returns, LoadScene(name) fails because
		// the definition lookup misses — used by the editor when the user
		// deletes a .scene file so a stale script-side LoadScene doesn't
		// resurrect an empty Scene from the registered definition.
		static void UnregisterScene(const std::string& name);

		static std::vector<std::weak_ptr<Scene>> GetLoadedScenes();
		static std::weak_ptr<Scene> GetLoadedScene(const std::string& name);
		static Scene* GetActiveScene();

		static bool SetActiveScene(const std::string& name);

		// Reorders a loaded scene to a new position in the loaded-scenes list.
		// `newIndex` is clamped to [0, GetLoadedSceneCount() - 1]. Returns
		// true on a real reorder (false if the scene isn't loaded or the
		// move is a no-op). The order affects: Entities-panel display order,
		// Update / FixedUpdate / OnPreRender iteration order, and the
		// fallback active-scene pick in RefreshActiveScene. Does NOT change
		// which scene is active — m_ActiveScene tracks a pointer, not an
		// index, so a reorder leaves the active selection intact.
		static bool MoveLoadedScene(const std::string& name, size_t newIndex);

		static bool HasSceneDefinition(const std::string& name);
		static bool IsSceneLoaded(const std::string& name);

		static bool IsInitialized() { return Get().m_IsInitialized; }

		static std::vector<std::string> GetRegisteredSceneNames();
		static std::vector<std::string> GetLoadedSceneNames();
		static size_t GetLoadedSceneCount() { return Get().m_LoadedScenes.size(); }
		static Scene* GetLoadedSceneAt(size_t index) {
			SceneManager& manager = Get();
			return index < manager.m_LoadedScenes.size() ? manager.m_LoadedScenes[index].get() : nullptr;
		}

		template<typename TFunc>
		static void ForeachLoadedScene(TFunc&& func) {
			Get().ForeachLoadedSceneImpl(std::forward<TFunc>(func));
		}

	private:
		using SceneDefinitionMap = std::unordered_map<std::string, std::unique_ptr<SceneDefinition>>;
		using LoadedSceneList = std::vector<std::shared_ptr<Scene>>;
		using SceneSetupCallback = std::function<void(Scene&)>;
		static SceneManager& Get();

		void Initialize();
		void RegisterCoreComponents();
		void Shutdown();

		void UpdateScenes();
		void OnPreRenderScenes();
		void FixedUpdateScenes();
		void InitializeStartupScenes();
		template<typename T>
		void RegisterComponentTypeImpl(ComponentInfo componentInfo) {
			m_ComponentRegistry.Register<T>(std::move(componentInfo));
		}
		void RegisterEntityPresetImpl(EntityPresetInfo preset);
		SceneDefinition& RegisterSceneImpl(const std::string& name);
		std::weak_ptr<Scene> LoadSceneImpl(const std::string& name);
		std::weak_ptr<Scene> LoadSceneAdditiveImpl(const std::string& name);
		std::weak_ptr<Scene> ReloadSceneImpl(const std::string& name);
		void UnloadSceneImpl(const std::string& name);
		void UnloadAllScenesImpl(bool includePersistent);
		void UnregisterSceneImpl(const std::string& name);
		std::vector<std::weak_ptr<Scene>> GetLoadedScenesImpl();
		std::weak_ptr<Scene> GetLoadedSceneImpl(const std::string& name);
		Scene* GetActiveSceneImpl() const;
		bool SetActiveSceneImpl(const std::string& name);
		bool MoveLoadedSceneImpl(const std::string& name, size_t newIndex);
		bool HasSceneDefinitionImpl(const std::string& name) const;
		bool IsSceneLoadedImpl(const std::string& name) const;
		std::vector<std::string> GetRegisteredSceneNamesImpl() const;
		std::vector<std::string> GetLoadedSceneNamesImpl() const;
		template<typename TFunc>
		void ForeachLoadedSceneImpl(TFunc&& func) {
			// Callbacks may append, but must not remove scenes while this loop holds vector references.
			for (size_t i = 0; i < m_LoadedScenes.size(); ++i) {
				const std::shared_ptr<Scene>& scenePointer = m_LoadedScenes[i];
				if (scenePointer && scenePointer->IsLoaded()) func(*scenePointer);
			}
		}
		std::shared_ptr<Scene> LoadSceneInternal(const std::string& name,
			bool additive,
			SceneSetupCallback setupCallback = {},
			bool makeActive = false,
			std::optional<size_t> insertIndex = std::nullopt);
		SceneDefinition* GetSceneDefinition(const std::string& name);
		const SceneDefinition* GetSceneDefinition(const std::string& name) const;
		LoadedSceneList::iterator FindLoadedSceneIterator(const std::string& name);
		LoadedSceneList::const_iterator FindLoadedSceneIterator(const std::string& name) const;
		void EnsureDefaultCamera2D(Scene& scene);
		void RollbackSceneLoad(const std::shared_ptr<Scene>& scene, size_t awakenedSystemCount);
		void ReleaseScene(LoadedSceneList::iterator it);
		void UnloadAllScenesInternal(bool includePersistent, bool ensureActiveCamera);
		void RefreshActiveScene();

		SceneDefinitionMap m_SceneDefinitions;
		std::vector<std::string> m_SceneDefinitionOrder;
		LoadedSceneList m_LoadedScenes;
		ComponentRegistry m_ComponentRegistry;
		std::vector<EntityPresetInfo> m_EntityPresets;
		Scene* m_ActiveScene = nullptr;
		bool m_IsInitialized = false;

		friend class Application;
		friend class PhysicsSystem2D;
	};
}
