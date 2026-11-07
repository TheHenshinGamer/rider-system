#pragma once

#include <cstdint>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace rcats
{
	// Internal event bus: layers publish plain structs and subscribe by type, so no layer
	// reaches into another. Synchronous and single-threaded by contract: publish and
	// subscribe only from the game's main thread (where input and messaging callbacks run).
	class EventBus
	{
	public:
		using Token = std::uint64_t;

		template <class E>
		Token Subscribe(std::function<void(const E&)> a_handler)
		{
			const auto token = _nextToken++;
			_handlers[typeid(E)].push_back({ token,
				[fn = std::move(a_handler)](const void* a_event) { fn(*static_cast<const E*>(a_event)); } });
			return token;
		}

		void Unsubscribe(Token a_token);

		template <class E>
		void Publish(const E& a_event) const
		{
			Dispatch(typeid(E), &a_event);
		}

		[[nodiscard]] std::size_t SubscriberCount() const noexcept;

	private:
		struct Handler
		{
			Token token;
			std::function<void(const void*)> fn;
		};

		void Dispatch(std::type_index a_type, const void* a_event) const;

		std::unordered_map<std::type_index, std::vector<Handler>> _handlers;
		Token _nextToken{ 1 };
	};
}
