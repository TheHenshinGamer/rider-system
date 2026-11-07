# Third-party code

| Component | Source | License | How it's used |
| --- | --- | --- | --- |
| CommonLibF4 (Dear-Modding-FO4 fork) | https://github.com/Dear-Modding-FO4/CommonLibF4 (git submodule, `extern/CommonLibF4`) | GPL-3.0-or-later + Modding Exception + GPL-3.0 Linking Exception (see `extern/CommonLibF4/EXCEPTIONS`); originally based on MIT code | Statically linked; F4SE/engine reverse-engineering library. One DLL for OG 1.10.163, NG 1.10.984 and AE 1.11.x. |
| commonlib-shared | submodule of the above (`lib/commonlib-shared`) | same as CommonLibF4 | REL/REX: Address Library lookup, logging, utilities |
| dearmoddingui-api | submodule of the above (`lib/dearmoddingui-api`) | see its repository | Header-only, pulled in by CommonLibF4 |
| spdlog | xmake package (via commonlib-shared) | MIT | Logging backend |

R-CATS itself is licensed GPL-3.0 (see `LICENSE`).

No code, animations, meshes, textures or behavior files from Skyrim mods are included.

## Upstream notes

- The fork force-pushes its main branch. Our submodule is pinned to a commit; if that commit ever
  disappears upstream, mirror the fork and repoint `.gitmodules`.
