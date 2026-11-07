# R-CATS: Rider Combat And Traversal System

Fallout 4 F4SE plugin: Kamen Rider–inspired action combat and parkour. See `CLAUDE.md` for the
design ground rules, layer architecture and build order.

## Supported runtimes

One DLL targets all of these. Engine addresses come from Address Library IDs given per runtime
(`REL::ID{ og, ng, ae }`), never hardcoded offsets.

| Runtime | Version | F4SE |
| --- | --- | --- |
| Old-Gen | 1.10.163 | 0.6.23 |
| Next-Gen | 1.10.984 | 0.7.x |
| Anniversary Edition | 1.11.137 – 1.11.240 (primary: **1.11.240**) | 0.7.9 for 1.11.240 |

Users need F4SE and the matching **Address Library for F4SE Plugins**.

## Layout

```
src/core/     engine-free: config (INI), logging front, event bus, layer registry, key codes
src/input/    engine-free Input layer: directional attack chords, hold-to-power, input buffer
src/plugin/   engine-facing: F4SE entry points, input sink (CommonLibF4)
tests/        unit tests for everything engine-free
res/          default R-CATS.ini
extern/       CommonLibF4 submodule (Dear-Modding-FO4 fork)
```

Engine-free code never includes CommonLibF4, so all game logic can be unit-tested without the game.

## Building

Requirements: Visual Studio 2026 with the C++ desktop workload, and [xmake](https://xmake.io) 3.0+.

```powershell
git submodule update --init --recursive
xmake f -m releasedbg      # or -m debug
xmake build                # builds the DLL + tests, then deploys
xmake test                 # runs the engine-free unit tests
xmake project -k vsxmake   # optional: generate a Visual Studio solution
```

Each build installs `R-CATS.dll` and `.pdb` to `D:\Naked Commonwealth\mods\R-CATS\F4SE\Plugins` and
seeds `R-CATS.ini` there if it's missing (it's never overwritten). Change the target with
`xmake f --mods_dir="..."`, or set the `XSE_FO4_MODS_PATH` environment variable.

## Logs

- Plugin: `Documents\My Games\Fallout4\F4SE\R-CATS.log`
- Papyrus: `F:\Documents\my games\Fallout4\Logs\Script`

## License

GPL-3.0 (see `LICENSE`), matching CommonLibF4. See `THIRD_PARTY.md`.
