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

- Unreal Engine 5.4
- Visual Studio 或 Rider；Visual Studio 组件配置见 `.vsconfig`
- Git LFS，用于 `.uasset`、`.umap` 等二进制资产

首次拉取后执行：

```powershell
git lfs install
git lfs pull
```

打开 `RPGDemo.uproject`，按需重新生成项目文件并编译 `RPGDemoEditor`。项目默认使用 Desktop、DX12 和 SM6 配置。

## 资产说明

`Content/InfinityBladeIceLands` 用于 `SurvivalGameModeMap` 的环境内容。项目中的第三方资源按其原许可使用。
