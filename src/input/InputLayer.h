#pragma once

#include "core/Config.h"
#include "core/EventBus.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace rcats::input
{
	enum class Direction : std::uint8_t
	{
		kNeutral,
		kForward,
		kBack,
		kLeft,
		kRight
	};

	enum class Action : std::uint8_t
	{
		kLightAttack,
		kPowerAttack
	};

	// Published when Block is pressed (active = true) and when it ends (false). Block is a
	// held state, not a one-shot action, so it bypasses the action buffer.
	struct BlockEvent
	{
		bool active;
		Direction direction;  // held direction when the block started (later: directional guard)
		double time;
	};

	[[nodiscard]] std::string_view ToString(Direction a_dir) noexcept;
	[[nodiscard]] std::string_view ToString(Action a_action) noexcept;

	// Published on the EventBus the moment an action is recognised.
	struct ActionEvent
	{
		Action action;
		Direction direction;  // W+PA = kPowerAttack/kForward, S+PA = .../kBack, etc.
		double time;          // seconds, same clock the sink feeds in
	};

	// Turns raw button / stick samples into directional attack actions, and holds the
	// most recent one in a buffer for Combo to consume when a cancel window opens.
	//
	// Engine-free: the plugin's InputSink normalises engine events into flat key codes
	// (core/Keys.h) and a monotonic time, so this whole layer is unit-testable.
	//
	// Attack semantics: a quick tap of Attack is a light attack (fired on release); holding
	// it past PowerAttackHoldMs fires a power attack immediately, and the release is then
	// swallowed. A dedicated PowerAttack key, if bound, fires a power attack on press.
	class InputLayer
	{
	public:
		InputLayer(InputConfig a_config, EventBus& a_bus);

		// a_down: true for press and held-repeat events, false for release.
		void OnButton(std::uint32_t a_key, bool a_down, double a_now);
		// Analog buttons (gamepad triggers), value in [0, 1]. Applies TriggerThreshold with
		// hysteresis so a trigger resting near the threshold can't chatter into extra attacks.
		void OnAnalogButton(std::uint32_t a_key, float a_value, double a_now);
		// Left stick, each axis in [-1, 1], +y = forward.
		void OnLeftStick(float a_x, float a_y, double a_now);
		// Call once per frame: turns a held Attack into a power attack, and drops any hold
		// whose repeat events stopped arriving (its release was lost, e.g. to a menu).
		void Update(double a_now);

		[[nodiscard]] Direction CurrentDirection(double a_now) const;
		// What a stick sample reads as on its own (deadzone + dominant axis).
		[[nodiscard]] Direction StickDirection(float a_x, float a_y) const noexcept;

		// Latest action still inside BufferWindowMs; latest wins, so mashing doesn't queue up.
		[[nodiscard]] std::optional<ActionEvent> PeekBuffered(double a_now) const;
		std::optional<ActionEvent> ConsumeBuffered(double a_now);

		[[nodiscard]] bool IsBlocking() const noexcept { return _blockHeld; }

		[[nodiscard]] const InputConfig& GetConfig() const noexcept { return _config; }

	private:
		struct DirKey
		{
			bool held{ false };
			double pressedAt{ -1.0 };
			double releasedAt{ -1.0 };
			double lastSeenAt{ -1.0 };
		};

		void ExpireStaleHolds(double a_now);

		static bool Matches(const KeyList& a_list, std::uint32_t a_key) noexcept;
		std::optional<Direction> DirectionFor(std::uint32_t a_key) const noexcept;
		void Emit(Action a_action, double a_now);
		void SetBlock(bool a_active, double a_now);

		InputConfig _config;
		EventBus& _bus;

		std::array<DirKey, 5> _dirs{};  // indexed by Direction; kNeutral unused
		float _stickX{ 0.0f };
		float _stickY{ 0.0f };
		Direction _stickLastDir{ Direction::kNeutral };  // direction the stick last left
		double _stickReleasedAt{ -1.0 };

		std::array<bool, 2> _triggerDown{};  // LT, RT

		bool _attackHeld{ false };
		bool _powerFiredThisHold{ false };
		double _attackPressedAt{ 0.0 };
		double _attackLastSeenAt{ 0.0 };
		bool _powerKeyHeld{ false };

		bool _blockHeld{ false };
		double _blockLastSeenAt{ 0.0 };

		std::optional<ActionEvent> _buffered;
	};
}
