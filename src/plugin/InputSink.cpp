#include "PCH.h"

#include "InputSink.h"
#include "SafeID.h"

#include "core/Keys.h"
#include "core/Log.h"

namespace rcats::plugin
{
	namespace
	{
		std::optional<std::uint32_t> ToFlatKeyCode(const RE::ButtonEvent& a_event)
		{
			const auto id = a_event.QIDCode();
			switch (a_event.device.get()) {
			case RE::INPUT_DEVICE::kKeyboard:
				// Windows virtual-key code, not a scan code; see keys::FromVirtualKey.
				return keys::FromVirtualKey(id);
			case RE::INPUT_DEVICE::kMouse:
				// Mouse ids are 0-7 for buttons, 8/9 for the wheel: same order as our 0x100 block.
				return id <= 9 ? std::optional{ keys::kMouseOffset + id } : std::nullopt;
			case RE::INPUT_DEVICE::kGamepad:
				return keys::FromGamepadIdCode(id);
			default:
				return std::nullopt;
			}
		}

		std::string_view DeviceName(RE::INPUT_DEVICE a_device) noexcept
		{
			switch (a_device) {
			case RE::INPUT_DEVICE::kKeyboard: return "keyboard";
			case RE::INPUT_DEVICE::kMouse: return "mouse";
			case RE::INPUT_DEVICE::kGamepad: return "gamepad";
			default: return "other";
			}
		}

		// Owned for the game's lifetime; MenuControls / the hook keep raw pointers to it.
		std::unique_ptr<InputSink> g_sink;

		InputSink& GetOrCreateSink(input::InputLayer& a_layer)
		{
			if (!g_sink) {
				g_sink = std::make_unique<InputSink>(a_layer);
			}
			return *g_sink;
		}

		// BSInputEventReceiver::PerformInputProcessing(const InputEvent* queueHead), vtable
		// slot 0. The engine calls it on each input receiver once per frame with the head of
		// that frame's event list (linked through InputEvent::next).
		using PerformInputProcessing_t = void (*)(RE::BSInputEventReceiver*, const RE::InputEvent*);
		PerformInputProcessing_t g_originalPlayerControls{ nullptr };
		bool g_usingPlayerControlsHook{ false };

		void PlayerControlsPerformInputProcessing(RE::BSInputEventReceiver* a_this, const RE::InputEvent* a_head)
		{
			if (g_sink) {
				g_sink->ObserveQueue(a_head);
			}
			g_originalPlayerControls(a_this, a_head);
		}
	}

	double InputSink::Now() noexcept
	{
		using namespace std::chrono;
		return duration<double>(steady_clock::now().time_since_epoch()).count();
	}

	bool InputSink::ShouldHandleEvent(const RE::InputEvent* a_event)
	{
		// MenuControls asks each handler ShouldHandleEvent() and dispatches On*Event to the
		// one that says yes; vanilla handlers (menu open, quicksave, Pip-Boy...) rely on
		// getting that yes. We only want to watch, so we read the event here and always
		// answer false. (Expected from the handler interface; confirm in-game in Phase 1.)
		if (a_event) {
			// The first in-game log (AE 1.11.240, 2026-11-07) showed MenuControls calling us
			// from about seven different threads during gameplay, so the engine walks this
			// handler list from task-pool jobs, not only the main thread. InputLayer and the
			// EventBus are single-threaded, so serialise everything that reaches them.
			const std::scoped_lock lock{ _mutex };
			Observe(*a_event);
		}
		return false;
	}

	void InputSink::ObserveQueue(const RE::InputEvent* a_head)
	{
		const std::scoped_lock lock{ _mutex };
		for (auto event = a_head; event; event = event->next) {
			Observe(*event);
		}
		// Once per frame even when the queue is empty: drives hold-to-power and drops holds
		// whose release we never saw (see InputLayer::Update).
		_layer.Update(Now());
	}

