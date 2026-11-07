#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// One flat key-code space for every device, the same layout F4SE/SKSE plugins
// conventionally use so INI values are familiar to users of other mods:
//   0x000-0x0FF  keyboard DirectInput scan codes (DIK_*)
//   0x100-0x107  mouse buttons 1-8
//   0x108-0x109  mouse wheel up / down
//   0x10A-0x119  gamepad buttons (XInput order, see kGamepad* below)
namespace rcats::keys
{
	inline constexpr std::uint32_t kNone = 0;

	inline constexpr std::uint32_t kMouseOffset = 0x100;
	inline constexpr std::uint32_t kMouseWheelUp = 0x108;
	inline constexpr std::uint32_t kMouseWheelDown = 0x109;
	inline constexpr std::uint32_t kGamepadOffset = 0x10A;

	inline constexpr std::uint32_t kGamepadDPadUp = kGamepadOffset + 0;
	inline constexpr std::uint32_t kGamepadDPadDown = kGamepadOffset + 1;
	inline constexpr std::uint32_t kGamepadDPadLeft = kGamepadOffset + 2;
	inline constexpr std::uint32_t kGamepadDPadRight = kGamepadOffset + 3;
	inline constexpr std::uint32_t kGamepadStart = kGamepadOffset + 4;
	inline constexpr std::uint32_t kGamepadBack = kGamepadOffset + 5;
	inline constexpr std::uint32_t kGamepadLeftThumb = kGamepadOffset + 6;
	inline constexpr std::uint32_t kGamepadRightThumb = kGamepadOffset + 7;
	inline constexpr std::uint32_t kGamepadLeftShoulder = kGamepadOffset + 8;
	inline constexpr std::uint32_t kGamepadRightShoulder = kGamepadOffset + 9;
	inline constexpr std::uint32_t kGamepadA = kGamepadOffset + 10;
	inline constexpr std::uint32_t kGamepadB = kGamepadOffset + 11;
	inline constexpr std::uint32_t kGamepadX = kGamepadOffset + 12;
	inline constexpr std::uint32_t kGamepadY = kGamepadOffset + 13;
	inline constexpr std::uint32_t kGamepadLeftTrigger = kGamepadOffset + 14;
	inline constexpr std::uint32_t kGamepadRightTrigger = kGamepadOffset + 15;
	inline constexpr std::uint32_t kMaxCode = kGamepadRightTrigger;

	// The engine reports gamepad buttons as XInput bitmasks (XINPUT_GAMEPAD_*), and the
	// triggers as the pseudo-codes 0x09 / 0x0A. Converts either form to our flat code.
	[[nodiscard]] std::optional<std::uint32_t> FromGamepadIdCode(std::uint32_t a_idCode) noexcept;

	// Fallout 4 reports keyboard ButtonEvents with Windows virtual-key codes (VK_*), not
	// DirectInput scan codes: in-game logs on AE 1.11.240 showed Up arrow as 0x26 (VK_UP).
	// CommonLibF4's BS_BUTTON_CODE enum lists the same VK values. Converts to our flat
	// space (DIK scan codes, by physical US-layout key position). nullopt if unknown.
	[[nodiscard]] std::optional<std::uint32_t> FromVirtualKey(std::uint32_t a_vk) noexcept;

	// Accepts names ("W", "Mouse1", "LMB", "PadRT", "Space"...) or numbers ("0x11", "17").
	[[nodiscard]] std::optional<std::uint32_t> Parse(std::string_view a_text);
	[[nodiscard]] std::string Name(std::uint32_t a_code);
}
