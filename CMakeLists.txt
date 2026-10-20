cmake_minimum_required(VERSION 3.20)

# Must be set before project() to take effect: pins the CRT selection so it
# tracks CMAKE_MSVC_RUNTIME_LIBRARY below (dynamic CRT, i.e. /MD) instead of
# whatever a given CMake version defaults to.
cmake_policy(SET CMP0091 NEW)

project(LibreCreatures CXX RC)

# --- This is an MSVC/MFC/Win32 project, full stop -----------------------
#
# The source is an independent reimplementation of Creatures 1, a 1996 32-bit
# Windows MFC application. Its object layouts, calling conventions, and COM/
# DirectSound plumbing are only meaningful on that target. There is no portable
# or cross-compiler build of this project, and there will not be one: MFC and
# ATL are MSVC-only, and the recovered semantics assume the x86 (Win32) ABI
# throughout.
if(NOT MSVC)
    message(FATAL_ERROR
        "LibreCreatures requires MSVC (Visual Studio, with the \"MFC and ATL "
        "support (x86)\" individual component installed). It cannot be built "
        "with any other compiler.")
endif()

option(LC_ALLOW_X64
    "Allow a 64-bit configure (docs/64-bit.md): the object-handle and \
holder-handle indirections make the recovered layout meaningful at that \
width too, but this is still a deliberate opt-in, not the default." OFF)
if(NOT LC_ALLOW_X64 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "LibreCreatures targets 32-bit x86 by default -- the original "
        "binary's recovered layout and calling conventions were only ever "
        "meaningful there. Reconfigure with -A Win32 (or -T host=x86), or "
        "pass -DLC_ALLOW_X64=ON for a deliberate 64-bit build (docs/64-bit.md).")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# MFC as a shared DLL (matches the recovered build: /D_AFXDLL, dynamic CRT).
# This is not "use whatever MFC you like" -- the original links dynamically,
# and _AFXDLL changes generated code paths (e.g. CDynLinkLibrary registration)
# that a static-MFC build would not take.
set(CMAKE_MFC_FLAG 2) # 2 == shared MFC, matches /MD + /D_AFXDLL
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
add_compile_options(/EHsc /GS /D_AFXDLL=1)
add_compile_definitions($<$<CONFIG:Debug>:_DEBUG> $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)

# The reconstructed resource script contains the original RT_MANIFEST. Tell
# CMake not to generate and embed a second manifest for the WIN32 target.
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")

