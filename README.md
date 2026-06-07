# RPGDemo

RPGDemo 是一个基于 Unreal Engine 5.4 的第三人称动作 RPG Demo 项目。工程以 C++ Gameplay 框架为核心，配合 Blueprint 资产搭建角色、输入、Gameplay Ability System、战斗组件、武器生成、动画层和基础敌人流程。

## 环境要求

- Unreal Engine：5.4
- 目标平台：桌面端，当前工程配置以 Windows / DX12 为主
- IDE：Visual Studio，所需组件见 `.vsconfig`
- 项目插件：
  - GameplayAbilities
  - ModelingToolsEditorMode，仅编辑器使用
- 版本管理：
  - `.uasset`、`.umap` 等 Unreal 二进制资产需要通过 Git LFS 拉取

## 当前内容

- C++ Runtime 模块：`RPGDemo`
- 默认地图：`/Game/Maps/FeatureDevMap`
- 默认 GameMode：`/Game/GameModes/BP_BaseGameMode`
- GameMode 当前指定：
  - 默认 Pawn：`BP_HeroCharacter`
  - PlayerController：`BP_HeroController`
- 角色基类实现 `IAbilitySystemInterface`，集中持有：
  - `URPGDemoAbilitySystemComponent`
  - `URPGDemoAttributeSet`
  - `CharacterStartUpData`
- Ability System Debug 配置已启用 HUD target：
  - `bUseDebugTargetFromHud=true`

## Gameplay Ability System

项目当前已有一条可复用的 GAS 启动管线：

- 自定义 `URPGDemoAbilitySystemComponent`
- 自定义 `URPGDemoAttributeSet`
- 角色被 Possess 时初始化 `AbilityActorInfo`
- StartUpData 负责向 ASC 授予能力和应用启动 GameplayEffect
- 支持两类能力激活策略：
  - `OnTriggered`：由输入或事件触发
  - `OnGiven`：授予能力时自动激活，结束后清理
- 按 Gameplay Tag 绑定输入和 Ability
- 支持武器能力的动态授予与移除

当前 AttributeSet 包含：

- `CurrentHealth`
- `MaxHealth`
- `CurrentRage`
- `MaxRage`
- `AttackPower`
- `DefensePower`

## 输入系统

项目使用 Enhanced Input，并通过 Gameplay Tag 组织输入绑定：

- `UDataAsset_InputConfig` 保存输入 Action 与 Tag 的映射
- `URPGDemoInputComponent` 封装 Native Input 和 Ability Input 绑定
- Hero 角色在 `SetupPlayerInputComponent` 中注册默认 Mapping Context
- 输入按下后转发到 `URPGDemoAbilitySystemComponent::OnAbilityInputPressed`

当前主要输入包括：

- Move
- Look
- Equip Axe
- Unequip Axe
- Light Attack Axe
- Heavy Attack Axe

## 角色

### Hero

英雄角色当前负责玩家侧的移动、镜头、输入和战斗入口：

- Capsule、Spring Arm、Follow Camera
- Enhanced Input 绑定
- HeroCombatComponent
- Hero StartUpData 同步加载
- 武器能力按输入 Tag 绑定
- Axe 装备、卸下、轻攻击、重攻击相关能力和蒙太奇资产

### Enemy

敌人角色当前已有基础 C++ 和 Blueprint 管线：

- `ARPGDemoEnemyCharacter`
- `UEnemyCombatComponent`
- 自动 AI Possess：`PlacedInWorldOrSpawned`
- Enemy StartUpData 异步加载
- Enemy GameplayAbility 基类
- Guardian 敌人 Blueprint、动画蓝图、启动数据、启动 GameplayEffect 和生成武器能力

当前主要敌人资产位于：

- `Content/EnemyCharacter/BP_EnemyCharacter_Base.uasset`
- `Content/EnemyCharacter/BP_EnemyWeapon_Base.uasset`
- `Content/EnemyCharacter/Gruntling/BP_Gruntling_Base.uasset`
- `Content/EnemyCharacter/Gruntling/BP_Gruntling_Guardian.uasset`
- `Content/EnemyCharacter/Gruntling/DA_Guardian.uasset`

## 战斗和武器

当前战斗框架以 PawnCombatComponent 为基础：

- 使用 Gameplay Tag 注册角色持有的武器
- 保存当前装备武器 Tag
- 支持按 Tag 查询武器
- 基础武器 Actor 提供 Mesh 和碰撞盒
- Hero 和 Enemy 分别有自己的 CombatComponent 派生类
- 通用武器生成能力位于 Shared GameplayAbility 目录

相关资产包括：

- `Content/PlayerCharacter/HeroWeapon/BP_HeroWeaponBase.uasset`
- `Content/PlayerCharacter/HeroWeapon/BP_HeroAxe.uasset`
- `Content/EnemyCharacter/BP_EnemyWeapon_Base.uasset`
- `Content/EnemyCharacter/Gruntling/BP_Guardian_Weapon.uasset`
- `Content/Shared/GameplayAbility/GA_Shared_SpawnWeapon_Base.uasset`

## 动画

动画侧当前包含：

