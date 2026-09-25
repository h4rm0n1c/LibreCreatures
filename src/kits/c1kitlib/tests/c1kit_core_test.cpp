// Portable tests for c1kitlib's header-only core: protocol decoding, launch
// arguments, reply parsing and the conversation helpers over a fake
// transport.  Builds with any C++17 compiler:
//   g++ -std=c++17 -D'__declspec(x)=' -I../include c1kit_core_test.cpp

#include "c1kit/conversation.hpp"
#include "c1kit/protocol.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace c1kit;

// Records every call in the pipe's verb form, and answers like the game.
class FakeTransport final : public MacroTransport {
public:
    std::vector<std::string> calls;
    std::string next_reply;
    long next_handle = 100;
    bool fail_create = false;
    long dead_handle = 0;  // load/destroy on this handle fail

    bool create_macro(short mode, long& handle) override {
        calls.push_back("CREATEMACRO " + std::to_string(mode));
        if (fail_create) {
            return false;
        }
        handle = next_handle++;
        return true;
    }
    bool destroy_macro(long handle) override {
        calls.push_back("DESTROYMACRO " + std::to_string(handle));
        return handle != dead_handle;
    }
    bool load_macro(long handle, const char* script) override {
        calls.push_back("LOADMACRO " + std::to_string(handle) + " " + script);
        return handle != dead_handle;
    }
    bool execute_macro(long handle, const char* script) override {
        calls.push_back("EXECUTEMACRO " + std::to_string(handle) + " " + script);
        return true;
    }
    bool request_macro(long handle) override {
        calls.push_back("REQUESTMACRO " + std::to_string(handle));
        reply_ = next_reply;
        return true;
    }
    const char* reply() const override { return reply_.c_str(); }
    std::size_t reply_length() const override { return reply_.size(); }
    void release() override {}

private:
    std::string reply_;
};

void test_communicate() {
    // YOUR_ID_IS for slot 6: tool<<16 | 0x0301.
    const KitMessage id = decode_communicate((6 << 16) | 0x0301, 0);
    assert(id.kind == kMessageKindIdentity);
    assert(id.code == kIdentityCodeYourIdIs);
    assert(id.aux == 6);
    // Control state 9 (pause): state<<8 | 0x02.
    const KitMessage pause = decode_communicate((9 << 8) | 0x02, 0);
    assert(pause.kind == kMessageKindControl);
    assert(pause.code == kControlStatePause);
    // Integer message to the Funeral Kit.
    const KitMessage integer = decode_communicate(0x0401, 1234);
    assert(integer.code == kIdentityCodeInteger && integer.payload == 1234);
}

void test_launch_args() {
    LaunchArgs args =
        parse_launch_args("/Embedding /ToolID=6 /ProgID=Overview.OLE");
    assert(args.embedding && !args.automation);
    assert(args.tool_id == 6);
    assert(args.prog_id == "Overview.OLE");
    args = parse_launch_args("-embedding -toolid=12");
    assert(args.embedding && args.tool_id == 12 && args.prog_id.empty());
    args = parse_launch_args("");
    assert(!args.embedding && args.tool_id == -1);
}

void test_take_field() {
    std::string text = "a|b|";
    assert(take_field(text, '|') == "a");
    assert(take_field(text, '|') == "b");
    assert(take_field(text, '|').empty() && text.empty());
    text = "tail";
    assert(take_field(text, '|') == "tail" && text.empty());
}

void test_parse_overview() {
    // A real reply captured from the game (ten fields per record).
    const std::string reply =
        "Male|4b5a4633|1|0:07|N/A|47%|Healthy|7|3547|927&"
        "Female|56424d31|2|0:06|No|47%|Healthy|7|4455|927";
    const std::vector<OverviewRecord> records = parse_overview(reply, 13);
    assert(records.size() == 2);
    assert(records[0][kOverviewName] == "Male");
    assert(records[0][kOverviewMoniker] == "4b5a4633");
    assert(records[0][kOverviewSex] == "1");
    assert(records[0][kOverviewAge] == "0:07");
    assert(records[0][kOverviewPregnancy] == "N/A");
    assert(records[0][kOverviewLifeForce] == "47%");
    assert(records[0][kOverviewMedical] == "Healthy");
    assert(records[0][kOverviewRoom] == "7");
    assert(records[1][kOverviewPregnancy] == "No");

    // The 1996 kit's cap.
    std::string many;
    for (int i = 0; i < 20; ++i) {
        many += (i ? "&" : "") + std::string("n|m") + std::to_string(i) + "|1";
    }
    assert(parse_overview(many, 13).size() == 13);
    assert(parse_overview("", 13).empty());
}

