#pragma once

namespace creatures1::platform {

// The concrete implementation performs the Windows GetModuleHandleA /
// GetProcAddress probe.  Keeping that SDK lookup behind this boundary leaves
// the application independent of Windows loader types.
class WineVersionExportProbe {
public:
    virtual ~WineVersionExportProbe() = default;

    virtual bool wine_get_version_is_available() const = 0;
};

class WineEnvironment {
public:
    explicit WineEnvironment(const WineVersionExportProbe& probe)
        : probe_(probe) {}

    bool is_running_under_wine() const;

private:
    const WineVersionExportProbe& probe_;
    mutable int cached_result_ = -1;
};

} // namespace creatures1::platform
