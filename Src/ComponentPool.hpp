#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

#include "IComponentPool.hpp"

namespace Ecs {
	template<typename T>
	class ComponentPool : public IComponentPool {
		static_assert(
			std::is_object_v<T> &&
			!std::is_array_v<T> &&
			std::is_same_v<T, std::remove_cvref_t<T>>,
			"A component type must be an unqualified object type"
		);
		static_assert(
			std::is_nothrow_move_assignable_v<T>,
			"A component type must be nothrow move-assignable"
		);
		static_assert(
			std::is_nothrow_move_constructible_v<T> ||
			std::is_copy_constructible_v<T>,
			"A component type must be nothrow move-constructible or copy-constructible"
		);
		static_assert(
			std::is_nothrow_destructible_v<T>,
			"A component type must be nothrow destructible"
		);

		struct StoredComponent {
			template<typename... Args>
			explicit StoredComponent(Args&&... args)
				: Value(std::forward<Args>(args)...)
			{}

			T Value;
		};

	public:
		void Reserve(std::size_t capacity) override {
			m_Sparse.reserve(capacity);
			m_Entities.reserve(capacity);
			m_Components.reserve(capacity);
		}

		template<typename... Args>
		T& Add(Entity ent, Args&&... args) {
			if (ent.Index == std::numeric_limits<std::uint32_t>::max())
				throw std::invalid_argument("Invalid entity index");

			const std::size_t entityIndex = ent.Index;

			if (entityIndex >= m_Sparse.size())
				m_Sparse.resize(entityIndex + 1, InvalidIndex);
			else if (m_Sparse[entityIndex] != InvalidIndex)
				throw std::logic_error("Entity index already has component");

			m_Components.emplace_back(
				std::forward<Args>(args)...
			);

			try {
				m_Entities.push_back(ent);
			}
			catch (...) {
				m_Components.pop_back();
				throw;
			}

			m_Sparse[entityIndex] = m_Entities.size() - 1;
			return m_Components.back().Value;
		}

		void Remove(Entity ent) noexcept override {
			const std::size_t index = Find(ent);

			if (index == InvalidIndex)
				return;

			const std::size_t lastIndex = m_Entities.size() - 1;

			if (index != lastIndex) {
				m_Components[index] = std::move(m_Components[lastIndex]);
				m_Entities[index] = m_Entities[lastIndex];
				m_Sparse[m_Entities[index].Index] = index;
			}

			m_Components.pop_back();
			m_Entities.pop_back();
			m_Sparse[ent.Index] = InvalidIndex;
		}

		bool Contains(Entity ent) const noexcept override {
			return Find(ent) != InvalidIndex;
		}

		T& Get(Entity ent) {
			const std::size_t index = Find(ent);

			if (index != InvalidIndex)
				return m_Components[index].Value;

			throw std::runtime_error("Entity does not have component");
		}

		const T& Get(Entity ent) const {
			const std::size_t index = Find(ent);

			if (index != InvalidIndex)
				return m_Components[index].Value;

			throw std::runtime_error("Entity does not have component");
		}

		T& GetDense(std::size_t index) noexcept {
			return m_Components[index].Value;
		}

		const T& GetDense(std::size_t index) const noexcept {
			return m_Components[index].Value;
		}

		const std::vector<Entity>& Entities() const noexcept {
			return m_Entities;
		}

		std::size_t Size() const noexcept {
			return m_Entities.size();
		}

	private:
		static constexpr std::size_t InvalidIndex =
			std::numeric_limits<std::size_t>::max();

		std::size_t Find(Entity ent) const noexcept {
			const std::size_t entityIndex = ent.Index;

			if (entityIndex >= m_Sparse.size())
				return InvalidIndex;

			const std::size_t componentIndex = m_Sparse[entityIndex];

			if (componentIndex >= m_Entities.size() ||
				m_Entities[componentIndex] != ent)
			{
				return InvalidIndex;
			}

			return componentIndex;
		}

	private:
		std::vector<Entity> m_Entities;
		std::vector<StoredComponent> m_Components;
		std::vector<std::size_t> m_Sparse;
	};
}
