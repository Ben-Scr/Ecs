# Entity Component System
A lightweight C++ 20 Header only Entity Component System

## Features

* Create entities with multiple components at once
* Multi component operations
* Fast, zero copy component queries
* Query filtering with `With<T>()` and `Without<T>()`
* Optional entity free query results with `WithoutEntity()`
* Safe structural changes during iteration with `Stable()`
* Capacity reservation with `Reserve()`
* Component add callbacks with `OnAdd<T>()`
* No external dependencies


## How to use
```cpp
#include <Ecs.hpp>

struct Position {
    float X = 0.0f;
    float Y = 0.0f;
};

struct Velocity {
    float X = 0.0f;
    float Y = 0.0f;
};

int main() {
    Ecs::Registry registry;

    Ecs::Entity entity = registry.Create();

    registry.Add<Position>(entity, 10.0f, 5.0f);
    registry.Add<Velocity>(entity, 1.0f, 0.0f);

    auto& position = registry.Get<Position>(entity);

    return 0;
}
```

## Demo Preview
<img src="Docs/Preview.png" width="48%" alt="Ecs Preview">

## Component add callbacks

`OnAdd<T>()` follows the same naming as `Add<T>()` and lets a registry configure components as soon as they are attached:

```cpp
class Scene {
public:
    void Setup() {
        m_Registry.OnAdd<Transform>()
            .Connect<&Scene::OnTransformAdded>(this);
    }

private:
    void OnTransformAdded(
        Ecs::Registry& registry,
        Ecs::Entity entity,
        Transform& transform)
    {
        transform.X = 10.0f;
    }

    Ecs::Registry m_Registry;
};
```

See `Docs/ComponentEvents.md` for event semantics and the planned lifecycle API.

## Building with Premake

The ECS itself is header-only. Premake generates the project files for the library headers and tests:

```bat
premake5 vs2022
```

or run (defaults to the `vs2022` Premake action):

```bat
Scripts\GenerateProjects.bat
```

You can pass another Premake action as the first argument when needed.

Generated Visual Studio/Premake output is written to `Build/`, `Bin/`, and `Bin-Int/` and is ignored by Git.

The IndexSDK demo is optional and is only generated when a complete SDK exists in `Thirdparty/IndexSDK-Dist-windows-x86_64`:

```bat
premake5 vs2022 --with-demo
```

