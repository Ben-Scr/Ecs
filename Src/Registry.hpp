#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ComponentPool.hpp"
#include "TypeList.hpp"

namespace Ecs {

	namespace Detail {
		template<typename... Ts>
		struct AreUnique : std::true_type {};

		template<typename T, typename... Rest>
		struct AreUnique<T, Rest...> : std::bool_constant<
			(!std::is_same_v<T, Rest> && ...) && AreUnique<Rest...>::value
		> {};
	}

	template<
		bool IncludeEntity,
		typename Components,
		typename Required,
		typename Excluded
	>
	class QueryView;

	class Registry {
	public:
		// Reserves entity metadata and current/future component pools.
		void Reserve(std::size_t capacity) {
			if (capacity > std::numeric_limits<std::uint32_t>::max())
				throw std::length_error("Entity capacity exceeds the supported limit");

			m_Generations.reserve(capacity);
			m_Alive.reserve(capacity);
			m_FreeIndices.reserve(capacity);

			for (auto& entry : m_ComponentPools)
				entry.second->Reserve(capacity);

			if (capacity > m_ReservedCapacity)
				m_ReservedCapacity = capacity;
		}

		// Creates a new entity
		Entity Create() {
			if (!m_FreeIndices.empty()) {
				std::uint32_t index = m_FreeIndices.back();
				m_FreeIndices.pop_back();
				m_Alive[index] = true;
				++m_EntityCount;

				return Entity{
					index,
					m_Generations[index]
				};
			}

			if (m_Generations.size() >=
				std::numeric_limits<std::uint32_t>::max())
			{
				throw std::length_error("Entity capacity exhausted");
			}

			const std::uint32_t index =
				static_cast<std::uint32_t>(m_Generations.size());

			m_Generations.push_back(0);
			try {
				m_Alive.push_back(true);
				m_FreeIndices.reserve(m_Generations.capacity());
			}
			catch (...) {
				if (m_Alive.size() > index)
					m_Alive.pop_back();

				m_Generations.pop_back();
				throw;
			}

			++m_EntityCount;

			return Entity{
				index,
				0
			};
		}

		// Creates an entity and default-constructs all requested components.
		template<typename FirstComponent, typename... OtherComponents>
		Entity Create() {
			static_assert(
				Detail::AreUnique<FirstComponent, OtherComponents...>::value,
				"Create component types must be unique"
				);

			const Entity ent = Create();

			try {
				Add<FirstComponent>(ent);
				(Add<OtherComponents>(ent), ...);
			}
			catch (...) {
				Destroy(ent);
				throw;
			}

			return ent;
		}

		// Destroys the passed entity
		void Destroy(Entity ent) {
			if (!IsValid(ent))
				return;

			for (auto& entry : m_ComponentPools)
				entry.second->Remove(ent);

			m_Alive[ent.Index] = false;
			--m_EntityCount;

			if (m_Generations[ent.Index] !=
				std::numeric_limits<std::uint32_t>::max())
			{
				++m_Generations[ent.Index];
				m_FreeIndices.push_back(ent.Index);
			}
		}

		// Returns the number of currently live entities
		std::size_t GetEntityCount() const noexcept {
			return m_EntityCount;
		}

		// Validates whether an entity is valid
		bool IsValid(Entity entity) const {
			return entity.Index < m_Generations.size()
				&& m_Alive[entity.Index]
				&& m_Generations[entity.Index] == entity.Generation;
		}

		// Adds a component to the passed entity. OnAdd<T>() callbacks run after
		// construction and may configure the new component before Add returns.
		template<typename T, typename... Args>
		T& Add(Entity ent, Args&&... args) {
			if (!IsValid(ent))
				throw std::runtime_error("Invalid entity");

			auto& pool = GetOrCreatePool<T>();
			T& component = pool.Add(
				ent,
				std::forward<Args>(args)...
			);

			try {
				pool.OnAdd().Publish(*this, ent, component);
			}
			catch (...) {
				pool.Remove(ent);
				throw;
			}

			return component;
		}

		// Returns the signal emitted whenever T is newly added.
		// Callback signature: void(Registry&, Entity, T&)
		template<typename T>
		ComponentSignal<T>& OnAdd() {
			return GetOrCreatePool<T>().OnAdd();
		}

		// Default-constructs multiple components and returns references to them.
		template<
			typename FirstComponent,
			typename SecondComponent,
			typename... OtherComponents
		>
		std::tuple<
			FirstComponent&,
			SecondComponent&,
			OtherComponents&...
		> Add(Entity ent) {
			static_assert(
				Detail::AreUnique<
				FirstComponent,
				SecondComponent,
				OtherComponents...
				>::value,
				"Add component types must be unique"
				);

			if (!IsValid(ent))
				throw std::runtime_error("Invalid entity");

			const bool hasAny =
				HasUnchecked<FirstComponent>(ent) ||
				HasUnchecked<SecondComponent>(ent) ||
				(HasUnchecked<OtherComponents>(ent) || ...);

			if (hasAny)
				throw std::logic_error(
					"Entity already has one or more requested components"
				);

			AddDefaultComponents<
				FirstComponent,
				SecondComponent,
				OtherComponents...
			>(ent);

			return {
				Get<FirstComponent>(ent),
				Get<SecondComponent>(ent),
				Get<OtherComponents>(ent)...
			};
		}

		// Removes the component from the passed entity
		template<typename T>
		void Remove(Entity ent) {
			auto* pool = TryGetPool<T>();

			if (!pool)
				return;

			pool->Remove(ent);
		}

