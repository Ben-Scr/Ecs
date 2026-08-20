# Entity Component System
A C++ Entity Component System

## Usage
```cpp
#include <Ecs.hpp>
```

```cpp
Ecs::Registry registry = {};
Entity entity = registry.Create();
registry.Add<TransformComponent>(entity);
```
