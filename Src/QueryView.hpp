#pragma once

#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "Entity.hpp"
#include "TypeList.hpp"
#include "Registry.hpp"

namespace Ecs {

	template<
		bool IncludeEntity,
		typename Components,
		typename Required,
		typename Excluded
	>
	class QueryView;


	template<
		bool IncludeEntity,
		typename... Components,
		typename... Required,
		typename... Excluded
	>
	class QueryView<
		IncludeEntity,
		TypeList<Components...>,
		TypeList<Required...>,
		TypeList<Excluded...>
	> {
		static_assert(
			sizeof...(Components) > 0,
			"A query needs at least one component"
		);
		static_assert(
			(std::is_object_v<Components> && ...)
			&& (std::is_object_v<Required> && ...)
			&& (std::is_object_v<Excluded> && ...)
			&& (!std::is_array_v<Components> && ...)
			&& (!std::is_array_v<Required> && ...)
			&& (!std::is_array_v<Excluded> && ...)
			&& (std::is_same_v<Components, std::remove_cvref_t<Components>> && ...)
			&& (std::is_same_v<Required, std::remove_cvref_t<Required>> && ...)
			&& (std::is_same_v<Excluded, std::remove_cvref_t<Excluded>> && ...),
			"Query component types must be unqualified object types"
		);

		using ComponentPools =
			std::tuple<ComponentPool<Components>*...>;
		using RequiredPools =
			std::tuple<ComponentPool<Required>*...>;
		using ExcludedPools =
			std::tuple<ComponentPool<Excluded>*...>;

	public:
		explicit QueryView(
			Registry& registry,
			bool snapshotCandidates = false)
			: m_Registry(&registry),
			m_SnapshotCandidates(snapshotCandidates)
		{}


		template<typename... Ts>
		auto With() const {
			return QueryView<
				IncludeEntity,
				TypeList<Components...>,
				TypeList<Required..., Ts...>,
				TypeList<Excluded...>
			>{
				*m_Registry,
				m_SnapshotCandidates
			};
		}


		template<typename... Ts>
		auto Without() const {
			return QueryView<
				IncludeEntity,
				TypeList<Components...>,
				TypeList<Required...>,
				TypeList<Excluded..., Ts...>
			>{
				*m_Registry,
				m_SnapshotCandidates
			};
		}


		auto WithoutEntity() const {
			return QueryView<
				false,
				TypeList<Components...>,
				TypeList<Required...>,
				TypeList<Excluded...>
			>{
				*m_Registry,
				m_SnapshotCandidates
			};
		}


		// Takes an entity snapshot when iteration begins, allowing components
		// and entities to be added or removed safely during that iteration.
		auto Stable() const {
			return QueryView{
				*m_Registry,
				true
			};
		}


	private:
		const std::vector<Entity>& CandidateEntities(
			const ComponentPools& componentPools,
			const RequiredPools& requiredPools,
			const IComponentPool*& candidatePool) const
		{
			static const std::vector<Entity> empty;
			const std::vector<Entity>* candidates = nullptr;
			std::size_t smallestSize =
				std::numeric_limits<std::size_t>::max();
			bool allPoolsExist = true;

			auto considerPool = [&](const auto* pool) {
				if (!pool) {
					allPoolsExist = false;
					return;
				}

				if (pool->Size() < smallestSize) {
					smallestSize = pool->Size();
					candidates = &pool->Entities();
					candidatePool = pool;
				}
			};

			std::apply(
				[&](const auto*... pools) {
					(considerPool(pools), ...);
				},
				componentPools
			);
			std::apply(
				[&](const auto*... pools) {
					(considerPool(pools), ...);
				},
				requiredPools
			);

			return allPoolsExist && candidates
				? *candidates
				: empty;
		}


	public:
		class Iterator {
		public:
			using difference_type = std::ptrdiff_t;
			using value_type = std::conditional_t<
				IncludeEntity,
				std::tuple<Entity, Components...>,
				std::tuple<Components...>
			>;
			using reference = std::conditional_t<
				IncludeEntity,
				std::tuple<Entity, Components&...>,
				std::tuple<Components&...>
			>;
			using iterator_category = std::input_iterator_tag;
			using iterator_concept = std::input_iterator_tag;

			Iterator() = default;


			Iterator& operator++() {
				if (!ReachedEnd()) {
					++m_Index;
					SkipInvalid();
				}

				return *this;
			}

			Iterator operator++(int) {
				Iterator previous = *this;
				++(*this);
				return previous;
			}

			bool operator==(const Iterator& other) const noexcept {
				if (m_Registry != other.m_Registry)
					return false;

				const bool reachedEnd = ReachedEnd();
				const bool otherReachedEnd = other.ReachedEnd();

				if (reachedEnd || otherReachedEnd)
					return reachedEnd && otherReachedEnd;

				return m_Candidates == other.m_Candidates
					&& m_Index == other.m_Index;
			}

			bool operator!=(const Iterator& other) const noexcept {
				return !(*this == other);
			}

			reference operator*() const {
				const Entity ent = (*m_Candidates)[m_Index];

				return std::apply(
					[&](auto*... pools) -> reference {
						auto getComponent = [&](auto* pool) -> decltype(auto) {
							if (m_CandidateIndexAligned &&
								pool == m_CandidatePool)
							{
								return pool->GetDense(m_Index);
							}

							return pool->Get(ent);
						};

						if constexpr (IncludeEntity) {
							return reference{
								ent,
								getComponent(pools)...
							};
						}
						else {
							return reference{
								getComponent(pools)...
							};
						}
					},
					m_ComponentPools
				);
			}


		private:
			Iterator(
				Registry& registry,
				const std::vector<Entity>& candidates,
				ComponentPools componentPools,
				RequiredPools requiredPools,
				ExcludedPools excludedPools,
				const IComponentPool* candidatePool,
				bool candidateIndexAligned,
				std::shared_ptr<const std::vector<Entity>> candidateOwner = {}
			)
				: m_Registry(&registry),
				m_Candidates(&candidates),
				m_CandidateOwner(std::move(candidateOwner)),
				m_ComponentPools(std::move(componentPools)),
				m_RequiredPools(std::move(requiredPools)),
				m_ExcludedPools(std::move(excludedPools)),
				m_CandidatePool(candidatePool),
				m_CandidateIndexAligned(candidateIndexAligned),
				m_IsSentinel(false)
			{
				SkipInvalid();
			}

			explicit Iterator(Registry& registry)
				: m_Registry(&registry)
			{}

			bool Matches(Entity ent) const {
				const bool hasComponents =
					std::apply(
						[&](auto*... pools) {
							return (
								(pools &&
									(m_CandidateIndexAligned &&
										pools == m_CandidatePool ||
										pools->Contains(ent))) && ...
							);
						},
						m_ComponentPools
					);

				const bool hasRequired =
					std::apply(
						[&](auto*... pools) {
							return (
								(pools &&
									(m_CandidateIndexAligned &&
										pools == m_CandidatePool ||
										pools->Contains(ent))) && ...
							);
						},
						m_RequiredPools
					);

				const bool hasExcluded =
					std::apply(
						[&](auto*... pools) {
							return (
								(pools && pools->Contains(ent)) || ...
							);
						},
						m_ExcludedPools
					);

				return hasComponents && hasRequired && !hasExcluded;
			}

			bool ReachedEnd() const noexcept {
				return m_IsSentinel
					|| !m_Candidates
					|| m_Index >= m_Candidates->size();
			}

			void SkipInvalid() {
				while (
					m_Index < m_Candidates->size() &&
					!Matches((*m_Candidates)[m_Index])
					) {
					++m_Index;
				}
			}


		private:
			friend class QueryView;

			Registry* m_Registry = nullptr;
			const std::vector<Entity>* m_Candidates = nullptr;
			std::shared_ptr<const std::vector<Entity>> m_CandidateOwner;
			ComponentPools m_ComponentPools;
			RequiredPools m_RequiredPools;
			ExcludedPools m_ExcludedPools;
			const IComponentPool* m_CandidatePool = nullptr;
			bool m_CandidateIndexAligned = false;
			std::size_t m_Index = 0;
			bool m_IsSentinel = true;
		};


		Iterator begin() const {
			ComponentPools componentPools{
				m_Registry->TryGetPool<Components>()...
			};
			RequiredPools requiredPools{
				m_Registry->TryGetPool<Required>()...
			};
			ExcludedPools excludedPools{
				m_Registry->TryGetPool<Excluded>()...
			};

			const IComponentPool* candidatePool = nullptr;
			const auto& candidateEntities = CandidateEntities(
				componentPools,
				requiredPools,
				candidatePool
			);

			if (m_SnapshotCandidates) {
				auto candidates =
					std::make_shared<const std::vector<Entity>>(
						candidateEntities
					);
				const auto* stableCandidates = candidates.get();

				return Iterator{
					*m_Registry,
					*stableCandidates,
					std::move(componentPools),
					std::move(requiredPools),
					std::move(excludedPools),
					candidatePool,
					false,
					std::move(candidates)
				};
			}

			return Iterator{
				*m_Registry,
				candidateEntities,
				std::move(componentPools),
				std::move(requiredPools),
				std::move(excludedPools),
				candidatePool,
				true
			};
		}


		Iterator end() const {
			return Iterator{ *m_Registry };
		}


	private:
		Registry* m_Registry;
		bool m_SnapshotCandidates = false;
	};

	template<typename... Components>
	QueryView<
		true,
		TypeList<Components...>,
		TypeList<>,
		TypeList<>
	> Registry::Query()
	{
		static_assert(
			sizeof...(Components) > 0,
			"Query requires at least one component"
			);

		return QueryView<
			true,
			TypeList<Components...>,
			TypeList<>,
			TypeList<>
		>{
			*this
		};
	}
}
