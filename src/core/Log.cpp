#include "core/Log.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <string>

namespace rcats::log
{
	namespace
	{
		void StderrSink(Level a_level, std::string_view a_message)
		{
			const auto tag = ToString(a_level);
			std::fprintf(stderr, "[%.*s] %.*s\n",
				static_cast<int>(tag.size()), tag.data(),
				static_cast<int>(a_message.size()), a_message.data());
		}

		std::atomic<Sink> g_sink{ &StderrSink };
		std::atomic<Level> g_level{ Level::kInfo };
	}

	void SetSink(Sink a_sink) noexcept { g_sink = a_sink ? a_sink : &StderrSink; }
	void SetLevel(Level a_level) noexcept { g_level = a_level; }
	Level GetLevel() noexcept { return g_level; }

	bool IsEnabled(Level a_level) noexcept
	{
		return a_level != Level::kOff && a_level >= g_level.load();
	}

	void Write(Level a_level, std::string_view a_message) noexcept
	{
		if (IsEnabled(a_level)) {
			g_sink.load()(a_level, a_message);
		}
	}

	std::optional<Level> ParseLevel(std::string_view a_text) noexcept
	{
		std::string lower(a_text);
		std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (lower == "trace") return Level::kTrace;
		if (lower == "debug") return Level::kDebug;
		if (lower == "info") return Level::kInfo;
		if (lower == "warn" || lower == "warning") return Level::kWarn;
		if (lower == "error") return Level::kError;
		if (lower == "critical") return Level::kCritical;
		if (lower == "off") return Level::kOff;
		return std::nullopt;
	}

	std::string_view ToString(Level a_level) noexcept
	{
		switch (a_level) {
		case Level::kTrace: return "trace";
		case Level::kDebug: return "debug";
		case Level::kInfo: return "info";
		case Level::kWarn: return "warn";
		case Level::kError: return "error";
		case Level::kCritical: return "critical";
		case Level::kOff: return "off";
		}
		return "?";
	}
}
