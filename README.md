# RPGDemo

RPGDemo 是基于 Unreal Engine 5.8 源码版的第三人称动作 RPG 示例项目，结合 C++、Blueprint、Gameplay Ability System（GAS）、Enhanced Input、Behavior Tree、EQS、AI Perception 和 UMG，实现单人生存玩法与 2–4 人 Windows Dedicated Server（DS）联机。

## 玩法与联机流程

客户端默认从 `MainMenuMap` 主菜单启动。单人模式选择难度后进入生存地图；多人模式通过主菜单的 Multiplayer 连接 DS，进入大厅，由房主设置难度并开始游戏。默认服务端地址为 `127.0.0.1:7777`，当前使用本机 Windows DS。

生存模式包含数据驱动的敌人波次、Guardian 近战敌人、Glacer 远程敌人及 Frost Giant Boss。四档难度为 Easy、Normal、Hard 和 Extremely Hard。

多人模式中，一名玩家死亡后，只要队伍还有存活玩家，就进入等待重生阶段；全队死亡才判定本局失败。正常重生等待为 10 秒。重生恢复初始携带武器状态，需要重新按 `1` 装备，再使用轻重攻击与武器特殊技能。特殊技能使用输入资产配置的组合键。

## 系统

- Hero：移动、镜头、斧类武器装备与卸下、轻重连击、怒气、特殊技能、治疗石和怒气石范围拾取。
- 战斗：GAS 能力、属性、GameplayEffect、伤害计算、输入标签、武器碰撞窗口、Hit React、Gameplay Cue、Hit Pause 和 Camera Shake。
- AI：AI Perception、Behavior Tree、Blackboard、EQS、NavMesh、Detour Crowd Avoidance 和队伍关系；TargetLock 仅选择存活敌人。
- 多人生命周期：PlayerState 持久 ASC、Pawn 重建、武器重新注册、死亡状态复制、尸体姿态与溶解、团灭判定和重生失败重试。
- UI：各本地玩家独立 HUD、拾取提示、技能与冷却、Boss 血条、波次提示、重生倒计时，以及主菜单、大厅、暂停、胜负和加载界面。

## 地图

| 地图 | 用途 | Windows 发布脚本显式 Cook |
| --- | --- | --- |
| `MainMenuMap` | 客户端默认启动地图和主菜单 | 是 |
| `MultiplayerLobbyMap` | DS 默认地图、房主与难度设置 | 是 |
| `SurvivalGameModeMap` | 单人及多人波次生存 | 是 |
| `CombatTestMap` | 战斗与网络自动化测试 | 否 |
| `FeatureDevMap` | 功能开发 | 否 |

## 源码环境

- Windows，Unreal Engine 5.8 源码版；脚本默认引擎路径为 `E:\UE Source\UnrealEngine-5.8`。
- Visual Studio 或 Rider；Visual Studio 组件配置见 [.vsconfig](.vsconfig)。
- .NET 10 SDK；配置脚本会选择源码引擎自带的实际 SDK 版本。
- Git LFS，用于 `.uasset`、`.umap` 等二进制资产。
- 当前维护和验证的 DS 平台为 Windows。

首次拉取后执行：

```powershell
git lfs install
git lfs pull
```

先在源码引擎目录运行 `Setup.bat`，再在项目根目录关联引擎并生成项目文件：

```powershell
pwsh -File .\Tools\Configure-SourceEngine.ps1 -EngineRoot 'E:\UE Source\UnrealEngine-5.8' -BuildEditor
```

[Configure-SourceEngine.ps1](Tools/Configure-SourceEngine.ps1) 更新 `EngineAssociation` 和本机 `global.json`，并生成项目文件；`-BuildEditor` 额外编译 `RPGDemoEditor`。`global.json` 包含本机 SDK 路径，不提交到仓库。

