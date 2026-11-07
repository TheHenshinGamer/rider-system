#include "input/InputLayer.h"

#include "core/Keys.h"
#include "core/Log.h"

#include <algorithm>
#include <cmath>

namespace rcats::input
{
	std::string_view ToString(Direction a_dir) noexcept
	{
		switch (a_dir) {
		case Direction::kNeutral: return "Neutral";
		case Direction::kForward: return "Forward";
		case Direction::kBack: return "Back";
		case Direction::kLeft: return "Left";
		case Direction::kRight: return "Right";
		}
		return "?";
	}

	std::string_view ToString(Action a_action) noexcept
	{
		switch (a_action) {
		case Action::kLightAttack: return "LightAttack";
		case Action::kPowerAttack: return "PowerAttack";
		}
		return "?";
	}

	InputLayer::InputLayer(InputConfig a_config, EventBus& a_bus) :
		_config(std::move(a_config)),
		_bus(a_bus)
	{}

	bool InputLayer::Matches(const KeyList& a_list, std::uint32_t a_key) noexcept
	{
		return std::ranges::find(a_list, a_key) != a_list.end();
	}

	std::optional<Direction> InputLayer::DirectionFor(std::uint32_t a_key) const noexcept
	{
		if (Matches(_config.forward, a_key)) return Direction::kForward;
		if (Matches(_config.back, a_key)) return Direction::kBack;
		if (Matches(_config.left, a_key)) return Direction::kLeft;
		if (Matches(_config.right, a_key)) return Direction::kRight;
		return std::nullopt;
	}

	void InputLayer::ExpireStaleHolds(double a_now)
	{
		// The engine re-sends a held button every frame. If those repeats stop without a
		// release, the release was lost (the second in-game test saw Mouse1 "held" for 17
		// minutes and then fire a bogus power attack). Drop such holds silently: no attack.
		constexpr double kStaleSecs = 0.5;
		if (_attackHeld && a_now - _attackLastSeenAt > kStaleSecs) {
			_attackHeld = false;
			_powerFiredThisHold = false;
			log::Debug("Input: attack hold expired (no release seen)");
		}
		// Unlike an attack, a block is a state: a lost release must still end it, or the
		// player would stay "blocking" forever.
		if (_blockHeld && a_now - _blockLastSeenAt > kStaleSecs) {
			log::Debug("Input: block hold expired (no release seen)");
			SetBlock(false, a_now);
		}
		for (auto& key : _dirs) {
			if (key.held && a_now - key.lastSeenAt > kStaleSecs) {
				key.held = false;
				key.releasedAt = -1.0;  // not a real release, so no chord leniency either
			}
		}
	}

	void InputLayer::OnButton(std::uint32_t a_key, bool a_down, double a_now)
	{
		ExpireStaleHolds(a_now);

		if (const auto dir = DirectionFor(a_key)) {
			auto& key = _dirs[static_cast<std::size_t>(*dir)];
			if (a_down) {
				key.lastSeenAt = a_now;
			}
			if (a_down && !key.held) {
				key.held = true;
				key.pressedAt = a_now;
			} else if (!a_down && key.held) {
				key.held = false;
				key.releasedAt = a_now;
			}
			// No return: a key may be bound to both a direction and an action.
		}

		if (Matches(_config.powerAttack, a_key)) {
			if (a_down && !_powerKeyHeld) {
				Emit(Action::kPowerAttack, a_now);
			}
			_powerKeyHeld = a_down;
		}

		if (Matches(_config.block, a_key)) {
			if (a_down) {
				_blockLastSeenAt = a_now;
			}
			if (a_down != _blockHeld) {
				SetBlock(a_down, a_now);
			}
		}

		if (Matches(_config.attack, a_key)) {
			if (a_down) {
				_attackLastSeenAt = a_now;
				if (!_attackHeld) {
					_attackHeld = true;
					_powerFiredThisHold = false;
					_attackPressedAt = a_now;
				}
				Update(a_now);
			} else if (_attackHeld) {
				_attackHeld = false;
				if (!_powerFiredThisHold) {
					Emit(Action::kLightAttack, a_now);
				}
			}
		}
	}

	void InputLayer::OnAnalogButton(std::uint32_t a_key, float a_value, double a_now)
	{
		if (a_key != keys::kGamepadLeftTrigger && a_key != keys::kGamepadRightTrigger) {
			OnButton(a_key, a_value != 0.0f, a_now);
			return;
		}
		auto& down = _triggerDown[a_key == keys::kGamepadRightTrigger ? 1 : 0];
		const float threshold = std::clamp(_config.triggerThreshold, 0.01f, 1.0f);
		if (down) {
			down = a_value >= threshold * 0.5f;
		} else {
			down = a_value >= threshold;
		}
		// Held repeats keep arriving with down == true, which drives hold-to-power as usual.
		OnButton(a_key, down, a_now);
	}