- 基础 AnimInstance
- 角色移动状态读取
- Hero AnimInstance
- Hero Linked Anim Layer
- Axe 装备状态下的 BlendSpace 和 AnimLayer
- Enemy 基础动画蓝图
- Guardian 动画蓝图和默认 BlendSpace
- AnimNotify / AnimNotifyState 辅助资产

相关目录：

- `Content/PlayerCharacter/AnimBP/`
- `Content/EnemyCharacter/ABP_Enemy_Base.uasset`
- `Content/EnemyCharacter/Gruntling/AnimBP/`
- `Content/Shared/AnimNotify/`
- `Content/Shared/AnimNotifyState/`

## Gameplay Tags

Native Gameplay Tags 当前覆盖：

- 输入标签
- 玩家能力标签
- 玩家武器标签
- 玩家事件标签
- 玩家状态标签
- 敌人武器标签

主要定义文件：

- `Source/RPGDemo/Public/RPGDemoGameplayTags.h`
- `Source/RPGDemo/Private/RPGDemoGameplayTags.cpp`

## 项目结构

```text
RPGDemo.uproject              项目描述文件
Config/                       项目、地图、输入、渲染和 GAS 配置
Source/RPGDemo/               Runtime C++ 模块
Content/Maps/                 当前功能开发地图
Content/GameModes/            Blueprint GameMode
Content/PlayerCharacter/      英雄角色、输入、能力、GE、动画和武器资产
Content/EnemyCharacter/       敌人角色、敌人武器、Guardian 资产和敌人动画资产
Content/Shared/               通用 GameplayAbility、GameplayEffect 和动画通知资产
Content/Assets/               导入的角色、敌人、武器、特效、UI、声音和动画资源
Content/ThirdPerson/          第三人称模板原型资源
Tools/                        项目辅助脚本
```

## 关键资产

- 默认地图：`Content/Maps/FeatureDevMap.umap`
- 默认 GameMode：`Content/GameModes/BP_BaseGameMode.uasset`
- 英雄角色：`Content/PlayerCharacter/BP_HeroCharacter.uasset`
- 英雄控制器：`Content/PlayerCharacter/BP_HeroController.uasset`
- 英雄启动数据：`Content/PlayerCharacter/DA_Hero.uasset`
- 英雄启动 GE：`Content/PlayerCharacter/GameplayEffect/GE_Hero_StartUp.uasset`
- 输入配置：`Content/PlayerCharacter/DA_InputConfig.uasset`
- 默认输入映射：`Content/PlayerCharacter/Input/IMC_Default.uasset`
- Axe 输入映射：`Content/PlayerCharacter/Input/IMC_Axe.uasset`
- Guardian 敌人：`Content/EnemyCharacter/Gruntling/BP_Gruntling_Guardian.uasset`
- Guardian 启动数据：`Content/EnemyCharacter/Gruntling/DA_Guardian.uasset`
- Guardian 启动 GE：`Content/EnemyCharacter/Gruntling/GameplayEffect/GE_Guardian_StartUp.uasset`

## C++ 模块概览

主要代码区域：

- `Characters/`：基础角色、英雄角色、敌人角色
- `AbilitySystem/`：AttributeSet、AbilitySystemComponent、GameplayAbility 基类、Hero/Enemy Ability 基类
- `Components/Input/`：Enhanced Input 绑定辅助组件
- `Components/Combat/`：Pawn、Hero、Enemy 战斗组件
- `Items/Weapons/`：基础武器和英雄武器类
- `DataAssets/`：输入配置和角色启动数据
- `AnimInstances/`：基础动画实例、角色动画实例、Hero 动画实例和 Linked Anim Layer
- `Controllers/`：Hero PlayerController
- `GameModes/`：基础 GameMode
- `RPGDemoGameplayTags`：Native Gameplay Tag 声明和定义
- `RPGDemoFunctionLibrary`：项目通用 Blueprint/C++ 辅助函数

## 打开项目

1. 安装 Unreal Engine 5.4。
2. 安装 Git LFS，并确保已拉取二进制资产。
3. 打开 `RPGDemo.uproject`。
4. 如果 Visual Studio 项目文件缺失或过期，从 `.uproject` 重新生成项目文件。
5. 编译 `RPGDemoEditor` 目标。

新机器首次使用仓库前，先执行：

```powershell
git lfs install
git lfs pull
```

## Git 说明

本仓库按 Unreal Engine 项目配置版本管理规则：

- `Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`、`.vs/` 等生成目录会被忽略。
- Unreal 二进制资产通过 Git LFS 跟踪。
- `RPGDemo.sln` 可以重新生成，不作为核心源文件依赖。
- 建议纳入版本管理的核心内容包括：`Config/`、`Source/`、`Content/`、`RPGDemo.uproject`、`.gitignore`、`.gitattributes`、`.editorconfig`。

## 当前状态

项目仍处于原型开发阶段。当前主要完成了 Hero 输入到能力的基础管线、GAS Attribute/GameplayEffect 启动流程、武器生成和装备基础、Hero/Enemy 战斗组件骨架，以及 Guardian 敌人的初步角色、动画、启动数据和能力资产。