`global.json` 的 `sdk.paths` 需要 .NET 10 或更高版本的 `dotnet` 主机。Rider 如仍选择旧 SDK，可将 .NET CLI 指向引擎的 `Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe`，MSBuild 使用其 `sdk\<实际 SDK 版本>\MSBuild.dll`，然后重新加载项目。

打开 `RPGDemo.uproject`。项目默认使用 Desktop、DX12 和 SM6 配置。

## Windows 打包与手动联机

在项目根目录分别打包客户端和服务端：

```powershell
pwsh -File .\Tools\Build-WindowsClient.ps1 -Configuration Shipping
pwsh -File .\Tools\Build-WindowsServer.ps1 -Configuration Development
```

脚本均接受 `-EngineRoot`、`-BuildId` 和 `-MaxParallelActions`。输出分别位于 `Artifacts/WindowsClient/`、`Artifacts/WindowsServer/`，包含程序目录、ZIP、SHA-256、发布清单及 `release-source.json` 源码快照。构建期间源码发生变化时，脚本会拒绝将结果作为一致的发布输出。

手动测试步骤：

1. 在服务端包内运行 `Start-Server.cmd`，默认只监听本机 `127.0.0.1:7777`。
2. 保持完整客户端目录，在客户端包内执行以下命令；双客户端测试时运行两次。
3. 在各自客户端主菜单点击 Multiplayer，进入大厅，由房主开始游戏。

```powershell
.\RPGDemo.exe -RPGDemoServer=127.0.0.1:7777 -windowed -ResX=960 -ResY=540
```

客户端入口保持主菜单，不通过地图或连接 URL 跳过菜单。地址也可在 `Config/DefaultGame.ini` 的 `DedicatedServerEndpoint` 配置，`-RPGDemoServer=` 参数可覆盖该地址。完整包需一起保留，不能只复制可执行文件。

**2026-10-18 打包状态：** 本轮已通过 Editor、Windows Development 和 Shipping 原生程序编译，尚未重新 Cook 或生成新的 Shipping 归档包；已有 `Artifacts` 客户端和历史 Release 不会自动包含本轮修复。

