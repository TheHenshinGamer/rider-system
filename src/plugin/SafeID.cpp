#include "PCH.h"

#include "SafeID.h"

namespace rcats::plugin
{
	namespace
	{
		// IDDB keeps its tables behind a protected accessor (its own lookup is the fatal
		// one). Deriving lets us name that accessor; we never construct an IDDBProbe, we
		// only call the inherited member on the real singleton through a member pointer.
		struct IDDBProbe : REL::IDDB
		{
			static std::optional<std::uint64_t> FindOffset(std::uint64_t a_id) noexcept
			{
				const REL::IDDB* db = REL::IDDB::GetSingleton();
				if (!db) {
					return std::nullopt;
				}

				// Format 5 (NG/AE databases): a flat array indexed by ID, 0 = no entry.
				constexpr auto getV5 = &IDDBProbe::get_id2offset<std::uint32_t>;
				if (const auto v5 = (db->*getV5)(); !v5.empty()) {
					if (a_id < v5.size() && v5[a_id] != 0) {
						return v5[a_id];
					}
					return std::nullopt;
				}

				// Formats 0-2 (OG databases): sorted (id, offset) pairs.
				constexpr auto getV0 = &IDDBProbe::get_id2offset<REL::IDDB::MAPPING>;
				const auto v0 = (db->*getV0)();
				const auto it = std::lower_bound(v0.begin(), v0.end(), a_id,
					[](const REL::IDDB::MAPPING& a_elem, std::uint64_t a_value) { return a_elem.id < a_value; });
				if (it != v0.end() && it->id == a_id) {
					return it->offset;
				}
				return std::nullopt;
			}
		};
	}

	std::optional<std::uintptr_t> TryAddress(const REL::ID& a_id) noexcept
	{
		const auto offset = IDDBProbe::FindOffset(a_id.id());
		if (!offset) {
			return std::nullopt;
		}
		return REX::FModule::GetExecutingModule().GetBaseAddress() + static_cast<std::uintptr_t>(*offset);
	}
}
