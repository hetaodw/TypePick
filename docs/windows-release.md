# Windows x64 installer

The release consists of `TypePick-Setup-0.1.0-x64.exe`, `TypePick-Source-0.1.0.zip`, `SHA256SUMS.txt`, `build-manifest.json` and Chinese usage instructions. The installer is currently unsigned. It targets Windows 10/11 x64 and 64-bit TSF clients; it does not install a 32-bit or ARM64 component.

## Build

Use Windows, Visual Studio 2022 C++ tools with ATL, Python 3, Git and NSIS 3. Run:

```powershell
./scripts/Build-Core.ps1
./scripts/Test-Rime.ps1
./scripts/Build-Boost.ps1
./scripts/Build-Weasel.ps1 -BoostRoot ./build/deps/boost/boost-1.84.0
./scripts/Get-ReleaseSources.ps1
./scripts/Build-Installer.ps1
```

If a previous build used another patch revision, pass a fresh `-Destination` to Build-Weasel and the same path as `-WeaselTree` to Build-Installer. Do not delete an existing working tree to force a rebuild. Dependencies are pinned in the scripts, product GUIDs in `config/product.json`. The source ZIP includes the tracked project, full upstream Weasel tree, pinned librime and plugin sources with recursive submodules, and Boost 1.84/1.89 source archives. Upstream librime's Windows build scripts describe how its supplied binary was built; TypePick does not modify that binary. The original pinyin_simp dictionary comes from `rime/rime-pinyin-simp` revision `0c6861ef7420ee780270ca6d993d18d4101049d0`.

## Runtime design

The Windows settings program stores the key in the current user's Credential Manager, target `TypePick/Jev`. It accepts literal `key="..."` or `TYPESAFE_API_KEY="..."` lines from `.env`; it does not evaluate shell expressions. Model calls prefer the process environment variable when explicitly set, then fall back to Credential Manager. No credentials are embedded in binaries or source archives. AI defaults off, and settings explain the text sent to Typesafe before opt-in. Enabling it currently supports only `notepad.exe`. The installed settings program defaults to a 1500 ms response budget; the native library's conservative fallback remains 600 ms.

TypePick has its own TSF CLSID/profile, IPC pipe/window/mutexes, HKLM `Software\TypePick`, startup value, install directory and `%APPDATA%\TypePick` directory. The upstream updater is disabled. Only the custom TSF DLL is registered. Uninstall unregisters it and removes recorded installed files, preserving user dictionaries and credentials.

Weasel IPC runs on pipe workers; its channel buffers are thread-local. The patch transfers each connection's buffer while its worker waits and dispatches the request synchronously on the server window thread, then restores the buffer to the worker. Rime state, candidate selection and the recommendation timer therefore share one thread. Model inference remains asynchronous on the Selector worker. This is covered by the real installed IPC test, including app identification, candidate output and Tab selection.

## Validation and limits

`Test-Installer.ps1` refuses to run outside disposable GitHub Actions runners. It checks silent install, TypePick CLSID registration, Credential Manager, real server IPC with normal pinyin dictionary, a deterministic recommendation accepted by Tab, rejection in a non-allowlisted client, uninstall and preservation of original Weasel registration. The demonstration model in this test never calls the network. This does not substitute for a human typing through TSF in a foreground application; application compatibility, focus handling and DPI should continue to be tested on Windows 10/11 desktops.

The `.env` is not sent to CI. The actual Jev endpoint was separately exercised locally with synthetic text before packaging; success of a test response does not guarantee a recommendation for every phrase. Unsupported/uncertain/late responses leave normal input unchanged.
