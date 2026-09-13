#pragma once

// MFC refuses to compile unless Winsock2 is included ahead of windows.h.
// Every concrete Win32/MFC unit includes this first so the order cannot drift.
#include <winsock2.h>
#include <windows.h>
