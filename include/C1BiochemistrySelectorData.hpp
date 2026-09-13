#pragma once

// Exact 32-entry selector tables recovered from Creatures.exe at 00454500
// and 00454580.  They are shared by Biochemistry and Lobe; keeping the
// declarations in one clean-project header prevents those owners from
// silently acquiring separate namespace-local copies.
extern unsigned int g_biochemistry_tick_selector_masks[32];
extern unsigned int g_biochemistry_tick_selector_q16_multipliers[32];
