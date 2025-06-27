// This stub is compiled when the Windows App SDK / WinUI 3 is not available.
// It provides a fallback implementation for TabDisplay::RunTrayApp so the
// application can still link and run (doing nothing more than logging a
// warning and exiting immediately).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <spdlog/spdlog.h>

namespace TabDisplay {

int RunTrayApp()
{
    spdlog::warn("WinUI 3 is not available – running without tray UI.");
    // Nothing to do in the stub; exit right away.
    return 0;
}

} // namespace TabDisplay 