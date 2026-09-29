# Third-party code

| Library | Version | Files | Licence | Used for |
| --- | --- | --- | --- | --- |
| [SDL3](https://github.com/libsdl-org/SDL) | 3.4.16 | `SDL3/include`, `SDL3/lib/x86/SDL3.lib`, `SDL3/lib/x86/SDL3.dll` (sha256 `47d508b4...c6531920`) from `SDL3-devel-3.4.16-VC.zip` | zlib (`SDL3/LICENSE.txt`) | Drawing the world view and eye view (neorender); `SDL3.dll` ships beside `Creatures.exe` |
| [stb_image](https://github.com/nothings/stb) | 2.30 | `stb/stb_image.h` (sha256 `594c2fe3...5f4200b3`) | Public domain or MIT (the file's own terms) | Decoding the PNG frames inside `.s32` sprite files (`src/c1/display/png_decode.cpp`, PNG only) |

Files here are vendored unchanged. Update them by replacing the file and the
row above.
