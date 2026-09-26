// Portable tests for the Hatchery's nest (c1kit/hatchery.hpp).
//   g++ -std=c++17 -I../include hatchery_test.cpp

#include "c1kit/hatchery.hpp"

#include <cassert>
#include <cstdio>

using namespace c1kit;

int main() {
    Nest nest;
    assert(parse_nest("100110", nest));
    assert(nest.eggs[0] == EggState::female && nest.eggs[1] == EggState::male);
    assert(format_nest(nest) == "100110" && nest.any_left());
    nest.eggs[0] = EggState::taken;
    assert(format_nest(nest) == "x00110");
    assert(parse_nest("xxxxxx", nest) && !nest.any_left());
    assert(!parse_nest("10011", nest) && !parse_nest("10a110", nest) && !nest.any_left());
    assert(format_nest(fresh_nest(0x2d)) == "101101");

    // Exactly what the 1996 kit sent for egg 1 (female).
    assert(hatch_script(0, true) ==
           "inst,sys: wtop,sys: cmra 2223 724,new: simp eggs 8 0 2000 0,pose 3,"
           "setv clas 33882624,setv attr 67,new: gene tokn mum1 tokn dad1 obv0,"
           "setv obv1 2,tick 2400,dde: hatc,mvto 2408 870");
    assert(hatch_script(5, false).find("simp eggs 8 40 ") != std::string::npos);
    assert(hatch_script(5, false).find("tokn mum6 tokn dad6") != std::string::npos);
    assert(hatch_script(5, false).find("setv obv1 1,") != std::string::npos);
    assert(hatch_script(5, false).find("mvto 2608 870") != std::string::npos);
    std::puts("hatchery_test: all passed");
    return 0;
}
