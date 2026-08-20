#include <cstddef>
#include <concepts>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Registry.hpp"

namespace {
	struct Position {
		int Value = 0;
	};

	struct Velocity {};
	struct Disabled {};

	struct ThrowOnConstruction {
		ThrowOnConstruction() {
			throw std::runtime_error("expected construction failure");
		}
	};

	struct MoveTracked {
		MoveTracked() = default;
		MoveTracked(const MoveTracked&) = delete;
		MoveTracked& operator=(const MoveTracked&) = delete;

		MoveTracked(MoveTracked&&) noexcept {
			++Moves;
		}

		MoveTracked& operator=(MoveTracked&&) noexcept {
			++Moves;
			return *this;
		}

		inline static std::size_t Moves = 0;
	};

	using PositionQuery = decltype(
		std::declval<Ecs::Registry&>().Query<Position>()
	);

	static_assert(std::input_iterator<typename PositionQuery::Iterator>);
	static_assert(std::ranges::input_range<PositionQuery>);

	void Check(bool condition, const char* message) {
		if (!condition)
			throw std::runtime_error(message);
	}

	void TestEntityLifetime() {
		Ecs::Registry registry;
		Ecs::Entity empty;

		Check(registry.GetEntityCount() == 0,
			"A new registry must contain no live entities");
		Check(!registry.IsValid(empty), "A default entity must be invalid");

		const Ecs::Entity original = registry.Create();
		Check(registry.GetEntityCount() == 1,
			"Creating an entity must increase the live count");
		Check(registry.IsValid(original), "A created entity must be valid");
		Check(!registry.IsValid(empty), "A default entity must remain invalid");

		registry.Destroy(original);
		Check(registry.GetEntityCount() == 0,
			"Destroying an entity must decrease the live count");
		Check(!registry.IsValid(original), "A destroyed entity must be invalid");

		const Ecs::Entity phantom{ original.Index, original.Generation + 1 };
		Check(!registry.IsValid(phantom), "A free slot must not validate");
		registry.Destroy(phantom);
		Check(registry.GetEntityCount() == 0,
			"Destroying an invalid entity must not change the live count");

		const Ecs::Entity recycled = registry.Create();
		const Ecs::Entity next = registry.Create();
		Check(registry.GetEntityCount() == 2,
			"Recycled entities must be included in the live count");
		Check(recycled.Index == original.Index, "A free slot should be recycled");
		Check(recycled != next, "A free slot must not be recycled twice");

		std::vector<Ecs::Entity> entities;
		for (std::size_t i = 0; i < 256; ++i)
			entities.push_back(registry.Create());

		for (const Ecs::Entity entity : entities)
			registry.Destroy(entity);

		std::vector<bool> seen(entities.back().Index + 1, false);
		for (std::size_t i = 0; i < entities.size(); ++i) {
			const Ecs::Entity entity = registry.Create();
			Check(entity.Index < seen.size(), "Recycling produced an unknown slot");
			Check(!seen[entity.Index], "Recycling produced a duplicate live entity");
			seen[entity.Index] = true;
		}
	}

	void TestComponentIntegrity() {
		Ecs::Registry registry;
		const Ecs::Entity entity = registry.Create();
		registry.Add<Position>(entity).Value = 7;

		bool duplicateRejected = false;
		try {
			registry.Add<Position>(entity);
		}
		catch (const std::logic_error&) {
			duplicateRejected = true;
		}

		Check(duplicateRejected, "A duplicate component must be rejected");
		Check(registry.Get<Position>(entity).Value == 7,
			"Rejecting a duplicate must preserve the original component");
		const Ecs::Registry& constRegistry = registry;
		Check(constRegistry.Get<Position>(entity).Value == 7,
			"Const component lookup returned the wrong component");

		std::size_t rows = 0;
		for (auto&& [foundEntity, position] : registry.Query<Position>()) {
			Check(foundEntity == entity, "A query returned the wrong entity");
			Check(position.Value == 7, "A query returned the wrong component");
			++rows;
		}
		Check(rows == 1, "A component must produce exactly one query row");

		const Ecs::Entity throwingEntity = registry.Create();
		bool constructionFailed = false;
		try {
			registry.Add<ThrowOnConstruction>(throwingEntity);
		}
		catch (const std::runtime_error&) {
			constructionFailed = true;
		}

		Check(constructionFailed, "The throwing component did not throw");
		Check(!registry.Has<ThrowOnConstruction>(throwingEntity),
			"A failed insertion must not leave an entity-only row");

		const Ecs::Entity boolEntity = registry.Create();
		registry.Add<bool>(boolEntity, true);
		Check(registry.Get<bool>(boolEntity), "A bool component lost its value");
		registry.Remove<bool>(boolEntity);
		Check(!registry.Has<bool>(boolEntity), "A bool component was not removed");
	}

