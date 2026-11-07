#pragma once

#include "input/InputLayer.h"

namespace rcats::plugin
{
	// Engine adapter for the Input layer: receives the game's input events, normalises
	// them to flat key codes + a monotonic clock, and feeds InputLayer. It only observes;
	// it never consumes or changes an event, so vanilla controls keep working.
	//
	// Two ways in, picked by [Input] HookPlayerControls:
	//  - PlayerControls hook (default): reads the whole per-frame input queue before the
	//    player's own handlers (attack, jump, sprint...) see it.
	//  - MenuControls handler (fallback): an entry in MenuControls' handler list. The first
	//    in-game tests (AE 1.11.240, 2026-11-07) showed this list never receives the right
	//    trigger during gameplay and can miss releases, so it's only used if the hook can't
	//    be installed.
	class InputSink final : public RE::BSInputEventUser
	{
	public:
		explicit InputSink(input::InputLayer& a_layer) noexcept :
			_layer(a_layer)
		{}

		bool ShouldHandleEvent(const RE::InputEvent* a_event) override;  // MenuControls path
		void ObserveQueue(const RE::InputEvent* a_head);                  // PlayerControls path

		[[nodiscard]] static double Now() noexcept;

	private:
		void Observe(const RE::InputEvent& a_event);

		input::InputLayer& _layer;
		std::mutex _mutex;
		std::unordered_map<std::uint64_t, bool> _lastPressed;  // raw-input log only
	};

	// Hooks PlayerControls::PerformInputProcessing. Returns false (and logs why) if the
	// vtable address isn't in this runtime's Address Library; nothing is patched then.
	bool InstallPlayerControlsHook(input::InputLayer& a_layer);

	// Attaches the sink to MenuControls once the singleton exists. Safe to call more than
	// once: later calls are no-ops. a_when names the trigger, for the log.
	bool RegisterInputSink(input::InputLayer& a_layer, std::string_view a_when);
}
