#pragma once

namespace creatures1::platform {

// The Win32 implementation owns token/SID lookup, SDDL conversion, heap
// cleanup, synchronization, and SECURITY_ATTRIBUTES storage.  C1 only needs
// to know whether the named-pipe security policy could be established.
class NamedPipeSecurityApi {
public:
    virtual ~NamedPipeSecurityApi() = default;

    virtual bool ensure_current_user_security_attributes() = 0;
};

bool build_current_user_security_descriptor(NamedPipeSecurityApi& api);

} // namespace creatures1::platform
