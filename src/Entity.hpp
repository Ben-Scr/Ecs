#pragma once
#include <cstdint>
#include <limits>

namespace Ecs {

	struct Entity
	{
		std::uint32_t Index = std::numeric_limits<std::uint32_t>::max();
		std::uint32_t Generation = 0;

	    static const Entity Null;

		constexpr bool operator==(const Entity& other) const noexcept {
			return Index == other.Index &&
				Generation == other.Generation;
		}

		constexpr bool operator!=(const Entity& other) const noexcept {
			return !(*this == other);
		}
	};

	inline const Entity Entity::Null{};
}
