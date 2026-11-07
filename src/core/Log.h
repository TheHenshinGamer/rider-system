#pragma once

#include <format>
#include <optional>
#include <string_view>
#include <utility>

// Engine-free logging front. Core code logs through here; the plugin installs a
// sink that forwards to CommonLibF4's log (F4SE log folder), tests use the default stderr sink.
namespace rcats::log
{
	enum class Level
	{
		kTrace,
		kDebug,
		kInfo,
		kWarn,
		kError,
		kCritical,
		kOff
	};

	using Sink = void (*)(Level a_level, std::string_view a_message);

	void SetSink(Sink a_sink) noexcept;
	void SetLevel(Level a_level) noexcept;
	[[nodiscard]] Level GetLevel() noexcept;
	[[nodiscard]] bool IsEnabled(Level a_level) noexcept;
	void Write(Level a_level, std::string_view a_message) noexcept;

	[[nodiscard]] std::optional<Level> ParseLevel(std::string_view a_text) noexcept;
	[[nodiscard]] std::string_view ToString(Level a_level) noexcept;

	template <class... Args>
	void Log(Level a_level, std::format_string<Args...> a_fmt, Args&&... a_args)
	{
		if (IsEnabled(a_level)) {
			Write(a_level, std::format(a_fmt, std::forward<Args>(a_args)...));
		}
	}

	template <class... Args>
	void Trace(std::format_string<Args...> a_fmt, Args&&... a_args) { Log(Level::kTrace, a_fmt, std::forward<Args>(a_args)...); }
	template <class... Args>
	void Debug(std::format_string<Args...> a_fmt, Args&&... a_args) { Log(Level::kDebug, a_fmt, std::forward<Args>(a_args)...); }
	template <class... Args>
	void Info(std::format_string<Args...> a_fmt, Args&&... a_args) { Log(Level::kInfo, a_fmt, std::forward<Args>(a_args)...); }
	template <class... Args>
	void Warn(std::format_string<Args...> a_fmt, Args&&... a_args) { Log(Level::kWarn, a_fmt, std::forward<Args>(a_args)...); }
	template <class... Args>
	void Error(std::format_string<Args...> a_fmt, Args&&... a_args) { Log(Level::kError, a_fmt, std::forward<Args>(a_args)...); }
	template <class... Args>
	void Critical(std::format_string<Args...> a_fmt, Args&&... a_args) { Log(Level::kCritical, a_fmt, std::forward<Args>(a_args)...); }
}