	void TestReserveAndCreateWithComponents() {
		Ecs::Registry registry;
		registry.Reserve(512);
		registry.Reserve(8);

		MoveTracked::Moves = 0;
		for (std::size_t i = 0; i < 512; ++i)
			registry.Create<MoveTracked>();

		Check(MoveTracked::Moves == 0,
			"Reserve did not reach a component pool created later");

		const Ecs::Entity entity = registry.Create<Position, Velocity>();
		Check(registry.IsValid(entity),
			"Create with components returned an invalid entity");
		Check(registry.Has<Position>(entity),
			"Create with components omitted the first component");
		Check(registry.Has<Velocity>(entity),
			"Create with components omitted a later component");
		Check(registry.Get<Position>(entity).Value == 0,
			"Create with components did not value-initialize a component");

		registry.Get<Position>(entity).Value = 42;
		registry.Reserve(1024);
		Check(registry.Get<Position>(entity).Value == 42,
			"Reserving an existing pool changed a component");

		Ecs::Registry rollbackRegistry;
		rollbackRegistry.Reserve(8);
		bool constructionFailed = false;

		try {
			rollbackRegistry.Create<Position, ThrowOnConstruction>();
		}
		catch (const std::runtime_error&) {
			constructionFailed = true;
		}

		Check(constructionFailed,
			"Create with a throwing component did not propagate the failure");
		const Ecs::Entity recycled = rollbackRegistry.Create();
		Check(recycled.Index == 0 && recycled.Generation == 1,
			"A failed component bundle did not recycle its entity safely");
		Check(!rollbackRegistry.Has<Position>(recycled),
			"A failed component bundle left an earlier component behind");
		rollbackRegistry.Add<Position>(recycled).Value = 73;
		Check(rollbackRegistry.Get<Position>(recycled).Value == 73,
			"A failed component bundle left a stale sparse slot behind");
		const auto failedQuery = rollbackRegistry.Query<Position>();
		std::size_t recycledRows = 0;
		for (auto&& [foundEntity, position] : failedQuery) {
			Check(foundEntity == recycled && position.Value == 73,
				"Rollback recovery produced an incorrect query row");
			++recycledRows;
		}
		Check(recycledRows == 1,
			"Rollback recovery produced the wrong query row count");
	}

	void TestSparseComponentLookup() {
		Ecs::Registry registry;
		registry.Reserve(1024);

		std::vector<Ecs::Entity> entities;
		for (int value = 0; value < 1024; ++value) {
			const Ecs::Entity entity = registry.Create<Position>();
			registry.Get<Position>(entity).Value = value;
			entities.push_back(entity);
		}

		const Ecs::Entity removed = entities[341];
		const Ecs::Entity moved = entities.back();
		registry.Remove<Position>(removed);

		Check(!registry.Has<Position>(removed),
			"Sparse removal left the removed component addressable");
		Check(registry.Get<Position>(moved).Value == 1023,
			"Sparse removal broke the moved component's lookup");

		registry.Add<Position>(removed).Value = 341;
		Check(registry.Get<Position>(removed).Value == 341,
			"A removed sparse slot could not be added again");

		const Ecs::Entity stale = entities[512];
		registry.Destroy(stale);
		const Ecs::Entity replacement = registry.Create<Position>();
		registry.Get<Position>(replacement).Value = 9001;
		registry.Remove<Position>(stale);
		Check(registry.Get<Position>(replacement).Value == 9001,
			"A stale generation removed the replacement entity's component");
	}