	void InputSink::Observe(const RE::InputEvent& a_event)
	{
		const bool logRaw = _layer.GetConfig().logRawInput;

		if (const auto button = a_event.As<RE::ButtonEvent>()) {
			const auto code = ToFlatKeyCode(*button);
			if (logRaw) {
				// Log each change between pressed and released, tracked per device+id. Not
				// keyed on heldDownSecs == 0: the first trigger event of a press can already
				// carry a non-zero hold time, and those presses went missing from the log.
				const auto device = button->device.get();
				const auto key = (static_cast<std::uint64_t>(device) << 32) | button->QIDCode();
				const bool pressed = button->QPressed();
				auto& last = _lastPressed[key];
				if (pressed != last) {
					last = pressed;
					log::Info("Raw: {} id=0x{:X} value={:.2f} held={:.2f}s -> {} {}",
						DeviceName(device), button->QIDCode(), button->QAnalogValue(), button->QHeldDownSecs(),
						code ? keys::Name(*code) : "unmapped", pressed ? "down" : "up");
				}
				// Every trigger sample at trace level, to see the analog curve of a pull.
				if (code && (*code == keys::kGamepadLeftTrigger || *code == keys::kGamepadRightTrigger)) {
					log::Trace("Raw: {} value={:.2f} held={:.2f}s", keys::Name(*code), button->QAnalogValue(),
						button->QHeldDownSecs());
				}
			}
			if (code) {
				// The engine sends a ButtonEvent every frame while a button is held
				// (heldDownSecs grows), then one with value 0 on release. For the gamepad
				// triggers value is the analog pull, so they go through the threshold path.
				_layer.OnAnalogButton(*code, button->QAnalogValue(), Now());
			}
		} else if (const auto stick = a_event.As<RE::ThumbstickEvent>()) {
			if (stick->QIDCode() == RE::ThumbstickEvent::kLeft) {
				static auto lastLogged = input::Direction::kNeutral;
				_layer.OnLeftStick(stick->xValue, stick->yValue, Now());
				if (logRaw) {
					// Logged on direction change only; the stick reports every frame. If this
					// never prints "Neutral" after letting go, the engine isn't sending a
					// centred event and direction will stick, so tell us.
					const auto after = _layer.StickDirection(stick->xValue, stick->yValue);
					if (after != lastLogged) {
						lastLogged = after;
						log::Info("Raw: left stick x={:.2f} y={:.2f} -> {}", stick->xValue, stick->yValue,
							input::ToString(after));
					}
				}
			}
		}
	}

	bool InstallPlayerControlsHook(input::InputLayer& a_layer)
	{
		if (g_usingPlayerControlsHook) {
			return true;
		}
		// VTABLE::PlayerControls[0] is the primary vtable (the BSInputEventReceiver base sits
		// at offset 0). CommonLibF4 gives it one ID for all runtimes; check it exists here.
		const auto& id = RE::VTABLE::PlayerControls[0];
		const auto vtable = TryAddress(id);
		if (!vtable) {
			log::Warn("Input: PlayerControls vtable (Address Library ID {}) not found on this runtime; "
				"PlayerControls hook not installed", id.id());
			return false;
		}

		GetOrCreateSink(a_layer);
		REL::Relocation<std::uintptr_t> vtbl{ *vtable };
		g_originalPlayerControls = reinterpret_cast<PerformInputProcessing_t>(
			vtbl.write_vfunc(0, &PlayerControlsPerformInputProcessing));
		g_usingPlayerControlsHook = true;
		log::Info("Input: hooked PlayerControls::PerformInputProcessing (vtable ID {})", id.id());
		return true;
	}

	bool RegisterInputSink(input::InputLayer& a_layer, std::string_view a_when)
	{
		if (g_usingPlayerControlsHook) {
			return true;  // the hook already sees everything; don't double-feed the layer
		}
		static bool registered = false;
		if (registered) {
			return true;
		}
		auto controls = RE::MenuControls::GetSingleton();
		if (!controls) {
			log::Error("Input: MenuControls singleton is null on {}; input sink not registered", a_when);
			return false;
		}

		registered = true;
		GetOrCreateSink(a_layer);
		// Insert at the front so we see each event before any vanilla handler can stop the walk.
		controls->handlers.insert(controls->handlers.begin(), static_cast<RE::BSInputEventUser*>(g_sink.get()));
		log::Info("Input: input sink registered with MenuControls on {} ({} handlers)", a_when,
			controls->handlers.size());
		return true;
	}
}