		// Removes every requested component from the entity.
		template<
			typename FirstComponent,
			typename SecondComponent,
			typename... OtherComponents
		>
		void Remove(Entity ent) {
			static_assert(
				Detail::AreUnique<
				FirstComponent,
				SecondComponent,
				OtherComponents...
				>::value,
				"Remove component types must be unique"
				);

			if (!IsValid(ent))
				return;

			Remove<FirstComponent>(ent);
			Remove<SecondComponent>(ent);
			(Remove<OtherComponents>(ent), ...);
		}

		// Returns the component of the passed entity
		template<typename T>
		T& Get(Entity ent) {
			if (!IsValid(ent))
				throw std::runtime_error("Invalid entity");

			auto* pool = TryGetPool<T>();

			if (!pool)
				throw std::runtime_error(
					"Component pool does not exist"
				);

			return pool->Get(ent);
		}

		// Returns the requested component of the entity
		template<typename T>
		const T& Get(Entity ent) const {
			if (!IsValid(ent))
				throw std::runtime_error("Invalid entity");

			const auto* pool = TryGetPool<T>();

			if (!pool)
				throw std::runtime_error(
					"Component pool does not exist"
				);

			return pool->Get(ent);
		}

		// Returns the requested component of the entity
		template<typename T>
		T* TryGet(Entity ent) {
			if (!IsValid(ent))
				return nullptr;

			auto* pool = TryGetPool<T>();

			if (!pool)
				return nullptr;

			return &pool->Get(ent);
		}

		// Returns the requested component of the entity
		template<typename T>
		const T* TryGet(Entity ent) const {
			if (!IsValid(ent))
				return nullptr;

			const auto* pool = TryGetPool<T>();

			if (!pool)
				return nullptr;

			return &pool->Get(ent);
		}

		template<typename T>
		const T& GetOrAdd(Entity ent) const {
			if (auto* tr = TryGet<T>(ent))
				return *tr;

			return Add<T>(ent);
		}

		template<typename T>
		T& GetOrAdd(Entity ent) {
			if (auto* tr = TryGet<T>(ent))
				return *tr;

			return Add<T>(ent);
		}

		// Returns whether the passed entity has the component
		template<typename T>
		bool Has(Entity ent) const {
			if (!IsValid(ent))
				return false;

			return HasUnchecked<T>(ent);
		}

		// Returns true when the entity has every requested component.
		template<
			typename FirstComponent,
			typename SecondComponent,
			typename... OtherComponents
		>
		bool Has(Entity ent) const {
			static_assert(
				Detail::AreUnique<
				FirstComponent,
				SecondComponent,
				OtherComponents...
				>::value,
				"Has component types must be unique"
				);

			if (!IsValid(ent))
				return false;

			return HasUnchecked<FirstComponent>(ent) &&
				HasUnchecked<SecondComponent>(ent) &&
				(HasUnchecked<OtherComponents>(ent) && ...);
		}

		template<typename T>
		ComponentPool<T>* TryGetPool() {
			auto it = m_ComponentPools.find(
				std::type_index(typeid(T))
			);

			if (it == m_ComponentPools.end())
				return nullptr;

			return static_cast<ComponentPool<T>*>(
				it->second.get()
				);
		}


		template<typename T>
		const ComponentPool<T>* TryGetPool() const {
			auto it = m_ComponentPools.find(
				std::type_index(typeid(T))
			);

			if (it == m_ComponentPools.end())
				return nullptr;

			return static_cast<const ComponentPool<T>*>(
				it->second.get()
				);
		}

		void Clear() {
			m_FreeIndices.clear();

			for (std::uint32_t i = 0; i < m_Generations.size(); ++i)
			{
				m_Alive[i] = false;

				if (m_Generations[i] != std::numeric_limits<std::uint32_t>::max())
				{
					++m_Generations[i];
					m_FreeIndices.push_back(i);
				}
			}

			for (auto& entry : m_ComponentPools)
				entry.second->Clear();

			m_EntityCount = 0;
		}

		void Reset()
		{
			m_FreeIndices.clear();
			m_Generations.clear();
			m_Alive.clear();
			m_ComponentPools.clear();

			m_ReservedCapacity = 0;
			m_EntityCount = 0;
		}


		// Creates a zero-copy view. Do not structurally modify its component
		// pools while iterating; call .Stable() on the view when mutation is needed.
		template<typename... Components>
		QueryView<
			true,
			TypeList<Components...>,
			TypeList<>,
			TypeList<>
		> Query();

	private:
		template<typename T>
		bool HasUnchecked(Entity ent) const {
			const auto* pool = TryGetPool<T>();

			return pool && pool->Contains(ent);
		}

		template<typename T, typename... Rest>
		void AddDefaultComponents(Entity ent) {
			Add<T>(ent);

			if constexpr (sizeof...(Rest) > 0) {
				try {
					AddDefaultComponents<Rest...>(ent);
				}
				catch (...) {
					Remove<T>(ent);
					throw;
				}
			}
		}

		template<typename T>
		ComponentPool<T>& GetOrCreatePool() {
			const std::type_index type = typeid(T);

			if (auto it = m_ComponentPools.find(type);
				it != m_ComponentPools.end())
			{
				return *static_cast<ComponentPool<T>*>(
					it->second.get()
					);
			}

			auto pool = std::make_unique<ComponentPool<T>>();
			pool->Reserve(m_ReservedCapacity);
			auto* ptr = pool.get();

			m_ComponentPools.emplace(
				type,
				std::move(pool)
			);

			return *ptr;
		}

		std::vector<std::uint32_t> m_Generations;
		std::vector<std::uint32_t> m_FreeIndices;
		std::vector<std::uint8_t> m_Alive;
		std::size_t m_ReservedCapacity = 0;
		std::size_t m_EntityCount = 0;

		std::unordered_map<
			std::type_index,
			std::unique_ptr<IComponentPool>
		> m_ComponentPools;
	};
}

#include "QueryView.hpp"
