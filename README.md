# RPGDemo

RPGDemo 是使用 Unreal Engine 5.4 开发的第三人称动作 RPG 示例项目。项目结合 C++、Blueprint、Gameplay Ability System、Enhanced Input、Gameplay Tags、Behavior Tree、EQS、AI Perception 和 UMG，实现角色战斗、敌人 AI、生存波次、难度存档与菜单流程。

## v1.0.0 可体验内容

Windows Shipping 版本从 `MainMenuMap` 启动。玩家可以选择难度并进入 `SurvivalGameModeMap`，完成分波次战斗；游戏内提供暂停、帮助、胜利和失败界面，并支持返回主菜单。关卡切换期间显示加载界面。

四档难度为 Easy、Normal、Hard 和 Extremely Hard。选择结果写入 SaveGame，并在生存模式初始化时读取。

## 系统

- Hero：移动、镜头、斧类武器装备、轻重连击、怒气、轻重特殊技能、治疗石和怒气石拾取。
- 战斗：GAS Ability、Attribute、GameplayEffect、ExecutionCalculation、Gameplay Tag 输入、武器碰撞窗口、Hit React、Gameplay Cue、Hit Pause 和 Camera Shake。
- 敌人：Guardian 近战、Glacer 远程投射物、Frost Giant Boss 多段攻击与按生命值召唤援军。
- AI：AI Perception、Behavior Tree、Blackboard、EQS、NavMesh、Detour Crowd Avoidance 和队伍关系。
- 生存模式：数据驱动的敌人波次、波次状态切换、难度倍率、胜负条件和界面。
- UI 与流程：主菜单、选项、帮助、暂停、胜利、失败、角色状态、敌人血条、Boss 血条和加载界面。

## 地图

| 地图 | 用途 | v1.0.0 Shipping |
| --- | --- | --- |
| `MainMenuMap` | 默认启动地图和主菜单 | 包含 |
| `SurvivalGameModeMap` | 生存波次玩法 | 包含 |
| `CombatTestMap` | 战斗、敌人和 Boss 测试 | 不显式 Cook |
| `FeatureDevMap` | 功能开发地图 | 不包含 |

## 目录

```text
Source/RPGDemo/
  AbilitySystem/      GAS、属性、伤害计算和 AbilityTask
  AI/                 Behavior Tree Task 与 Service
  Components/         战斗、输入和 UI 组件
  GameModes/          基础与生存模式逻辑
  Items/              武器、投射物和拾取物
  SaveGame/           难度存档

Content/
  PlayerCharacter/    Hero、输入、能力、动画和 UI 数据
  EnemyCharacter/     Guardian、Glacer、Frost Giant、BT 与 EQS
  GameModes/          主菜单、生存模式和波次数据资产
  Maps/               主菜单、生存、战斗测试和开发地图
  Widgets/            菜单、HUD、状态栏和敌人 UI
  InfinityBladeIceLands/  生存场景环境资源
```

## 下载

Windows Shipping 包：[GitHub Release v1.0.0](https://github.com/Linxuan-MY/UE5-RPGDemo/releases/tag/v1.0.0)

压缩包名称：`RPGDemo-v1.0.0-Windows-Shipping.zip`。

## 源码环境

- Unreal Engine 5.8 源码版；默认路径为 `E:\UE Source\UnrealEngine-5.8`
- Visual Studio 或 Rider；Visual Studio 组件配置见 `.vsconfig`
- .NET 10 SDK；项目 `global.json` 优先选择源码引擎自带的 `10.0.203` SDK
- Git LFS，用于 `.uasset`、`.umap` 等二进制资产

首次拉取后执行：

```powershell
git lfs install
git lfs pull
```

先在源码引擎中运行 `Setup.bat`，再关联引擎并生成项目文件：

```powershell
pwsh -File .\Tools\Configure-SourceEngine.ps1 -EngineRoot 'E:\UE Source\UnrealEngine-5.8'
```

脚本会同步更新 `EngineAssociation` 和 `global.json` 中的内置 SDK 路径与版本。添加 `-BuildEditor` 可编译 `RPGDemoEditor`。

`global.json` 的 `sdk.paths` 需要 .NET 10 或更高版本的 `dotnet` 主机；只有 .NET 10 运行时并不代表系统已安装 SDK。Rider 如仍选择旧 SDK，可在 **Settings → Build, Execution, Deployment → Toolset and Build** 中将 .NET CLI 指向引擎的 `Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe`，MSBuild 使用该 SDK 的 `sdk\10.0.203\MSBuild.dll` 或支持 .NET 10 的 Rider 内置版本，然后重新加载项目。

打开 `RPGDemo.uproject`。项目默认使用 Desktop、DX12 和 SM6 配置。

## 资产说明

`Content/InfinityBladeIceLands` 用于 `SurvivalGameModeMap` 的环境内容。项目中的第三方资源按其原许可使用。
