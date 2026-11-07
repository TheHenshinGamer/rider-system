#pragma once

namespace rcats::plugin
{
	// Non-fatal Address Library lookup. REL::ID::address() calls REX::FAIL (which ends the
	// game) when the running runtime's database has no entry for the ID; CLAUDE.md requires
	// that a missing address disables a layer instead. Check with this before building any
	// REL::Relocation from one of our IDs.
	[[nodiscard]] std::optional<std::uintptr_t> TryAddress(const REL::ID& a_id) noexcept;
}
