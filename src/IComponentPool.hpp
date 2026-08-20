#pragma once
#include <cstddef>

#include "Entity.hpp"

namespace Ecs {

	class IComponentPool {
	public:
		virtual ~IComponentPool() = default;
		virtual void Reserve(std::size_t capacity) = 0;
		virtual void Remove(Entity ent) noexcept = 0;
		virtual bool Contains(Entity ent) const noexcept = 0;
	};
}