set(SOURCE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src/c1)
set(INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Recursive glob is a deliberate call for a tree this size and this closely
# tied to a fixed recovered source layout: re-run cmake (or just reconfigure)
# after adding or removing a .cpp. `CONFIGURE_DEPENDS` covers that for CMake
# >= 3.12 generators that support it (Ninja, Makefiles); Visual Studio's own
# generator re-globs on every build anyway.
file(GLOB_RECURSE C1_SOURCES CONFIGURE_DEPENDS
    "${SOURCE_ROOT}/*.cpp"
)

set(C1_RESOURCE_SCRIPT ${SOURCE_ROOT}/resources/c1_resources.rc)

add_executable(Creatures WIN32
    ${C1_SOURCES}
    ${C1_RESOURCE_SCRIPT}
)

target_include_directories(Creatures PRIVATE
    ${INCLUDE_ROOT}
    ${SOURCE_ROOT}
)

# Native-compatibility builds (src/c1/common/compat.hpp, docs/original-bugs.md):
# LC_NATIVE_MODE compiles every switchable fix out, for the lab's own A/B
# testing.  LC_COMPAT_RUNTIME is what every shipped LibreCreatures exe is: a
# live, in-memory fix table that librefixes.ini sets at start-up and the
# Options > LibreFixes window changes while the game runs (also still
# readable from C1_NATIVE_ROWS, for the lab).  On by default, so a plain
# `cmake -S . -B ...` with no -D flags gives the same build the release
# scripts ship; LC_NATIVE_MODE turns it back off, since the two are
# separate builds.
option(LC_NATIVE_MODE "Build the native game: every switchable fix off" OFF)
if(LC_NATIVE_MODE)
    set(LC_COMPAT_RUNTIME_DEFAULT OFF)
else()
    set(LC_COMPAT_RUNTIME_DEFAULT ON)
endif()
option(LC_COMPAT_RUNTIME "Live-toggleable fixes: librefixes.ini, the \
LibreFixes window, and C1_NATIVE_ROWS" ${LC_COMPAT_RUNTIME_DEFAULT})
if(LC_NATIVE_MODE AND LC_COMPAT_RUNTIME)
    message(FATAL_ERROR "LC_NATIVE_MODE and LC_COMPAT_RUNTIME are separate builds")
endif()
if(LC_NATIVE_MODE)
    target_compile_definitions(Creatures PRIVATE LC_NATIVE_MODE=1)
endif()
if(LC_COMPAT_RUNTIME)
    target_compile_definitions(Creatures PRIVATE LC_COMPAT_RUNTIME=1)
endif()


target_link_libraries(Creatures PRIVATE
    # MFC, ANSI/MBCS (not the "u" Unicode variant) -- the original is an
    # ANSI Win32 application, and the recovered string handling throughout
    # assumes narrow characters.
    mfc140.lib
    mfcs140.lib
    atls.lib
    # Win32 API surface the recovered code actually calls into.
    kernel32.lib
    user32.lib
    gdi32.lib
    advapi32.lib
    shell32.lib
    ole32.lib
    oleaut32.lib
    winmm.lib
    dsound.lib
    comctl32.lib
    comdlg32.lib
    uuid.lib
)

set_target_properties(Creatures PROPERTIES
    OUTPUT_NAME "Creatures"
    LINK_FLAGS "/SUBSYSTEM:WINDOWS,6.00"
)

# SDL3 (third_party/SDL3, the official x86 VC build) draws the world view.
# SDL3.dll ships beside Creatures.exe.
set(SDL3_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL3)
target_include_directories(Creatures PRIVATE ${SDL3_ROOT}/include)
target_link_libraries(Creatures PRIVATE ${SDL3_ROOT}/lib/x86/SDL3.lib)
add_custom_command(TARGET Creatures POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL3_ROOT}/lib/x86/SDL3.dll $<TARGET_FILE_DIR:Creatures>)

# Group sources by their real subdirectory in generated IDEs (Visual Studio,
# CLion) instead of one flat list -- purely cosmetic, no effect on the build.
source_group(TREE ${SOURCE_ROOT} FILES ${C1_SOURCES} ${C1_RESOURCE_SCRIPT})

# --- Kits ----------------------------------------------------------------
#
# c1kitlib.dll is the data side every kit shares (the game connection, the
# kit's OLE server, settings); it has no UI.  Each kit executable is the kit
# shell (windows, drawing) plus that kit's own pages and resources.  The shell
# is compiled into every kit rather than linked from a static library: it
# holds the MFC application object, which nothing references by name, so a
# library member would be dropped by the linker.
set(KITS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/src/kits)

file(GLOB C1KITLIB_SOURCES CONFIGURE_DEPENDS "${KITS_ROOT}/c1kitlib/src/*.cpp")
add_library(c1kitlib SHARED ${C1KITLIB_SOURCES})
target_include_directories(c1kitlib PUBLIC ${KITS_ROOT}/c1kitlib/include)
target_compile_definitions(c1kitlib PRIVATE C1KITLIB_BUILD)
target_link_libraries(c1kitlib PRIVATE
    kernel32.lib user32.lib advapi32.lib ole32.lib oleaut32.lib uuid.lib)

file(GLOB C1KITSHELL_SOURCES CONFIGURE_DEPENDS "${KITS_ROOT}/shell/src/*.cpp")

