#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "Entity.hpp"

namespace Ecs {

	class Registry;

	// A lightweight component-specific event signal.
	// Callbacks use the canonical signature:
	//     void(Registry&, Entity, T&)
	template<typename T>
	class ComponentSignal {
	public:
		template<auto Callback, typename Instance>
		bool Connect(Instance* instance) {
			static_assert(
				std::is_invocable_r_v<
					void,
					decltype(Callback),
					Instance&,
					Registry&,
					Entity,
					T&
				>,
				"Component callback must have the signature "
				"void(Registry&, Entity, T&)"
			);

			if (!instance)
				throw std::invalid_argument("Component callback instance cannot be null");

			const Connection connection{
				CallbackId<Callback>(),
				instance,
				[](void* object, Registry& registry, Entity ent, T& component) {
					std::invoke(
						Callback,
						*static_cast<Instance*>(object),
						registry,
						ent,
						component
					);
				}
			};

			if (Contains(connection))
				return false;

			m_Connections.push_back(connection);
			return true;
		}

		template<auto Callback>
		bool Connect() {
			static_assert(
				std::is_invocable_r_v<
					void,
					decltype(Callback),
					Registry&,
					Entity,
					T&
				>,
				"Component callback must have the signature "
				"void(Registry&, Entity, T&)"
			);

			const Connection connection{
				CallbackId<Callback>(),
				nullptr,
				[](void*, Registry& registry, Entity ent, T& component) {
					std::invoke(Callback, registry, ent, component);
				}
			};

			if (Contains(connection))
				return false;

			m_Connections.push_back(connection);
			return true;
		}

		template<auto Callback, typename Instance>
		bool Disconnect(Instance* instance) noexcept {
			if (!instance)
				return false;

			return Disconnect(CallbackId<Callback>(), instance);
		}

		template<auto Callback>
		bool Disconnect() noexcept {
			return Disconnect(CallbackId<Callback>(), nullptr);
		}

		void DisconnectAll() noexcept {
			m_Connections.clear();
		}

		std::size_t GetConnectionCount() const noexcept {
			return m_Connections.size();
		}

	private:
		friend class Registry;

		struct Connection {
			const void* Callback = nullptr;
			void* Instance = nullptr;
			void (*Invoke)(void*, Registry&, Entity, T&) = nullptr;
		};

		template<auto Callback>
		static const void* CallbackId() noexcept {
			static const char id = 0;
			return &id;
		}

		bool Contains(const Connection& connection) const noexcept {
			return std::any_of(
				m_Connections.begin(),
				m_Connections.end(),
				[&](const Connection& current) {
					return current.Callback == connection.Callback &&
						current.Instance == connection.Instance;
				}
			);
		}

		bool Disconnect(const void* callback, void* instance) noexcept {
			const auto it = std::find_if(
				m_Connections.begin(),
				m_Connections.end(),
				[&](const Connection& connection) {
					return connection.Callback == callback &&
						connection.Instance == instance;
				}
			);

			if (it == m_Connections.end())
				return false;

			m_Connections.erase(it);
			return true;
		}

		void Publish(Registry& registry, Entity ent, T& component) {
			// Connections are invoked in registration order. Connecting or
			// disconnecting this same signal from inside a callback is unsupported.
			for (const Connection& connection : m_Connections)
				connection.Invoke(connection.Instance, registry, ent, component);
		}

	private:
		std::vector<Connection> m_Connections;
	};
}
