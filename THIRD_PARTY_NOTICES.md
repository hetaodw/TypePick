# Third-party notices

TypePick's original code, documentation, test fixtures and build scripts are licensed under AGPL-3.0-only. Copyright (c) 2026 TypePick contributors. Third-party materials retain their original notices and licenses.

| Component | Pinned source | License / location |
| --- | --- | --- |
| Weasel (小狼毫) | [rime/weasel](https://github.com/rime/weasel/tree/d73f6295e8252ed2f7b9c12bae32e9001b1afdaa), submodule | GPLv3; see `upstream/weasel/LICENSE.txt` and notices within upstream |
| librime header | [rime/librime 1.17.0](https://github.com/rime/librime/tree/1.17.0) | BSD-3-Clause; `third_party/librime/LICENSE` |
| nlohmann/json | [v3.12.0](https://github.com/nlohmann/json/tree/v3.12.0) | MIT; `third_party/nlohmann/LICENSE.MIT` |
| Boost (downloaded for the Weasel build) | [boostorg/boost 1.84.0](https://github.com/boostorg/boost/releases/tag/boost-1.84.0) | Boost Software License 1.0; `LICENSE_1_0.txt` in the downloaded source |

The Weasel integration patch marks the TypePick modifications and leaves upstream notices intact. GPLv3 section 13 permits combination with AGPLv3 code; the respective source retains its license, and the AGPL network-interaction requirements apply to the combination as specified there. The top-level AGPL license does not replace Weasel's license or permit proprietary relicensing of upstream code.

The Rime binary is downloaded from the official release by `scripts/Get-Rime.ps1`, not checked into this repository. Release asset: `rime-33e7814-Windows-msvc-x64.7z`, SHA256 `7478c7caa4ff6b37de86daba1f7ce4a994a4f5ba24872a820fb2b3a9b01fed15`. librime's linked libraries and plugins have their own licenses. Before distributing a complete binary installer, collect the notices and corresponding-source obligations of the exact binary dependencies, including upstream bundled WTL, WinSparkle, Boost, plugins, dictionaries and utilities. This development repository is not such an installer.

The small `data/typepick_demo.*` dictionary is authored for this project. No third-party production dictionary or model weights are included. The Jev API is a remote service with separate terms; this repository includes only its client implementation.

The build script pins the official GitHub asset `boost-1.84.0.7z`, SHA256 `ce132f85fc706aa8b5c7e6e52a8338de33a688e92e7c8fada3713194b109232e`. This checksum was computed from the downloaded official release asset. The archive is a modular source distribution; Boost.Build creates the aggregate header tree before compiling.