function(add_c1_kit target output_name kit_dir)
    file(GLOB kit_sources CONFIGURE_DEPENDS "${kit_dir}/src/*.cpp")
    add_executable(${target} WIN32
        ${C1KITSHELL_SOURCES}
        ${kit_sources}
        ${kit_dir}/resources/c1_resources.rc
    )
    target_include_directories(${target} PRIVATE
        ${KITS_ROOT}/shell/include
        ${kit_dir}/src
        ${kit_dir}/resources
    )
    target_link_libraries(${target} PRIVATE
        c1kitlib
        mfc140.lib
        mfcs140.lib
        kernel32.lib
        user32.lib
        gdi32.lib
        advapi32.lib
        shell32.lib
        ole32.lib
        oleaut32.lib
        uuid.lib
        comctl32.lib
        comdlg32.lib
        winmm.lib
        dsound.lib
    )
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${output_name}"
        LINK_FLAGS "/SUBSYSTEM:WINDOWS,6.00"
    )
    install(TARGETS ${target} RUNTIME DESTINATION .)
endfunction()

# observation.exe, as the original is named.  Registers as the original's
# OVERVIEW.OLE server in Tools slot 6: run it once on its own and the game's
# Tools menu opens it instead of the 1996 kit.
add_c1_kit(ObservationKit "observation" ${KITS_ROOT}/observation)
# "Score Kit.exe", the Performance Kit: OLE server Score.OLE, Tools slot 8.
add_c1_kit(ScoreKit "Score Kit" ${KITS_ROOT}/score)
# "Owner's Kit.exe": OLE server Owner.OLE, Tools slot 2.
add_c1_kit(OwnersKit "Owner's Kit" ${KITS_ROOT}/owner)
# "Funeral Kit.exe": OLE server Funeral.OLE, Tools slot 9.
add_c1_kit(FuneralKit "Funeral Kit" ${KITS_ROOT}/funeral)
# "Science Kit.exe": OLE server Science.OLE, Tools slot 4.
add_c1_kit(ScienceKit "Science Kit" ${KITS_ROOT}/science)
# "Health Kit.exe": OLE server Health.OLE, Tools slot 3.
add_c1_kit(HealthKit "Health Kit" ${KITS_ROOT}/health)
# "Breeder's Kit.exe": OLE server Sex.OLE, Tools slot 5.
add_c1_kit(BreedersKit "Breeder's Kit" ${KITS_ROOT}/breeder)
# "Hatchery.exe": OLE server Hatchery.OLE, Tools slot 0.
add_c1_kit(Hatchery "Hatchery" ${KITS_ROOT}/hatchery)
# "Injector.exe", the Injector Kit 2.0 (Object Injector): OLE server
# ObjectInjector.OLE, Tools slot 7.
add_c1_kit(Injector "Injector" ${KITS_ROOT}/injector)
# "BiochemKit.exe", the Biochemistry Kit (v1.2): OLE server BiochemKit.OLE,
# Tools slot 1.
add_c1_kit(BiochemKit "BiochemKit" ${KITS_ROOT}/biochem)
# "Ecology Kit.exe", new in LibreCreatures (no 1996 original): OLE server
# Ecology.OLE, Tools slot 10.
add_c1_kit(EcologyKit "Ecology Kit" ${KITS_ROOT}/ecology)

install(TARGETS c1kitlib RUNTIME DESTINATION .)

# Keep packaging predictable. The executable and public project material are
# installed; original game data is intentionally never staged.
install(TARGETS Creatures RUNTIME DESTINATION .)
install(FILES
    README.md
    LICENSE
    SOURCE-CODE.txt
    DESTINATION .
)
# SDL3.dll must sit beside Creatures.exe, with SDL's licence.
install(FILES ${SDL3_ROOT}/lib/x86/SDL3.dll DESTINATION .)
install(FILES ${SDL3_ROOT}/LICENSE.txt DESTINATION . RENAME SDL3-LICENSE.txt)
install(DIRECTORY docs DESTINATION .)
