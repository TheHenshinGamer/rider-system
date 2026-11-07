#pragma once

#include "core/Log.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace rcats
{
	enum class LayerId : std::uint8_t
	{
		kInput,
		kAnimReplacer,
		kCombo,
		kHitDetect,
		kImpact,
		kMovement,
		kParkour,
		kSkills,
		kTransform,

		kCount
	};

	inline constexpr std::size_t kLayerCount = static_cast<std::size_t>(LayerId::kCount);

	[[nodiscard]] std::string_view ToString(LayerId a_id) noexcept;
	[[nodiscard]] std::optional<LayerId> ParseLayerId(std::string_view a_text) noexcept;

	using KeyList = std::vector<std::uint32_t>;

	struct InputConfig
	{
		// Each binding accepts several keys so keyboard and gamepad can coexist.
		KeyList forward{ 0x11 };  // W
		KeyList back{ 0x1F };     // S
		KeyList left{ 0x1E };     // A
		KeyList right{ 0x20 };    // D
		KeyList attack{ 0x100, 0x119 };  // Mouse1, PadRT
		KeyList powerAttack{};    // empty: power attack = hold Attack past the threshold
	KeyList block{ 0x101, 0x118 };  // Mouse2, PadLT (vanilla's block / aim buttons)

		float powerAttackHoldMs{ 300.0f };  // Attack held this long becomes a power attack
		float chordWindowMs{ 120.0f };      // a direction released this recently still counts
		float bufferWindowMs{ 250.0f };     // how long an action waits for Combo to consume it
		float stickDeadzone{ 0.35f };       // left stick magnitude below this is neutral
		// Gamepad triggers are analog (0-1) and report a non-zero value at the lightest touch,
		// so a trigger counts as pressed at this value and released below half of it.
		float triggerThreshold{ 0.5f };
		bool logRawInput{ false };          // log every press/release and stick move (diagnostics)
		bool hookPlayerControls{ true };    // read input via the PlayerControls hook (else MenuControls)
	};

	struct Config
	{
		log::Level logLevel{ log::Level::kInfo };
		std::array<bool, kLayerCount> layers{ []() {
			std::array<bool, kLayerCount> all{};
			all.fill(true);
			return all;
		}() };
		InputConfig input{};

		[[nodiscard]] bool IsEnabled(LayerId a_id) const noexcept { return layers[static_cast<std::size_t>(a_id)]; }
	};

	struct ConfigLoadResult
	{
		Config config{};
		bool fileFound{ false };
		std::vector<std::string> warnings{};  // logged by the caller once logging is up
	};

	// Unknown keys and bad values are reported as warnings and fall back to defaults;
	// a broken INI never stops the plugin from loading.
	[[nodiscard]] ConfigLoadResult ParseConfig(std::string_view a_text);
	[[nodiscard]] ConfigLoadResult LoadConfig(const std::filesystem::path& a_path);
}
