#pragma once

#include "core/Config.h"

#include <array>
#include <functional>

namespace rcats
{
	enum class LayerState : std::uint8_t
	{
		kNotRegistered,
		kDisabledByConfig,
		kFailed,
		kActive
	};

	[[nodiscard]] std::string_view ToString(LayerState a_state) noexcept;

	// Installs each layer if its config flag allows it. A layer whose install returns false
	// or throws is logged and left inactive; the rest keep going (fail safe, never crash).
	class LayerRegistry
	{
	public:
		using InstallFn = std::function<bool()>;

		void Register(LayerId a_id, InstallFn a_install);
		void InstallAll(const Config& a_config);

		[[nodiscard]] LayerState GetState(LayerId a_id) const noexcept;
		[[nodiscard]] bool IsActive(LayerId a_id) const noexcept { return GetState(a_id) == LayerState::kActive; }

	private:
		std::array<InstallFn, kLayerCount> _install{};
		std::array<LayerState, kLayerCount> _state{};
	};
}
