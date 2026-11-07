#include "core/Keys.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <format>
#include <utility>

namespace rcats::keys
{
	namespace
	{
		struct NamedKey
		{
			std::string_view name;
			std::uint32_t code;
		};

		// First entry for a code is its canonical display name.
		constexpr auto kNames = std::to_array<NamedKey>({
			{ "Escape", 0x01 }, { "1", 0x02 }, { "2", 0x03 }, { "3", 0x04 }, { "4", 0x05 },
			{ "5", 0x06 }, { "6", 0x07 }, { "7", 0x08 }, { "8", 0x09 }, { "9", 0x0A }, { "0", 0x0B },
			{ "Minus", 0x0C }, { "Equals", 0x0D }, { "Backspace", 0x0E }, { "Tab", 0x0F },
			{ "Q", 0x10 }, { "W", 0x11 }, { "E", 0x12 }, { "R", 0x13 }, { "T", 0x14 },
			{ "Y", 0x15 }, { "U", 0x16 }, { "I", 0x17 }, { "O", 0x18 }, { "P", 0x19 },
			{ "LeftBracket", 0x1A }, { "RightBracket", 0x1B }, { "Enter", 0x1C }, { "LCtrl", 0x1D },
			{ "A", 0x1E }, { "S", 0x1F }, { "D", 0x20 }, { "F", 0x21 }, { "G", 0x22 },
			{ "H", 0x23 }, { "J", 0x24 }, { "K", 0x25 }, { "L", 0x26 },
			{ "Semicolon", 0x27 }, { "Apostrophe", 0x28 }, { "Tilde", 0x29 }, { "LShift", 0x2A },
			{ "Backslash", 0x2B }, { "Z", 0x2C }, { "X", 0x2D }, { "C", 0x2E }, { "V", 0x2F },
			{ "B", 0x30 }, { "N", 0x31 }, { "M", 0x32 }, { "Comma", 0x33 }, { "Period", 0x34 },
			{ "Slash", 0x35 }, { "RShift", 0x36 }, { "LAlt", 0x38 }, { "Space", 0x39 }, { "CapsLock", 0x3A },
			{ "F1", 0x3B }, { "F2", 0x3C }, { "F3", 0x3D }, { "F4", 0x3E }, { "F5", 0x3F },
			{ "F6", 0x40 }, { "F7", 0x41 }, { "F8", 0x42 }, { "F9", 0x43 }, { "F10", 0x44 },
			{ "F11", 0x57 }, { "F12", 0x58 },
			{ "RCtrl", 0x9D }, { "RAlt", 0xB8 },
			{ "Home", 0xC7 }, { "Up", 0xC8 }, { "PageUp", 0xC9 }, { "Left", 0xCB }, { "Right", 0xCD },
			{ "End", 0xCF }, { "Down", 0xD0 }, { "PageDown", 0xD1 }, { "Insert", 0xD2 }, { "Delete", 0xD3 },

			{ "Mouse1", 0x100 }, { "LMB", 0x100 }, { "Mouse2", 0x101 }, { "RMB", 0x101 },
			{ "Mouse3", 0x102 }, { "MMB", 0x102 }, { "Mouse4", 0x103 }, { "Mouse5", 0x104 },
			{ "Mouse6", 0x105 }, { "Mouse7", 0x106 }, { "Mouse8", 0x107 },
			{ "WheelUp", kMouseWheelUp }, { "WheelDown", kMouseWheelDown },

			{ "PadDPadUp", kGamepadDPadUp }, { "PadDPadDown", kGamepadDPadDown },
			{ "PadDPadLeft", kGamepadDPadLeft }, { "PadDPadRight", kGamepadDPadRight },
			{ "PadStart", kGamepadStart }, { "PadBack", kGamepadBack },
			{ "PadLS", kGamepadLeftThumb }, { "PadRS", kGamepadRightThumb },
			{ "PadLB", kGamepadLeftShoulder }, { "PadRB", kGamepadRightShoulder },
			{ "PadA", kGamepadA }, { "PadB", kGamepadB }, { "PadX", kGamepadX }, { "PadY", kGamepadY },
			{ "PadLT", kGamepadLeftTrigger }, { "PadRT", kGamepadRightTrigger },
		});

