# TypePick

基于 **Rime／小狼毫二次开发**的 Windows 中文输入法项目：Rime 生成候选，TypePick 在后台结合本次连续输入的上下文推荐候选，用户按 **Tab** 采用。

当前是 Windows x64 MVP，提供独立安装包、设置 EXE、简体拼音词库和 Jev 候选推荐。构建会在干净的 Windows 环境中执行安装、注册、真实服务进程选词及卸载检查；实际桌面应用兼容性仍需继续验收。首版不含 32 位或 ARM64 输入组件。

## 安装使用

运行 `TypePick-Setup-0.1.0-x64.exe`，从桌面打开“TypePick 设置”，点击“保存并启动”，再按 **Win + 空格**切换到 TypePick。若未显示，注销后重新登录。设置中导入自己的 `.env`（支持一行 `key="..."`），勾选 AI 推荐并保存；在记事本中出现 AI 浮窗时按 Tab 采用。密钥只存入本机 Windows 凭据管理器，不随安装包分发。

安装包及对应源码由 GitHub Actions 的 `TypePick-Windows-x64` artifact 交付。构建和验证范围见 [Windows 发布说明](docs/windows-release.md)，操作步骤见 [使用说明](packaging/使用说明.txt)。

## 第一版交互

1. 正常输入拼音，Rime 按原顺序显示候选。
2. 停顿 150 ms 后，后台请求 Jev，从当前页候选中选择，允许弃权。
3. 达到置信度和概率差阈值时，单独显示 `TypePick AI · 推荐词 [Tab 采用]`。
4. Tab 接受；空格、数字键仍交给原输入法。继续输入、失焦、换页、编辑或切换应用时，旧推荐作废。

AI 默认关闭。本阶段只允许在 **notepad.exe** 中启用；其他应用继续使用普通 Rime 输入。扩大应用范围前，需要接入 TSF 密码框和输入范围信号并做兼容性测试。

## 快速验证（Windows x64 / PowerShell 7）

需要 Visual Studio C++ Build Tools、Windows SDK、CMake/Ninja 组件和 Git。

```powershell
git clone --recurse-submodules https://github.com/hetaodw/TypePick.git
cd TypePick
./scripts/Build-Core.ps1
./scripts/Test-Rime.ps1
```

第二个脚本自动下载并校验固定版本的官方 librime。它运行五项真实引擎测试：两种上下文的 Tab 提交，以及失焦、输入变化、应用变化后的拒绝提交。测试使用项目自编的小词库和明确标注的 **demo 演示推荐**，不调用 Jev，也不注册系统输入法。

测试结果写入 `build/probes/<本次编号>/results.json`。测试工具为 `build/core-native/TypePickProbe.exe`。

## 小狼毫二次开发

上游以 Git submodule 固定在 `d73f6295e8252ed2f7b9c12bae32e9001b1afdaa`，接入变更保存在 [patches/weasel-typepick.patch](patches/weasel-typepick.patch)。

```powershell
./scripts/Prepare-Weasel.ps1
# 在已安装 ATL、已编译 x64 静态 Boost 的开发机上：
./scripts/Build-Weasel.ps1 -BoostRoot C:/deps/boost_1_84_0
```

脚本在 `build/weasel` 生成开发副本，接入按键、上屏、焦点和候选变化事件。构建目标为小狼毫 x64 服务端及 TSF DLL；没有自动安装、替换已有输入法或制作安装器。

完整依赖、配置及当前限制见 [开发说明](docs/development.md)，数据流见 [架构说明](docs/architecture.md)，当前验证结果见 [验证记录](docs/verification.md)。

## Jev 配置与数据范围

完成小狼毫开发构建后，将 [config/typepick.example.json](config/typepick.example.json) 复制为小狼毫**用户资料目录**中的 `typepick.json`，将 `enabled` 改为 `true`。`mode: "demo"` 是本地演示；`mode: "jev"` 才会调用远端模型。重启开发版服务端后生效。

Jev 密钥从服务端进程的 `TYPESAFE_API_KEY` 环境变量读取，服务端本身不读取 `.env`，也不把密钥写入配置或日志。API 契约采用 [TypeSafe 官方文档](https://docs.typesafe.ai/introduction/quickstart)。

本地联调支持项目根目录 `.env` 中仅一行 `key="你的密钥"`，运行 `./scripts/Test-Jev.ps1` 即可。脚本只向测试进程临时注入环境变量，结束后恢复，不执行文件内容。它发送两个固定测试句子，默认采用 3000 ms **诊断时限**；不改变输入法的默认 600 ms 时限或 0.7 置信度门槛。实际请求及 Rime 上屏已验证，详见验证记录。

远端请求包含当前拼音、当前页最多 10 个候选，以及本输入法在当前连续输入段中已经上屏的最近约 100 个 UTF-16 单元。不会读取整篇文档；本段文字仍可能含私人信息，应只在愿意发送到模型服务的文本上启用。焦点、导航和会话变化会清空上下文。网络故障、密钥缺失、超时或低置信度时不显示推荐，普通输入继续工作。

## 许可证与商用

TypePick 原创代码采用 **AGPL-3.0-only**，全文见 [LICENSE](LICENSE)。允许商用、收费和修改；其他人也可以依同一许可证商用。分发须履行相应源码及许可证义务；修改 AGPL 软件并让用户通过网络与其交互时，还须依第 13 条向这些用户提供对应源码获取机会。

小狼毫保留其 GPLv3 声明，librime 保留 BSD 声明，其他依赖保留各自条款。GPLv3 与 AGPLv3 的组合按两份许可证第 13 条处理，不把上游代码声明为 TypePick 独有。详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。

输入法生成的普通文字和文档不因使用 TypePick 而适用 AGPL。关于页署名遵循标准许可证要求；本项目没有另加“必须展示 TypePick 品牌”的自定义条款。
