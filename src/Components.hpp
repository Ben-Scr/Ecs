#pragma once
#include <string>

#include "Vec.hpp"


struct TransformComponent {
	Vec3 Position;
	Vec3 Scale;
	Vec3 Rotation;
};

struct NameComponent {
	std::string Name;
};
