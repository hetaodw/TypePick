# 开发与运行

## 原生模块

Windows x64，PowerShell 7，Visual Studio 2022 或更新版 C++ Build Tools，Windows 10/11 SDK，CMake/Ninja。运行：

```powershell
./scripts/Build-Core.ps1
./scripts/Test-Rime.ps1
```

`Build-Core.ps1` 自动寻找开发工具，配置 Ninja、编译并执行测试；也可用 `-CMakePath` 指定 CMake。构建路径均在项目 `build` 下。首次 Rime 测试从官方 GitHub release 下载 1.17.0 x64 包并验证 SHA256。

已下载的 librime 可手动传入，不重复下载：

```powershell
./scripts/Test-Rime.ps1 -RimeDll C:/deps/rime/dist/lib/rime.dll
```

## 接入小狼毫

准备源码不需要 ATL。首次执行应用补丁，后续执行更新 TypePick 源码覆盖层；上游版本或补丁变更时要求新建目标目录，防止丢失开发修改。

```powershell
git submodule update --init upstream/weasel
./scripts/Prepare-Weasel.ps1
```

完整构建还需要对应 MSVC 工具集的 **C++ ATL**，以及 Boost 1.84.0 头文件及 x64 静态库。在 Visual Studio Installer 中添加 ATL。参考上游构建方式，在 Boost 源码目录的 x64 开发命令行中运行：

```bat
bootstrap.bat
b2 -j4 --with-filesystem --with-json --with-locale --with-regex --with-serialization --with-system --with-thread toolset=msvc link=static runtime-link=static variant=release architecture=x86 address-model=64 define=BOOST_USE_WINAPI_VERSION=0x0603 stage
```

必须与小狼毫所用工具集一致；Boost 较老版本可能不认识更新的 MSVC，优先用 VS2022/v143。Boost 官方下载与许可证见 [Boost 1.84.0](https://www.boost.org/releases/1.84.0/)。

也可运行 `./scripts/Build-Boost.ps1` 从 Boost 官方 GitHub release 下载并校验模块化源码包，编译到 `build/deps/boost/boost-1.84.0`。GitHub Actions 中另有完整小狼毫构建任务，使用带 ATL 的 Windows 2022 开发环境。

```powershell
./scripts/Build-Weasel.ps1 -BoostRoot C:/deps/boost_1_84_0 -PlatformToolset v143
# 可选：提供本地已解压的、相同架构的官方 librime dist
./scripts/Build-Weasel.ps1 -BoostRoot C:/deps/boost_1_84_0 -RimeDistribution C:/deps/rime/dist
```

该脚本仅构建 `WeaselServer`、`WeaselTSF` 的 x64 Release 及其项目依赖，不创建完整安装器或32位客户端。使用上游随仓库提供的 WTL、WinSparkle 等资源，保留其许可证。在可丢弃的 Windows 测试环境中，才继续进行小狼毫部署、服务启动和 TSF 注册验收。当前脚本不自动修改注册表、用户方案或已安装输入法。

开发版暂沿用小狼毫的服务名、TSF 标识、注册表及用户目录。**不能与原版作为两个独立输入法并存安装**；产品化前须设计隔离标识和升级/回滚路径。

## 启用推荐

在测试环境的小狼毫用户资料目录创建 `typepick.json`，参考 `config/typepick.example.json`：

- `enabled: false` 为默认；改成 `true` 才启用。
- `mode: "demo"` 本地演示；`mode: "jev"` 使用远端模型。
- `allowed_apps` 目前只能包含 `notepad.exe`；增加其他应用会令配置校验失败并关闭 AI。
- `debounce_ms` 范围 0–2000；`timeout_ms` 范围 100–3000。
- 置信度与概率差阈值范围 0–1。无效 JSON 或参数会关闭 AI。

配置在服务端初始化时读取，变更后重启开发服务端。配置和密钥不要提交 Git。

测试 Jev 的独立工具（只发送你指定的测试文本，不需要安装输入法）：

```powershell
# 先通过自己的密钥管理方式设置当前进程 TYPESAFE_API_KEY。
./build/core-native/TypePickProbe.exe --rime C:/deps/rime/dist/lib/rime.dll --data ./data --user ./build/live-probe --live --input yanjiu --context '这个问题需要进一步'
```

也可以在根目录 `.env` 中只填写一行 `key="你的密钥"`（或 `TYPESAFE_API_KEY="你的密钥"`），然后运行：

```powershell
./scripts/Test-Jev.ps1
# 复现输入法默认时限；如果请求超时，该测试会返回失败。
./scripts/Test-Jev.ps1 -TimeoutMs 600
```

脚本只使用固定的两个合成句子，仅读取文件中的字面值，不执行 PowerShell 内容。密钥临时映射到当前测试进程的 `TYPESAFE_API_KEY`，结束时恢复此前值。结果保存在 `build/live-probes/<编号>/results.json`，仅含测试文本、候选、状态、耗时、置信度和可能的上屏文本。`uncertain` 或 `abstained` 表示 API 已回应但未采用，不能等同于推荐成功。

默认诊断时限为 3000 ms，独立工具也可指定 `--timeout-ms 3000`。这不修改正式配置。2026-09-22 的两次原生真实请求分别耗时 714 / 809 ms，默认 600 ms 在这次环境中不足；这些少量样本不构成延迟分布或中文准确率评测。

读取输出中的 `status`：`recommended` 表示答案通过校验；`missing_api_key`、`timeout`、`uncertain`、`abstained`、`http_XXX` 等表示不采用。命令执行成功不等于模型选对，仍需中文歧义词数据集评测。工具无推荐时不伪造上屏结果。

本版无设置界面、托盘错误提示、请求缓存、全应用敏感输入检测或安装器。现有验证范围见 `verification.md`。
