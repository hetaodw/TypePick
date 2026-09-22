# 验证记录

日期：2026-09-22。Windows x64；MSVC 19.51.36244；CMake 4.2；librime 1.17.0。

| 项目 | 结果 |
| --- | --- |
| 原生模块、WinHTTP 客户端、推荐窗口、TypePickProbe 编译链接 | 通过 |
| 候选/配置/答案校验、默认关闭、防抖、跨会话旧结果丢弃、撤销、不阻塞提交、超时、越界、异常回退 | 原生测试通过 |
| Rime 输入 `yanjiu`，上下文“这个问题需要进一步”，演示推荐后 Tab | 实际上屏“研究” |
| Rime 输入 `yanjiu`，上下文“这家商店主要卖”，演示推荐后 Tab | 实际上屏“烟酒” |
| 已显示推荐后失焦、原始输入变化、应用变为 chrome.exe | 三种场景均拒绝旧推荐，没有提交文本 |
| Tab 采用后的松开事件 | 被消耗，不泄漏到应用 |
| 小狼毫固定版本补丁应用、重复准备源码 | 通过 |
| Jev 远端请求和中文推荐质量 | 未验证，当前没有配置 TYPESAFE_API_KEY |
| 完整 WeaselServer / TSF 构建 | 未完成，本机缺少 ATL；官方组件下载发生 TLS 连接失败 |
| 注册系统输入法、真实记事本/浏览器/Office/游戏兼容性 | 未执行 |

Rime 测试调用的是真实 `rime.dll`；推荐来源为本地 demo 固定规则。这验证了候选选择和窗口/Tab 桥接，不代表已经验证小狼毫 IPC、实际 TSF 安装或模型效果。

运行 `scripts/Test-Rime.ps1` 会重新生成独立的 `build/probes/<编号>/results.json`，包含每次测试的输入、上下文、commit 或 rejected 字段。`scripts/Build-Core.ps1` 执行原生测试。不要把本记录理解为正式发布验收。