	void TestVariadicComponentOperations() {
		Ecs::Registry registry;
		registry.Reserve(8);

		const Ecs::Entity entity = registry.Create();
		auto components = registry.Add<Position, Velocity>(entity);
		std::get<0>(components).Value = 123;

		Check(registry.Has<Position, Velocity>(entity),
			"Variadic Has did not find every component");
		Check(registry.Get<Position>(entity).Value == 123,
			"Variadic Add returned the wrong component reference");

		registry.Add<Disabled>(entity);
		registry.Remove<Position, Velocity>(entity);
		Check(!registry.Has<Position>(entity) && !registry.Has<Velocity>(entity),
			"Variadic Remove left a requested component behind");
		Check(registry.Has<Disabled>(entity),
			"Variadic Remove removed an unrequested component");

		const Ecs::Entity partial = registry.Create();
		registry.Add<Position>(partial).Value = 77;
		Check(!registry.Has<Position, Velocity>(partial),
			"Variadic Has used any-component rather than all-component semantics");

		bool existingRejected = false;
		try {
			registry.Add<Position, Velocity>(partial);
		}
		catch (const std::logic_error&) {
			existingRejected = true;
		}

		Check(existingRejected,
			"Variadic Add accepted a partially existing component set");
		Check(registry.Get<Position>(partial).Value == 77,
			"Rejected variadic Add changed an existing component");
		Check(!registry.Has<Velocity>(partial),
			"Rejected variadic Add inserted a later component");

		const Ecs::Entity rollback = registry.Create();
		bool constructionFailed = false;
		try {
			registry.Add<Position, ThrowOnConstruction>(rollback);
		}
		catch (const std::runtime_error&) {
			constructionFailed = true;
		}

		Check(constructionFailed,
			"Variadic Add did not propagate a construction failure");
		Check(!registry.Has<Position>(rollback),
			"Variadic Add left an earlier component after failure");
		registry.Add<Position>(rollback).Value = 9;
		Check(registry.Get<Position>(rollback).Value == 9,
			"Variadic Add rollback left a stale sparse entry");
	}

	void TestQueryFiltersAndConstIteration() {
		Ecs::Registry registry;
		const Ecs::Entity included = registry.Create();
		const Ecs::Entity excluded = registry.Create();
		const Ecs::Entity positionOnly = registry.Create();

		registry.Add<Position>(included).Value = 11;
		registry.Add<Velocity>(included);
		registry.Add<Position>(excluded).Value = 22;
		registry.Add<Velocity>(excluded);
		registry.Add<Disabled>(excluded);
		registry.Add<Position>(positionOnly).Value = 33;

		const auto query = registry.Query<Position>()
			.With<Velocity>()
			.Without<Disabled>();

		std::size_t rows = 0;
		for (auto&& [entity, position] : query) {
			Check(entity == included, "Query filters included the wrong entity");
			Check(position.Value == 11, "Query filters returned the wrong value");
			++rows;
		}
		Check(rows == 1, "Query filters returned the wrong row count");

		std::size_t componentOnlyRows = 0;
		for (auto&& [position] : query.WithoutEntity()) {
			Check(position.Value == 11,
				"A component-only query returned the wrong value");
			++componentOnlyRows;
		}
		Check(componentOnlyRows == 1,
			"A component-only query returned the wrong row count");

		auto iterator = query.begin();
		const auto end = query.end();
		Check(iterator != end, "A non-empty query started at end");
		const auto previous = iterator++;
		Check(previous != end, "Postfix increment returned the wrong iterator");
		Check(iterator == end, "Iterator equality did not recognize the end");
	}

	void TestStructuralMutationDuringQuery() {
		Ecs::Registry registry;
		for (int value = 0; value < 4; ++value) {
			const Ecs::Entity entity = registry.Create();
			registry.Add<Position>(entity).Value = value;
		}

		std::size_t destroyed = 0;
		for (auto&& [entity, position] :
			registry.Query<Position>().Stable())
		{
			(void)position;
			registry.Destroy(entity);
			++destroyed;
		}

		Check(destroyed == 4,
			"Destroying entities during a query skipped or overran candidates");
		const auto emptyQuery = registry.Query<Position>();
		Check(emptyQuery.begin() == emptyQuery.end(),
			"Destroyed entities remained queryable");
	}
}

int main() {
	try {
		TestEntityLifetime();
		TestComponentIntegrity();
		TestReserveAndCreateWithComponents();
		TestSparseComponentLookup();
		TestVariadicComponentOperations();
		TestQueryFiltersAndConstIteration();
		TestStructuralMutationDuringQuery();
	}
	catch (const std::exception& error) {
		std::cerr << "ECS test failure: " << error.what() << '\n';
		return 1;
	}

	std::cout << "All ECS tests passed\n";
	return 0;
}
