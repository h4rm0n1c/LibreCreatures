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

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR
        "LibreCreatures targets 32-bit x86 only -- the original binary's "
        "recovered layout and calling conventions are not meaningful at any "
        "other pointer width. Reconfigure with -A Win32 (or -T host=x86, "
        "whichever your generator uses).")
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

# Group sources by their real subdirectory in generated IDEs (Visual Studio,
# CLion) instead of one flat list -- purely cosmetic, no effect on the build.
source_group(TREE ${SOURCE_ROOT} FILES ${C1_SOURCES} ${C1_RESOURCE_SCRIPT})

# Keep packaging predictable. The executable and public project material are
# installed; original game data is intentionally never staged.
install(TARGETS Creatures RUNTIME DESTINATION .)
install(FILES
    README.md
    LICENSE
    SOURCE-CODE.txt
    DESTINATION .
)
install(DIRECTORY docs DESTINATION .)
