# R-CATS — Rider Combat And Traversal System (working name)

Fallout 4 F4SE plugin: Kamen Rider–inspired action combat (combo chains, power-attack variants,
bone-traced hits, stagger/hitstop) plus parkour/traversal (vault, climb, mantle), tied to a
transformation belt. Working name R-CATS; RIOT is the alternate. Don't rename anything without asking.

## Ground rules (non-negotiable)

- **Original work only.** Do not copy code, animations, meshes, textures or behavior files from
  any Skyrim mod. MCO, BFCO, OAR, Precision, TDM, Elden Rim, dTry's Key Utils, Modern Stagger Lock,
  SkyParkour and the Kamen Rider Zeztz moveset are design references for *feel and architecture* only.
- Open-source references may be read to understand an approach; reuse code only if its license allows
  it, and record the license + attribution in `THIRD_PARTY.md` when you do. If unsure, ask me first.
- Animations come from original work or sources with explicit conversion permission.

## Environment

- Windows, Visual Studio 2026 (C++ desktop workload), xmake 3.x, CommonLibF4.
- **Target game version: AE 1.11.240** (main install), with OG 1.10.163 (GOG install at
  `G:\Fallout 4`) and NG 1.10.984 supported from the same DLL from day one. CommonLibF4 fork:
  Dear-Modding-FO4 (GPL-3 + modding exception, submodule at `extern/CommonLibF4`); build is xmake.
  R-CATS is GPL-3. Use Address Library IDs as `REL::ID{ og, ng, ae }`, never hardcoded offsets;
  every new hook must resolve on all three runtimes or disable its layer. Note: the library's
  ID lookup is fatal on a missing ID, so check our own IDs exist before resolving them.
- Mod Organizer 2 mods folder: `D:\Naked Commonwealth\mods`. The build should copy the DLL (and PDB)
  to `D:\Naked Commonwealth\mods\R-CATS\F4SE\Plugins\` after each build.
- Skeleton: ZeX (ZaZ Extended Skeleton). Animation and hit-detection work must assume ZeX bone names.
- Papyrus logs: `F:\Documents\my games\Fallout4\Logs\Script`. Plugin log goes to the standard F4SE log folder.
- Related projects of mine: AnimBridge (Skyrim <-> FO4 animation converter), 4Stim, Papyrus Overdrive.
  Don't add dependencies on them without asking.

## Architecture — one module per layer

| Layer | Responsibility |
| --- | --- |
| Input | Directional + attack chords (W+PA, S+PA, D+PA), input buffer, rebindable keys |
| AnimReplacer | Conditional animation swaps (weapon, stance, form, state); everything else plays through it |
| Combo | Behavior graph hooks for melee/unarmed, chains, power-attack variants, cancel windows |
| HitDetect | Bone-traced weapon/limb collision replacing cone checks |
| Impact | Stagger, hitstop, knockback, launches |
| Movement | Directional third-person movement, lock-on |
| Parkour | Ledge raycasts (vault/climb/mantle/too tall), controller takeover along root motion |
| Skills | AP-cost specials, charge states, finishers |
| Transform | Belt-triggered moveset swap, powerup modes, possible Power Armor hook |

Layers talk through a small internal event bus, not by reaching into each other. Each layer can be
disabled from config so problems can be isolated.

## Build order (finish and commit each before starting the next)

1. Foundation: plugin skeleton, config (INI now, MCM later), logging, Input layer
2. AnimReplacer — milestone: custom idle/swing plays in-game
3. Combo — milestone: 4-hit unarmed string chains correctly
4. HitDetect — milestone: hits land where they visually land
5. Impact
6. Movement + lock-on
7. Parkour
8. Skills
9. Transform
10. Crossover content (aerial attacks off vaults, ledge Rider Kick)

**Current phase: 1.**

## Working conventions

- Small, reviewable steps. One feature per change; commit when it builds and loads.
- Every hook gets a log line on install and a config flag to disable it.
- Fail safe: if a hook or address can't resolve, log it and disable that layer instead of crashing.
- Comment *why* for any reverse-engineered behavior (what the engine does, how we found it).
- Ask before: changing the target game version, adding a dependency, touching save data/cosave format.

## Lore (for naming forms, recipes, terminals)

The belt was sold as an AntAgonizer toy (Hubris Comics, The Unstoppables) but was a West Tek field
trial; the toys recorded kids' motion data to train the real suit's motion assist. Belts were made at
Wilson Atomatoys near Quincy. Full design: the R-CATS design doc on claude.ai.
