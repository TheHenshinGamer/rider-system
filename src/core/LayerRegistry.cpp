#include "core/LayerRegistry.h"

#include "core/Log.h"

#include <exception>

namespace rcats
{
	std::string_view ToString(LayerState a_state) noexcept
	{
		switch (a_state) {
		case LayerState::kNotRegistered: return "not implemented";
		case LayerState::kDisabledByConfig: return "disabled by config";
		case LayerState::kFailed: return "FAILED";
		case LayerState::kActive: return "active";
		}
		return "?";
	}

	void LayerRegistry::Register(LayerId a_id, InstallFn a_install)
	{
		_install[static_cast<std::size_t>(a_id)] = std::move(a_install);
	}

	void LayerRegistry::InstallAll(const Config& a_config)
	{
		for (std::size_t i = 0; i < kLayerCount; ++i) {
			const auto id = static_cast<LayerId>(i);
			auto& state = _state[i];

			if (!_install[i]) {
				state = LayerState::kNotRegistered;
			} else if (!a_config.IsEnabled(id)) {
				state = LayerState::kDisabledByConfig;
			} else {
				bool ok = false;
				try {
					ok = _install[i]();
				} catch (const std::exception& e) {
					log::Error("Layer {}: install threw: {}", ToString(id), e.what());
				} catch (...) {
					log::Error("Layer {}: install threw a non-standard exception", ToString(id));
				}
				state = ok ? LayerState::kActive : LayerState::kFailed;
			}

			const auto level = state == LayerState::kFailed ? log::Level::kError : log::Level::kInfo;
			log::Log(level, "Layer {}: {}", ToString(id), ToString(state));
		}
	}

	LayerState LayerRegistry::GetState(LayerId a_id) const noexcept
	{
		return _state[static_cast<std::size_t>(a_id)];
	}
}