	Direction InputLayer::StickDirection(float a_x, float a_y) const noexcept
	{
		const float magnitude = std::hypot(a_x, a_y);
		if (magnitude < _config.stickDeadzone || magnitude <= 0.0f) {
			return Direction::kNeutral;
		}
		if (std::abs(a_y) >= std::abs(a_x)) {
			return a_y > 0.0f ? Direction::kForward : Direction::kBack;
		}
		return a_x > 0.0f ? Direction::kRight : Direction::kLeft;
	}

	void InputLayer::OnLeftStick(float a_x, float a_y, double a_now)
	{
		// Remember when the stick leaves a direction so flick-then-attack gets the same
		// chord leniency as tapping W then attacking.
		const auto before = StickDirection(_stickX, _stickY);
		if (before != Direction::kNeutral && StickDirection(a_x, a_y) != before) {
			_stickLastDir = before;
			_stickReleasedAt = a_now;
		}
		_stickX = a_x;
		_stickY = a_y;
	}

	void InputLayer::Update(double a_now)
	{
		ExpireStaleHolds(a_now);

		// A dedicated PowerAttack key replaces hold-to-power entirely.
		if (!_config.powerAttack.empty()) {
			return;
		}
		const double holdSecs = _config.powerAttackHoldMs / 1000.0;
		if (_attackHeld && !_powerFiredThisHold && a_now - _attackPressedAt >= holdSecs) {
			_powerFiredThisHold = true;
			Emit(Action::kPowerAttack, a_now);
		}
	}

	Direction InputLayer::CurrentDirection(double a_now) const
	{
		// 1) A held direction key; if several are held, the most recently pressed wins
		//    (so W held + tap D reads as Right, matching what the player last asked for).
		Direction best = Direction::kNeutral;
		double bestTime = -1.0;
		for (std::size_t i = 1; i < _dirs.size(); ++i) {
			if (_dirs[i].held && _dirs[i].pressedAt > bestTime) {
				best = static_cast<Direction>(i);
				bestTime = _dirs[i].pressedAt;
			}
		}
		if (best != Direction::kNeutral) {
			return best;
		}

		// 2) Left stick past the deadzone, dominant axis wins.
		if (const auto stick = StickDirection(_stickX, _stickY); stick != Direction::kNeutral) {
			return stick;
		}

		// 3) Chord leniency: a direction tapped just before the attack still counts, so
		//    "W then PA" in quick succession reads the same as "W+PA" pressed together.
		const double windowSecs = _config.chordWindowMs / 1000.0;
		bestTime = -1.0;
		for (std::size_t i = 1; i < _dirs.size(); ++i) {
			const double released = _dirs[i].releasedAt;
			if (released >= 0.0 && a_now - released <= windowSecs && released > bestTime) {
				best = static_cast<Direction>(i);
				bestTime = released;
			}
		}
		if (_stickReleasedAt >= 0.0 && a_now - _stickReleasedAt <= windowSecs && _stickReleasedAt > bestTime) {
			best = _stickLastDir;
		}
		return best;
	}

	void InputLayer::Emit(Action a_action, double a_now)
	{
		const ActionEvent event{ a_action, CurrentDirection(a_now), a_now };
		_buffered = event;
		log::Debug("Input: {} ({}) at {:.3f}", ToString(event.action), ToString(event.direction), event.time);
		_bus.Publish(event);
	}

	void InputLayer::SetBlock(bool a_active, double a_now)
	{
		_blockHeld = a_active;
		const BlockEvent event{ a_active, CurrentDirection(a_now), a_now };
		if (a_active) {
			log::Debug("Input: Block start ({}) at {:.3f}", ToString(event.direction), event.time);
		} else {
			log::Debug("Input: Block end at {:.3f}", event.time);
		}
		_bus.Publish(event);
	}

	std::optional<ActionEvent> InputLayer::PeekBuffered(double a_now) const
	{
		if (_buffered && a_now - _buffered->time <= _config.bufferWindowMs / 1000.0) {
			return _buffered;
		}
		return std::nullopt;
	}

	std::optional<ActionEvent> InputLayer::ConsumeBuffered(double a_now)
	{
		auto result = PeekBuffered(a_now);
		_buffered.reset();
		return result;
	}
}
