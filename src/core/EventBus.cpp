#include "core/EventBus.h"

#include "core/Log.h"

#include <exception>

namespace rcats
{
	void EventBus::Unsubscribe(Token a_token)
	{
		for (auto& [type, handlers] : _handlers) {
			std::erase_if(handlers, [&](const Handler& h) { return h.token == a_token; });
		}
	}

	std::size_t EventBus::SubscriberCount() const noexcept
	{
		std::size_t count = 0;
		for (const auto& [type, handlers] : _handlers) {
			count += handlers.size();
		}
		return count;
	}

	void EventBus::Dispatch(std::type_index a_type, const void* a_event) const
	{
		const auto it = _handlers.find(a_type);
		if (it == _handlers.end()) {
			return;
		}

		// Copy so a handler may subscribe/unsubscribe during dispatch without
		// invalidating the iteration.
		const auto handlers = it->second;
		for (const auto& handler : handlers) {
			// One misbehaving subscriber must not take down the others (or the game).
			try {
				handler.fn(a_event);
			} catch (const std::exception& e) {
				log::Error("EventBus: handler for {} threw: {}", a_type.name(), e.what());
			} catch (...) {
				log::Error("EventBus: handler for {} threw a non-standard exception", a_type.name());
			}
		}
	}
}
