# Building and packaging

LibreCreatures is a 32-bit Windows application. The supported build is Visual
Studio 2022 with the MSVC, MFC, and ATL components installed, driven by CMake.

## Build locally

Open a **Developer PowerShell for VS 2022** and run:

```powershell
cmake -S . -B build/windows-msvc-x86 -G "Visual Studio 17 2022" -A Win32
cmake --build build/windows-msvc-x86 --config Release --parallel
```

The executable is `build/windows-msvc-x86/Release/Creatures.exe`.

The release build uses the shared x86 MFC/CRT runtime. A machine that does not
already have the matching Visual C++ and MFC redistributable installed may need
that Microsoft runtime before `Creatures.exe` will start. The current package
does not bundle Microsoft runtime DLLs.

To stage a redistributable package containing the executable, licence, README,
public guide, and a corresponding-source notice:

```powershell
cmake --install build/windows-msvc-x86 --config Release --prefix dist/LibreCreatures
Compress-Archive -Path dist/LibreCreatures/* -DestinationPath dist/LibreCreatures-windows-x86.zip -Force
```

The repository also provides `scripts/build-release.ps1`, which performs those
steps, records the source commit in `SOURCE-CODE.txt`, and creates the ZIP in
one command. Continuous integration calls that script rather than maintaining
a second build recipe.

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

`.github/workflows/windows-release.yml` runs the same PowerShell script on
Windows. Pull requests and pushes produce a downloadable build artifact. A tag
such as `v0.1.0` additionally creates a GitHub Release and attaches the ZIP.

The workflow is deliberately ordinary: checkout, configure with CMake, build
with MSVC x86, stage with `cmake --install`, and upload one archive. A fork can
reuse it without access to the research workspace or any original game files.