历史体验包：[GitHub Release v1.0.0](https://github.com/Linxuan-MY/UE5-RPGDemo/releases/tag/v1.0.0)，文件名为 `RPGDemo-v1.0.0-Windows-Shipping.zip`。该历史包不代表当前分支功能或最新修复。

## PIE 焦点与重生输入修复

同进程 PIE 窗口共享键鼠对应的 Slate 用户。初始化或重生时无条件调用 `SetInputMode(GameOnly)` 会改变共享用户焦点，表现为输入落到另一个窗口，或存活玩家暂时失去控制。当前将角色输入恢复与焦点操作分开，菜单焦点操作限定于所属活动视口。

重生后不能装备的另一条原因是旧武器映射残留：基础映射优先级为 0，`1` 表示装备；武器映射优先级为 1，`1` 表示卸下并消费输入。旧 Pawn 失去控制后跳过映射移除，就会阻挡新 Pawn 的装备动作。

当前由本地 `RPGDemoHeroController` 通过 `SyncPawnInputContexts` / `ReleasePawnInputContexts` 管理基础与武器映射。死亡、失去控制、更换 Pawn 和退出时清理旧记录；迟到的旧 Pawn 注销不会影响新 Pawn，其他系统的映射保留。该修复没有新增生产 RPC 或复制字段，保留原键位、冷却、重生时长与服务器武器授予规则。

## 回归测试

在 Editor 自动化测试中运行 `RPGDemo.Network.InputLifecycle`。测试创建隔离 DS 和两个同进程 Standalone PIE 窗口，经主菜单连接入口进入大厅及真实生存地图，结束后恢复使用前的相关 PIE 设置。

测试覆盖两名玩家各三轮死亡与重生，分别在携带武器、装备武器、装备动画中死亡。检查另一玩家持续按住 W 时的焦点与移动、重生后首次按 `1` 装备、卸下、轻重攻击、特殊技能，以及映射隔离和旧尸体清理。输入通过视口键事件进入 Enhanced Input，特殊技能组合键从输入资产解析。

独立 DS 和客户端进程的生命周期回归：

```powershell
pwsh -File .\Tools\Test-Lifecycle.ps1 -Clients 2 -KeyInput
pwsh -File .\Tools\Test-Lifecycle.ps1 -Clients 2 -KeyInput -PacketLag 100 -PacketLoss 2
```

[Test-Lifecycle.ps1](Tools/Test-Lifecycle.ps1) 默认启动 Windows Editor DS 和两个 `-game` 客户端，使用隔离端口 `17777`，日志保存在 `Saved/Automation/Lifecycle/`。可通过 `-Port` 更换端口，通过 `-ServerExecutable`、`-ClientExecutable` 指定支持测试开关的 Development 程序。`-KeyInput` 通过视口键事件检查能力；未指定时保留原有直接能力激活测试路径。Shipping 不启用生命周期测试夹具，不能用作该脚本的测试客户端。

脚本启动的测试客户端会直接连接测试房间，以便自动跑测；这与手动启动 Shipping 客户端先进入主菜单的发布流程不同。另有 `-RespawnRegression` 可检查出生点阻塞、重生失败重试和团灭判定。

2026-10-18 验证结果：

| 验证 | 结果 |
| --- | --- |
| Editor Development、Windows Development、Windows Shipping 原生编译 | 通过 |
| `RPGDemo.Network.InputLifecycle` | 六轮通过，245.442 秒，0 错误 |
| 独立 Windows Editor DS＋双客户端，普通网络 | 120 条通过记录，0 失败 |
| 同上，100ms 延迟＋2% 丢包 | 120 条通过记录，0 失败 |

独立进程测试使用新 Development 客户端程序和已有基线包的只读 Cook 资源，覆盖重生后的武器操作、AI、UI、拾取、团灭、返回大厅再次开局及断线清理；不代表完成了新发布包的完整验收。

**验证范围：** 六轮 PIE 测试中的 DS 也位于编辑器进程内；外部 DS 测试使用两个独立客户端进程。尚未对“同进程双 Standalone PIE 连接外部 DS”组合完整重复六轮，也未完成 Windows Alt-Tab、任务栏切窗和鼠标捕获的全面人工验收。详细日志与自动化结果保留在本地 `Saved/Automation/`，不提交构建产物。

## 目录与版本控制

```text
Source/RPGDemo/
  Public/             对外接口与类型声明
  Private/
    AbilitySystem/    GAS、属性、伤害计算和 AbilityTask
    AI/               行为树任务与服务
    Characters/       玩家、敌人与角色生命周期
    Components/       战斗、动作输入和 UI 组件
    Controllers/      玩家、AI 与大厅控制器
    GameModes/        大厅、生存、波次与重生
    Input/            本地视口输入模式与焦点处理
    Items/            武器、投射物和拾取物
    Tests/            战斗、AI、网络及输入回归

Content/
  PlayerCharacter/    Hero、输入、能力、动画和 UI 数据
  EnemyCharacter/     Guardian、Glacer、Frost Giant、BT 与 EQS
  GameModes/          游戏模式与波次数据资产
  Maps/               主菜单、大厅、生存与测试地图
  Widgets/            菜单、HUD、状态栏和敌人 UI
  InfinityBladeIceLands/  生存场景环境资源
```

源码、配置、工具脚本与所需资产纳入版本控制；Unreal 资产通过 Git LFS 保存。`Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`、`Artifacts/`、根目录 `tmp/` / `Temp/` 和本机工具配置等按 [.gitignore](.gitignore) 排除。

## 资产说明

`Content/InfinityBladeIceLands` 用于 `SurvivalGameModeMap` 的环境内容。项目中的第三方资源按其原许可使用。