		bool EqualsIgnoreCase(std::string_view a, std::string_view b)
		{
			return std::ranges::equal(a, b, [](unsigned char x, unsigned char y) {
				return std::tolower(x) == std::tolower(y);
			});
		}

		std::string_view Trim(std::string_view a_text)
		{
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.front()))) a_text.remove_prefix(1);
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.back()))) a_text.remove_suffix(1);
			return a_text;
		}
	}

	std::optional<std::uint32_t> FromGamepadIdCode(std::uint32_t a_idCode) noexcept
	{
		// XINPUT_GAMEPAD_* bitmask -> flat code. Bits 0x0400/0x0800 are unused by XInput.
		constexpr std::array<std::pair<std::uint32_t, std::uint32_t>, 14> kMasks{ {
			{ 0x0001, kGamepadDPadUp }, { 0x0002, kGamepadDPadDown },
			{ 0x0004, kGamepadDPadLeft }, { 0x0008, kGamepadDPadRight },
			{ 0x0010, kGamepadStart }, { 0x0020, kGamepadBack },
			{ 0x0040, kGamepadLeftThumb }, { 0x0080, kGamepadRightThumb },
			{ 0x0100, kGamepadLeftShoulder }, { 0x0200, kGamepadRightShoulder },
			{ 0x1000, kGamepadA }, { 0x2000, kGamepadB }, { 0x4000, kGamepadX }, { 0x8000, kGamepadY },
		} };

		// The engine sends analog triggers as button events with these ids
		// (CommonLibF4 F4SE::Inputs::XInput::kGameButton_LeftTrigger / _RightTrigger).
		if (a_idCode == 0x09) return kGamepadLeftTrigger;
		if (a_idCode == 0x0A) return kGamepadRightTrigger;

		for (const auto& [mask, code] : kMasks) {
			if (a_idCode == mask) return code;
		}
		return std::nullopt;
	}

	std::optional<std::uint32_t> FromVirtualKey(std::uint32_t a_vk) noexcept
	{
		// VK -> DIK scan code, sorted by VK. Positions follow the US layout, matching how
		// DIK codes (and our INI key names) are defined. Extended keys (arrows, nav block,
		// right-hand modifiers, numpad /) use DIK's 0x80 | scan form.
		struct Mapping
		{
			std::uint32_t vk;
			std::uint32_t dik;
		};
		static constexpr auto kTable = std::to_array<Mapping>({
			{ 0x08, 0x0E }, { 0x09, 0x0F }, { 0x0D, 0x1C }, { 0x10, 0x2A }, { 0x11, 0x1D }, { 0x12, 0x38 },
			{ 0x13, 0xC5 }, { 0x14, 0x3A }, { 0x1B, 0x01 }, { 0x20, 0x39 }, { 0x21, 0xC9 }, { 0x22, 0xD1 },
			{ 0x23, 0xCF }, { 0x24, 0xC7 }, { 0x25, 0xCB }, { 0x26, 0xC8 }, { 0x27, 0xCD }, { 0x28, 0xD0 },
			{ 0x2C, 0xB7 }, { 0x2D, 0xD2 }, { 0x2E, 0xD3 }, { 0x30, 0x0B }, { 0x31, 0x02 }, { 0x32, 0x03 },
			{ 0x33, 0x04 }, { 0x34, 0x05 }, { 0x35, 0x06 }, { 0x36, 0x07 }, { 0x37, 0x08 }, { 0x38, 0x09 },
			{ 0x39, 0x0A }, { 0x41, 0x1E }, { 0x42, 0x30 }, { 0x43, 0x2E }, { 0x44, 0x20 }, { 0x45, 0x12 },
			{ 0x46, 0x21 }, { 0x47, 0x22 }, { 0x48, 0x23 }, { 0x49, 0x17 }, { 0x4A, 0x24 }, { 0x4B, 0x25 },
			{ 0x4C, 0x26 }, { 0x4D, 0x32 }, { 0x4E, 0x31 }, { 0x4F, 0x18 }, { 0x50, 0x19 }, { 0x51, 0x10 },
			{ 0x52, 0x13 }, { 0x53, 0x1F }, { 0x54, 0x14 }, { 0x55, 0x16 }, { 0x56, 0x2F }, { 0x57, 0x11 },
			{ 0x58, 0x2D }, { 0x59, 0x15 }, { 0x5A, 0x2C }, { 0x5B, 0xDB }, { 0x5C, 0xDC }, { 0x5D, 0xDD },
			{ 0x60, 0x52 }, { 0x61, 0x4F }, { 0x62, 0x50 }, { 0x63, 0x51 }, { 0x64, 0x4B }, { 0x65, 0x4C },
			{ 0x66, 0x4D }, { 0x67, 0x47 }, { 0x68, 0x48 }, { 0x69, 0x49 }, { 0x6A, 0x37 }, { 0x6B, 0x4E },
			{ 0x6D, 0x4A }, { 0x6E, 0x53 }, { 0x6F, 0xB5 }, { 0x70, 0x3B }, { 0x71, 0x3C }, { 0x72, 0x3D },
			{ 0x73, 0x3E }, { 0x74, 0x3F }, { 0x75, 0x40 }, { 0x76, 0x41 }, { 0x77, 0x42 }, { 0x78, 0x43 },
			{ 0x79, 0x44 }, { 0x7A, 0x57 }, { 0x7B, 0x58 }, { 0x90, 0x45 }, { 0x91, 0x46 }, { 0xA0, 0x2A },
			{ 0xA1, 0x36 }, { 0xA2, 0x1D }, { 0xA3, 0x9D }, { 0xA4, 0x38 }, { 0xA5, 0xB8 }, { 0xBA, 0x27 },
			{ 0xBB, 0x0D }, { 0xBC, 0x33 }, { 0xBD, 0x0C }, { 0xBE, 0x34 }, { 0xBF, 0x35 }, { 0xC0, 0x29 },
			{ 0xDB, 0x1A }, { 0xDC, 0x2B }, { 0xDD, 0x1B }, { 0xDE, 0x28 },
		});
		static_assert(std::ranges::is_sorted(kTable, {}, &Mapping::vk));

		const auto it = std::ranges::lower_bound(kTable, a_vk, {}, &Mapping::vk);
		if (it != kTable.end() && it->vk == a_vk) {
			return it->dik;
		}
		return std::nullopt;
	}

	std::optional<std::uint32_t> Parse(std::string_view a_text)
	{
		const auto text = Trim(a_text);
		if (text.empty()) {
			return std::nullopt;
		}

		for (const auto& entry : kNames) {
			if (EqualsIgnoreCase(entry.name, text)) {
				return entry.code;
			}
		}

		// Numeric fallback: "0x11" or "17".
		std::uint32_t value = 0;
		const char* first = text.data();
		const char* last = text.data() + text.size();
		int base = 10;
		if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
			first += 2;
			base = 16;
		}
		const auto [ptr, ec] = std::from_chars(first, last, value, base);
		if (ec != std::errc{} || ptr != last || value > kMaxCode) {
			return std::nullopt;
		}
		return value;
	}

	std::string Name(std::uint32_t a_code)
	{
		for (const auto& entry : kNames) {
			if (entry.code == a_code) {
				return std::string(entry.name);
			}
		}
		return std::format("0x{:X}", a_code);
	}
}