void test_scores() {
    // A reply captured from the game: hatchery 2, natural 0, previous 0,
    // current 2, points 8 (putv writes "%d|" per value).
    ScoreValues values;
    assert(parse_score_values("2|0|0|2|8|", values));
    assert(values.hatchery_eggs == 2 && values.natural_eggs == 0 &&
           values.previous_norns == 0 && values.current_norns == 2 &&
           values.breeding_points == 8);
    assert(breeders_score(values) == 8);
    values.natural_eggs = 3;
    assert(breeders_score(values) == 3 * 256 + 8);
    values.natural_eggs = 1000;
    assert(breeders_score(values) == 99999);
    assert(!parse_score_values("2|0|0|", values));
    int value = -1;
    assert(parse_first_value("12|", value) && value == 12);
    assert(!parse_first_value("", value));
}

void test_life_force() {
    assert(life_force_percent("47%") == 47);
    assert(life_force_percent("100%") == 100);
    assert(life_force_percent("Dead") == 0);  // bug 8 in the contract
    assert(life_force_percent("") == 0);
}

void test_poll_sequence() {
    // Observation's poll against the game's request log: a fresh query
    // holder every poll, the previous one destroyed first.
    FakeTransport game;
    game.next_reply = "Male|1|1|0:01|N/A|50%|Healthy|1|0|0";
    MacroConversation conversation(game);
    std::string reply;
    for (int poll = 0; poll < 2; ++poll) {
        assert(conversation.reconnect(kMacroModeQuery));
        assert(conversation.query("inst,dde: getb ovvd,endm", reply));
    }
    const std::vector<std::string> expected = {
        "CREATEMACRO 1",
        "LOADMACRO 100 inst,dde: getb ovvd,endm",
        "REQUESTMACRO 100",
        "DESTROYMACRO 100",
        "CREATEMACRO 1",
        "LOADMACRO 101 inst,dde: getb ovvd,endm",
        "REQUESTMACRO 101",
    };
    assert(game.calls == expected);
    assert(reply == game.next_reply);
}

void test_quit_keeps_holder() {
    FakeTransport game;
    game.next_handle = 7;
    assert(execute_scheduled(game, "inst,app: quit 6,endm", true));
    assert((game.calls == std::vector<std::string>{
        "CREATEMACRO 0", "EXECUTEMACRO 7 inst,app: quit 6,endm"}));
    game.calls.clear();
    assert(execute_scheduled(game, "x", false));
    assert(game.calls.back() == "DESTROYMACRO 8");
}

void test_reused_holder() {
    // One holder across polls; a holder the game no longer knows is
    // replaced once, even though destroying it fails.
    FakeTransport game;
    game.next_reply = "r";
    MacroConversation conversation(game);
    std::string reply;
    const char* script = "inst,dde: getb ovvd,endm";
    assert(conversation.query_reusing_holder(kMacroModeQuery, script, reply));
    assert(conversation.query_reusing_holder(kMacroModeQuery, script, reply));
    game.dead_handle = 100;
    assert(conversation.query_reusing_holder(kMacroModeQuery, script, reply));
    assert(reply == "r" && conversation.handle() == 101);
    const std::vector<std::string> expected = {
        "CREATEMACRO 1",
        "LOADMACRO 100 inst,dde: getb ovvd,endm",
        "REQUESTMACRO 100",
        "LOADMACRO 100 inst,dde: getb ovvd,endm",
        "REQUESTMACRO 100",
        "LOADMACRO 100 inst,dde: getb ovvd,endm",
        "DESTROYMACRO 100",
        "CREATEMACRO 1",
        "LOADMACRO 101 inst,dde: getb ovvd,endm",
        "REQUESTMACRO 101",
    };
    assert(game.calls == expected);
}

void test_create_failure() {
    FakeTransport game;
    game.fail_create = true;
    MacroConversation conversation(game);
    std::string reply;
    assert(!conversation.reconnect(kMacroModeQuery));
    assert(!conversation.query("x", reply));
    assert(!execute_scheduled(game, "x", false));
}

} // namespace

int main() {
    test_communicate();
    test_launch_args();
    test_take_field();
    test_parse_overview();
    test_life_force();
    test_scores();
    test_poll_sequence();
    test_quit_keeps_holder();
    test_reused_holder();
    test_create_failure();
    std::puts("c1kit core tests passed");
    return 0;
}
