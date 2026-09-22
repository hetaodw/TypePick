# Third-party notices

TypePick's original code, documentation, test fixtures and build scripts are licensed under AGPL-3.0-only. Copyright (c) 2026 TypePick contributors. Third-party materials retain their original notices and licenses.

| Component | Pinned source | License / location |
| --- | --- | --- |
| Weasel (小狼毫) | [rime/weasel](https://github.com/rime/weasel/tree/d73f6295e8252ed2f7b9c12bae32e9001b1afdaa), submodule | GPLv3; see `upstream/weasel/LICENSE.txt` and notices within upstream |
| librime header | [rime/librime 1.17.0](https://github.com/rime/librime/tree/1.17.0) | BSD-3-Clause; `third_party/librime/LICENSE` |
| nlohmann/json | [v3.12.0](https://github.com/nlohmann/json/tree/v3.12.0) | MIT; `third_party/nlohmann/LICENSE.MIT` |
| Boost (downloaded for the Weasel build) | [boostorg/boost 1.84.0](https://github.com/boostorg/boost/releases/tag/boost-1.84.0) | Boost Software License 1.0; `LICENSE_1_0.txt` in the downloaded source |

The Weasel integration patch marks the TypePick modifications and leaves upstream notices intact. GPLv3 section 13 permits combination with AGPLv3 code; the respective source retains its license, and the AGPL network-interaction requirements apply to the combination as specified there. The top-level AGPL license does not replace Weasel's license or permit proprietary relicensing of upstream code.

The Rime binary is downloaded from the official release by `scripts/Get-Rime.ps1`, not checked into this repository. Release asset: `rime-33e7814-Windows-msvc-x64.7z`, SHA256 `7478c7caa4ff6b37de86daba1f7ce4a994a4f5ba24872a820fb2b3a9b01fed15`. `Get-ReleaseSources.ps1` collects its pinned sources and submodules; `Build-Installer.ps1` includes dependency license/copyright/notice files. The accompanying source ZIP includes the full TypePick/Weasel sources, librime and plugins, and Boost source archives. Preserve and distribute the source ZIP alongside the binary installer.

The small `data/typepick_demo.*` dictionary is authored for this project. The installed dictionary `packaging/data/pinyin_simp.dict.yaml` is from [rime-pinyin-simp](https://github.com/rime/rime-pinyin-simp/tree/0c6861ef7420ee780270ca6d993d18d4101049d0), Apache-2.0; original LICENSE and AUTHORS are in `third_party/pinyin-simp`. The `weasel.yaml` UI configuration is copied from the pinned Weasel source. The Jev API is a remote service with separate terms; no model weights are included.

WTL 10 headers are bundled by upstream Weasel, copyright Microsoft Corporation and WTL Team, Microsoft Public License; see `third_party/wtl/MS-PL.txt` (license text mirrored from sailfish009/wtl). The TypePick build removes all WinSparkle calls and does not bundle its DLL. Windows runtime/system libraries are provided by Microsoft. The installer is generated with NSIS; the NSIS compiler is not bundled.

The official librime DLL includes librime-lua (`ec52e48ea18f11af37717a01c337f853215cf70b`, BSD-3-Clause), librime-octagram (`dfcc15115788c828d9dd7b4bff68067d3ce2ffb8`, GPL-3.0) and librime-predict (`920bd41ebf6f9bf6855d14fbe80212e54e749791`, BSD-3-Clause). The Lua vendor source is pinned to the thirdparty-branch revision preceding the official release (`fa40fadd8af1e5b1fbd55703ccbd54476956d74c`); original Lua notices are retained. librime's pinned dependency submodules include glog, leveldb, marisa-trie, OpenCC and yaml-cpp; their complete original notices are collected in `licenses/dependencies` in the installed product. Its Windows build uses Boost 1.89.0, whose source archive is also included. See the dependency source directories for file-specific licenses.

The build script pins the official GitHub asset `boost-1.84.0.7z`, SHA256 `ce132f85fc706aa8b5c7e6e52a8338de33a688e92e7c8fada3713194b109232e`. This checksum was computed from the downloaded official release asset. The archive is a modular source distribution; Boost.Build creates the aggregate header tree before compiling.
