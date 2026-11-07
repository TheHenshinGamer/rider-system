#include "core/Config.h"

#include "core/Keys.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <format>
#include <fstream>
#include <sstream>

namespace rcats
{
	namespace
	{
		constexpr std::array<std::string_view, kLayerCount> kLayerNames{
			"Input", "AnimReplacer", "Combo", "HitDetect", "Impact",
			"Movement", "Parkour", "Skills", "Transform"
		};

		std::string_view Trim(std::string_view a_text)
		{
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.front()))) a_text.remove_prefix(1);
			while (!a_text.empty() && std::isspace(static_cast<unsigned char>(a_text.back()))) a_text.remove_suffix(1);
			return a_text;
		}

		bool EqualsIgnoreCase(std::string_view a, std::string_view b)
		{
			return std::ranges::equal(a, b, [](unsigned char x, unsigned char y) {
				return std::tolower(x) == std::tolower(y);
			});
		}

		std::optional<bool> ParseBool(std::string_view a_text)
		{
			for (auto t : { "1", "true", "on", "yes" }) {
				if (EqualsIgnoreCase(a_text, t)) return true;
			}
			for (auto f : { "0", "false", "off", "no" }) {
				if (EqualsIgnoreCase(a_text, f)) return false;
			}
			return std::nullopt;
		}

		std::optional<float> ParseFloat(std::string_view a_text)
		{
			float value = 0.0f;
			const auto [ptr, ec] = std::from_chars(a_text.data(), a_text.data() + a_text.size(), value);
			if (ec != std::errc{} || ptr != a_text.data() + a_text.size()) {
				return std::nullopt;
			}
			return value;
		}

		std::optional<KeyList> ParseKeyList(std::string_view a_text)
		{
			KeyList keys;
			if (EqualsIgnoreCase(a_text, "none") || a_text.empty()) {
				return keys;
			}
			while (!a_text.empty()) {
				const auto comma = a_text.find(',');
				const auto item = Trim(a_text.substr(0, comma));
				const auto code = keys::Parse(item);
				if (!code) {
					return std::nullopt;
				}
				keys.push_back(*code);
				a_text = comma == std::string_view::npos ? std::string_view{} : a_text.substr(comma + 1);
			}
			return keys;
		}

		class Parser
		{
		public:
			explicit Parser(ConfigLoadResult& a_result) :
				_result(a_result)
			{}

			void Line(std::size_t a_lineNo, std::string_view a_line)
			{
				_lineNo = a_lineNo;

				// ';' and '#' start a comment anywhere on the line.
				const auto comment = a_line.find_first_of(";#");
				auto line = Trim(a_line.substr(0, comment));
				if (line.empty()) {
					return;
				}

				if (line.front() == '[') {
					if (line.back() != ']') {
						Warn(std::format("malformed section header '{}'", line));
						return;
					}
					_section = std::string(Trim(line.substr(1, line.size() - 2)));
					return;
				}

				const auto eq = line.find('=');
				if (eq == std::string_view::npos) {
					Warn(std::format("expected key=value, got '{}'", line));
					return;
				}
				Value(Trim(line.substr(0, eq)), Trim(line.substr(eq + 1)));
			}

		private:
			void Warn(std::string a_message)
			{
				_result.warnings.push_back(std::format("line {}: {}", _lineNo, a_message));
			}

			void BadValue(std::string_view a_key, std::string_view a_value)
			{
				Warn(std::format("[{}] {}: invalid value '{}', keeping default", _section, a_key, a_value));
			}

			void Value(std::string_view a_key, std::string_view a_value)
			{
				auto& cfg = _result.config;

				if (EqualsIgnoreCase(_section, "General")) {
					if (EqualsIgnoreCase(a_key, "LogLevel")) {
						if (const auto level = log::ParseLevel(a_value)) cfg.logLevel = *level;
						else BadValue(a_key, a_value);
						return;
					}
				} else if (EqualsIgnoreCase(_section, "Layers")) {
					if (const auto id = ParseLayerId(a_key)) {
						if (const auto on = ParseBool(a_value)) cfg.layers[static_cast<std::size_t>(*id)] = *on;
						else BadValue(a_key, a_value);
						return;
					}
				} else if (EqualsIgnoreCase(_section, "Input")) {
					auto& in = cfg.input;
					const std::array<std::pair<std::string_view, KeyList*>, 7> bindings{ {
						{ "Forward", &in.forward }, { "Back", &in.back },
						{ "Left", &in.left }, { "Right", &in.right },
						{ "Attack", &in.attack }, { "PowerAttack", &in.powerAttack },
						{ "Block", &in.block },
					} };
					for (const auto& [name, list] : bindings) {
						if (EqualsIgnoreCase(a_key, name)) {
							if (auto keys = ParseKeyList(a_value)) *list = std::move(*keys);
							else BadValue(a_key, a_value);
							return;
						}
					}

					const std::array<std::pair<std::string_view, float*>, 5> numbers{ {
						{ "PowerAttackHoldMs", &in.powerAttackHoldMs },
						{ "ChordWindowMs", &in.chordWindowMs },
						{ "BufferWindowMs", &in.bufferWindowMs },
						{ "StickDeadzone", &in.stickDeadzone },
						{ "TriggerThreshold", &in.triggerThreshold },
					} };
					const std::array<std::pair<std::string_view, bool*>, 2> flags{ {
						{ "LogRawInput", &in.logRawInput },
						{ "HookPlayerControls", &in.hookPlayerControls },
					} };
					for (const auto& [name, field] : flags) {
						if (EqualsIgnoreCase(a_key, name)) {
							if (const auto on = ParseBool(a_value)) *field = *on;
							else BadValue(a_key, a_value);
							return;
						}
					}

					for (const auto& [name, field] : numbers) {
						if (EqualsIgnoreCase(a_key, name)) {
							const auto value = ParseFloat(a_value);
							if (value && *value >= 0.0f) *field = *value;
							else BadValue(a_key, a_value);
							return;
						}
					}
				}

				Warn(std::format("unknown setting [{}] {}", _section, a_key));
			}

			ConfigLoadResult& _result;
			std::string _section;
			std::size_t _lineNo{ 0 };
		};
	}

	std::string_view ToString(LayerId a_id) noexcept
	{
		const auto index = static_cast<std::size_t>(a_id);
		return index < kLayerCount ? kLayerNames[index] : "?";
	}

	std::optional<LayerId> ParseLayerId(std::string_view a_text) noexcept
	{
		for (std::size_t i = 0; i < kLayerCount; ++i) {
			if (EqualsIgnoreCase(kLayerNames[i], a_text)) {
				return static_cast<LayerId>(i);
			}
		}
		return std::nullopt;
	}

	ConfigLoadResult ParseConfig(std::string_view a_text)
	{
		ConfigLoadResult result;
		result.fileFound = true;
		Parser parser(result);

		std::size_t lineNo = 0;
		while (!a_text.empty()) {
			++lineNo;
			const auto nl = a_text.find('\n');
			auto line = a_text.substr(0, nl);
			if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
			parser.Line(lineNo, line);
			a_text = nl == std::string_view::npos ? std::string_view{} : a_text.substr(nl + 1);
		}
		return result;
	}

	ConfigLoadResult LoadConfig(const std::filesystem::path& a_path)
	{
		std::ifstream file(a_path, std::ios::binary);
		if (!file) {
			ConfigLoadResult result;
			result.warnings.push_back(std::format("{} not found, using defaults", a_path.string()));
			return result;
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		return ParseConfig(buffer.str());
	}
}
