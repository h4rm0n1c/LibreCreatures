// The brain recorder is a lab tool; the code under test only reports to it
// when a recording is on.  These stand in for it, switched off.

#include "brain/brain_recorder.hpp"

namespace creatures1::brain {

bool g_chemical_flows_wanted = false;
bool g_brain_events_wanted = false;
bool g_brain_votes_wanted = false;

void record_chemical_flow(const void*, ChemicalOrigin, std::uint32_t, std::uint8_t, int) {}

ChemicalOrigin ChemicalOriginScope::current_origin() { return ChemicalOrigin::unknown; }
std::uint32_t ChemicalOriginScope::current_number() { return 0; }

} // namespace creatures1::brain
