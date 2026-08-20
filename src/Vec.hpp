#pragma once

struct Vec2 {
	float X;
	float Y;

	bool operator == (const Vec2& other)const {
		return other.X == X && other.Y == Y;
	}
	bool operator != (const Vec2& other)const {
		return !(*this == other);
	}

	Vec2 operator +=(const Vec2& other)const {
		return Vec2(X + other.X, Y + other.Y);
	}
	Vec2 operator -=(const Vec2& other)const {
		return Vec2(X - other.X, Y - other.Y);
	}
};


struct Vec3 {
	float X;
	float Y;
	float Z;

	bool operator == (const Vec3& other) const {
		return other.X == X && other.Y == Y && other.Z == Z;
	}
	bool operator != (const Vec3& other) const {
		return !(*this == other);
	}

	Vec3 operator +=(const Vec3& other)const {
		return Vec3(X + other.X, Y + other.Y, Z + other.Z);
	}
	Vec3 operator -=(const Vec3& other) const {
		return Vec3(X - other.X, Y - other.Y, Z - other.Z);
	}
};
