# Building and packaging

LibreCreatures is a Windows application. It builds as a 32-bit (x86)
program, the default and the reference build, or as a 64-bit (x64) program
([64-bit.md](64-bit.md)). The supported build is Visual Studio 2022 with the
MSVC, MFC and ATL components for the target architecture, driven by CMake
3.20 or newer.

## Build locally

Open a **Developer PowerShell for VS 2022** and run, for 32-bit:

```powershell
cmake -S . -B build/windows-msvc-x86 -G "Visual Studio 17 2022" -A Win32
cmake --build build/windows-msvc-x86 --config Release --parallel
```

or, for 64-bit:

```powershell
cmake -S . -B build/windows-msvc-x64 -G "Visual Studio 17 2022" -A x64 -DLC_ALLOW_X64=ON
cmake --build build/windows-msvc-x64 --config Release --parallel
```

Without `-DLC_ALLOW_X64=ON`, CMake stops when the target is not 32-bit.

The executable is `build/windows-msvc-<arch>/Release/Creatures.exe`. The
same build produces the kits and the `c1kitlib.dll` they share: `Hatchery.exe`,
`BiochemKit.exe`, `Owner's Kit.exe`, `Health Kit.exe`, `Science Kit.exe`,
`Breeder's Kit.exe`, `observation.exe`, `Injector.exe`, `Score Kit.exe`,
`Funeral Kit.exe`, and the new `Ecology Kit.exe`; see
[`src/kits/README.md`](../src/kits/README.md). (The Lab Kit in `src/kits/labkit`
is the test harness's kit, not for players; the CMake build does not make it.) Run a kit once on its own
to register it in the game's Tools menu. A 64-bit game uses 64-bit kits: the
two widths cannot share one folder, because each needs its own MFC and C
runtime DLLs.

On the `neorender` branch the build also links SDL3
(`third_party/SDL3/lib/x86` or `lib/x64`) and installs `SDL3.dll` and
`SDL3-LICENSE.txt` beside the executable.

Other CMake options: `-DLC_NATIVE_MODE=ON` compiles every switchable fix out
(the native game, for comparison tests). The default build reads
`librefixes.ini` and has the Options > LibreFixes window
(`LC_COMPAT_RUNTIME`).

The release build uses the shared MFC/CRT runtime. A machine that does not
already have the matching (x86 or x64) Visual C++ 2015-2022 and MFC
redistributable installed may need that Microsoft runtime before
`Creatures.exe` will start. The package does not bundle Microsoft runtime
DLLs.

To stage a redistributable package containing the executable, the kits, the
licence, README, public guide, and a corresponding-source notice:

```powershell
cmake --install build/windows-msvc-x86 --config Release --prefix dist/LibreCreatures
Compress-Archive -Path dist/LibreCreatures/* -DestinationPath dist/LibreCreatures-windows-x86.zip -Force
```

The repository also provides `scripts/build-release.ps1`, which performs those
steps, records the source commit in `SOURCE-CODE.txt`, and creates the ZIP in
one command. `-Arch x64` makes the 64-bit package
(`dist/LibreCreatures-windows-x64.zip`). Continuous integration calls that
script rather than maintaining a second build recipe.

## Supplying game data

The package contains the engine, not the original game's content. The most
reliable first run is to place the executable beside a legally obtained
Creatures 1 installation and launch it with that installation's normal
resource paths. The original game records those paths in the Windows registry;
LibreCreatures reads the same settings when they are present.

When no registry settings exist, startup falls back to the process working
directory. A saved world opened directly can still provide its generated
resources beside the save:

```text
Creatures.exe             (from the LibreCreatures package)
World.sfc                 (optional starting world)
Images/                   (world-specific galleries and sprites)
Genetics/                 (world-specific genome files)
```

When a saved world is opened directly, its neighbouring `Images` and `Genetics`
directories are used for world-specific generated resources. The original
installation, registry settings, and game data remain the user's responsibility.

## GitHub Actions

`.github/workflows/windows-release.yml` runs on Windows:

- A push or a pull request builds that branch for x86 as a compile check. It does
  not package or publish anything.
- A version tag (`v*`) or a manual run builds both branches for both
  architectures with `scripts/build-release.ps1` and uploads four zips:
  `LibreCreatures-windows-x86.zip`, `LibreCreatures-windows-x64.zip`,
  `LibreCreatures-neorender-windows-x86.zip` and
  `LibreCreatures-neorender-windows-x64.zip`. A tag also creates a GitHub
  Release with the four zips attached.

The workflow is deliberately ordinary: checkout, configure with CMake, build
with MSVC, stage with `cmake --install`, and upload the archives. A fork can
reuse it without access to the research workspace or any original game files.
