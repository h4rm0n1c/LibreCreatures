#pragma once

// The two ways the 1996 kits drive SFC.OLE, built on the MacroTransport
// primitives.  Header-only and portable, so it can be tested against a fake
// transport.

#include <string>

#include "c1kit.hpp"
#include "protocol.hpp"

namespace c1kit {

// A query conversation owns one macro holder at a time.
class MacroConversation {
public:
    explicit MacroConversation(MacroTransport& transport)
        : transport_(transport) {}

    MacroConversation(const MacroConversation&) = delete;
    MacroConversation& operator=(const MacroConversation&) = delete;

    // ReconnectDdeConversation (Observation @ 0x004033f0): destroy the
    // current holder, if any, then create a new one.  A failed destroy
    // leaves the old handle in place and fails.
    bool reconnect(short mode) {
        if (handle_ != 0) {
            if (!transport_.destroy_macro(handle_)) {
                return false;
            }
            handle_ = 0;
        }
        long created = 0;
        if (!transport_.create_macro(mode, created)) {
            return false;
        }
        handle_ = created;
        return handle_ != 0;
    }

    // LoadMacro then RequestMacro on the current holder
    // (Observation LoadOverviewData @ 0x004055f0).
    bool query(const char* script, std::string& reply) {
        reply.clear();
        if (handle_ == 0 || script == nullptr) {
            return false;
        }
        if (!transport_.load_macro(handle_, script)) {
            return false;
        }
        if (!transport_.request_macro(handle_)) {
            return false;
        }
        reply.assign(transport_.reply(),
                     binary_ ? transport_.reply_bytes() : transport_.reply_length());
        return true;
    }

    // query_reusing_holder for a binary reply (`dde: lobe`): the whole reply
    // BSTR, zero bytes and all.
    bool query_binary(short mode, const char* script, std::string& reply) {
        binary_ = true;
        const bool ok = query_reusing_holder(mode, script, reply);
        binary_ = false;
        return ok;
    }

    // Query on a holder that is kept between calls.  The 1996 kits replace
    // the holder before every query (a Create/Destroy pair per poll); a
    // holder can run any number of scripts, so this reuses it, replacing it
    // only when there is none or a query on it fails.
    bool query_reusing_holder(short mode, const char* script,
                              std::string& reply) {
        if (handle_ == 0 && !reconnect(mode)) {
            return false;
        }
        if (query(script, reply)) {
            return true;
        }
        // The holder may no longer exist on the game side, in which case its
        // destroy fails too; close() forgets the handle either way.
        close();
        return reconnect(mode) && query(script, reply);
    }

    void close() {
        if (handle_ != 0) {
            transport_.destroy_macro(handle_);
            handle_ = 0;
        }
    }

    long handle() const { return handle_; }

private:
    MacroTransport& transport_;
    long handle_ = 0;
    bool binary_ = false;
};

// ExecuteDdeCommandWithReconnect (Observation @ 0x00403540): a fresh
// scheduler-mode holder per command.  The holder is destroyed afterwards
// unless `keep_holder` (the 1996 kits keep it while quitting, because
// `app: quit` takes the kit down with it).
inline bool execute_scheduled(MacroTransport& transport, const char* script,
                              bool keep_holder) {
    long handle = 0;
    if (!transport.create_macro(kMacroModeSchedule, handle) ||
        handle == 0) {
        return false;
    }
    if (!transport.execute_macro(handle, script)) {
        transport.destroy_macro(handle);
        return false;
    }
    if (!keep_holder) {
        transport.destroy_macro(handle);
    }
    return true;
}

} // namespace c1kit
