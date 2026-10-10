// g++ -std=c++17 -Ilabkit/src labkit/tests/labkit_protocol_test.cpp -o t && ./t
#include "labkit_protocol.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main() {
    using namespace labkit;
    Request r = parse_request("CAOS dde: putv 1,endm");
    check(r.verb == "CAOS" && r.argument == "dde: putv 1,endm", "verb and raw argument");
    r = parse_request("PING\r\n");
    check(r.verb == "PING" && r.argument.empty(), "bare verb, line end dropped");
    r = parse_request("SCHEDULE a  b");
    check(r.argument == "a  b", "argument kept as sent");

    unsigned ms = 0, count = 0;
    check(parse_hang("500", ms, count) && ms == 500 && count == 1, "hang default count");
    check(parse_hang("250 3", ms, count) && ms == 250 && count == 3, "hang with count");
    check(!parse_hang("", ms, count), "hang needs ms");
    check(!parse_hang("10 0", ms, count), "hang count zero refused");
    check(!parse_hang("10 2 x", ms, count), "hang trailing junk refused");

    check(describe_message(1, 3, 11, 0) == "your id is 11", "identity");
    check(describe_message(2, 8, 0, 0) == "close (state 8)", "close");
    check(describe_message(2, 6, 0, 0) == "selected creature changed (state 6)", "selected");
    check(describe_message(1, 4, 0, -5) == "integer -5", "integer");
    check(describe_message(1, 1, 0, 0) == "kind 1 code 1", "other data");

    std::printf(failures == 0 ? "ok\n" : "%d failures\n", failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
