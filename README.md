# RPGDemo

RPGDemo 是一个基于 Unreal Engine 5.4 的第三人称动作 RPG 技术 Demo。项目重点不是堆叠完整游戏内容，而是验证一套可以扩展的动作 RPG Gameplay 架构：用 C++ 承担核心运行时框架，用 Blueprint 和数据资产承载角色、能力、动画、AI 行为和反馈表现。

当前 Demo 已形成一条可体验的玩法闭环：玩家角色使用斧类武器进行轻攻击和重攻击，敌人通过感知系统进入追击、转向、走位、近战攻击或远程投射物攻击、受击反馈和死亡流程，战斗结果同步驱动 UI、动画、Hit Pause、Camera Shake 和 Gameplay Cue。

## 技术栈

- Unreal Engine 5.4
- C++ Gameplay Framework
- Gameplay Ability System
- Enhanced Input
- Gameplay Tags
- AI Perception、Behavior Tree、Blackboard、EQS
- Detour Crowd Avoidance
- Motion Warping
- UMG
- Blueprint Data Assets、Animation Blueprint、Anim Notify / Anim Notify State
- Git LFS，用于管理 `.uasset`、`.umap` 等二进制资源

## 项目亮点

### GAS 驱动的角色能力框架

项目以 Gameplay Ability System 作为角色能力、属性和战斗结算的核心。基础角色统一持有自定义 `URPGDemoAbilitySystemComponent` 和 `URPGDemoAttributeSet`，Hero 与 Enemy 通过各自的 StartUpData 获得初始 Ability 和 GameplayEffect。

能力授予和激活并不直接绑定具体按键或蓝图节点，而是通过 Gameplay Tag 建立连接。Hero 的输入配置把 Enhanced Input Action 映射到 `Input.*` 标签，ASC 再根据 AbilitySpec 的 Dynamic Ability Tags 激活对应能力。武器装备后也可以动态授予和移除武器能力，使角色基础能力与武器能力保持解耦。

伤害结算使用自定义 ExecutionCalculation。攻击 Ability 构建 GameplayEffect Spec，通过 SetByCaller 写入基础伤害、攻击类型和连击段数；执行计算再读取 Source `AttackPower`、Target `DefensePower`，计算最终伤害并写回 `DamageTaken`。AttributeSet 在 `PostGameplayEffectExecute` 中完成生命值扣减、属性夹紧、UI 广播和死亡状态标签写入。

### 数据驱动的近战战斗链路

近战系统围绕 CombatComponent、武器 Actor、Gameplay Event 和 Animation Notify 组织。角色通过 Gameplay Tag 注册当前持有武器，攻击蒙太奇中的 `ANS_ToggleWeaponCollision` 控制武器碰撞窗口，武器碰撞盒只在有效攻击帧启用。

命中发生后，CombatComponent 会过滤重复命中目标，并向拥有者发送 `Shared.Event.MeleeHit`。Hero 侧还会额外发送 `Player.Event.HitPause`，用于驱动命中停顿和镜头反馈。最终伤害由共享 GameplayEffect 结算，敌人受击和死亡再由响应型 Ability 处理。这样的链路让“攻击输入、动画窗口、碰撞命中、伤害计算、反馈表现”保持清晰边界。

### 远程敌人与投射物链路

新增的 Glacer 敌人扩展了敌人 Ability 体系中的远程攻击分支。行为树可以通过距离和位置查询选择射击节奏，配合 `EQS_FindShootProjectileLocation` 寻找更合适的发射位置，再由 `GA_Glacer_Projectile` 驱动施法蒙太奇、攻击预警和投射物生成。

投射物使用 C++ 基类 `ARPGDemoProjectileBase` 承担通用飞行、碰撞和伤害派发逻辑。投射物支持 OnHit 与 OnBeginOverlap 两种伤害策略：Glacer 的投射物采用 Overlap 模式，对同阵营 Pawn 只穿透不结算，对玩家这类敌对 Pawn 才触发格挡判断、伤害 GameplayEffect、HitReact 和命中特效；对世界物体仍保留阻挡命中表现。

### 敌人 AI 行为闭环

敌人侧已经接入基础 AI 行为链路。`ARPGDemoAIController` 使用 AI Perception 进行视野感知，并通过 Generic Team Id 区分敌我关系；路径跟随使用 `UCrowdFollowingComponent`，并在项目配置中启用 CrowdManager 避障参数。

Behavior Tree 和 Blackboard 负责组织 Guardian 与 Glacer 的战斗状态。项目中包含原生 BTTask / BTService，用于面向目标旋转和持续朝向目标；蓝图行为树资产进一步组织追击、距离检测、EQS 走位、Strafe 状态切换、按标签激活敌人 Ability、以及攻击目标更新。敌人近战攻击同样复用共享命中事件和伤害 GameplayEffect，远程攻击复用共享投射物事件和伤害 GameplayEffect，因此玩家与敌人的战斗流程不是两套割裂实现。

### UI 与反馈事件流

UI 数据不直接散落在角色或 Widget 蓝图中，而是通过 `PawnUIComponent`、`HeroUIComponent`、`EnemyUIComponent` 和 `IPawnUIInterface` 建立统一的数据入口。AttributeSet 在属性变化时广播生命值、怒气等百分比，Widget 基类在初始化时获取对应 UI Component，Blueprint Widget 只负责表现层绑定。

战斗反馈也沿用事件化思路。Gameplay Cue 负责命中和死亡音效，Hit Pause Ability 和 Camera Shake 负责近战打击感，敌人死亡通过 `Shared.Status.Dead` 标签触发后续死亡能力。这样可以把战斗结果、状态变化和表现反馈串联起来，同时减少系统之间的直接依赖。

### C++ 与 Blueprint 的职责拆分

项目把稳定、可复用、需要强类型约束的逻辑放在 C++ 中，包括 ASC 扩展、AttributeSet、Ability 基类、CombatComponent、AIController、BTTask / BTService、输入组件、UI Component 和通用函数库。Blueprint 主要承担资产配置、Ability 具体表现、Behavior Tree 编排、Animation Blueprint、Widget 表现和 DataAsset 填表。

这种拆分使 Demo 既能体现 C++ 架构能力，也保留 UE 工作流中快速迭代内容的优势。

## 当前可体验内容

- 第三人称 Hero 角色移动、镜头控制、斧类武器装备和卸下
- 轻攻击、重攻击和连击段数参与伤害倍率
- 基于武器碰撞窗口的近战命中检测
- 命中停顿、近战 Camera Shake、命中音效 Gameplay Cue
- Hero 生命值、怒气和装备武器图标 UI
- Enemy 头顶生命值条
- Guardian 敌人感知玩家、追击、转向、走位、近战攻击
- Glacer 敌人感知玩家、选择射击位置、施法并发射投射物
- Guardian 受击反应、死亡 Ability、死亡蒙太奇和死亡音效
- EQS Strafe 走位、敌人近战 Ability 随行为树激活
- CombatTestMap 战斗测试地图，用于验证敌人组合、远程投射物和战斗行为

默认地图为 `/Game/Maps/FeatureDevMap`，默认 GameMode 为 `/Game/GameModes/BP_BaseGameMode`。

## 核心流程

### 玩家攻击流程

```text
Enhanced Input
  -> Input Gameplay Tag
  -> ASC 激活 Hero GameplayAbility
  -> 播放攻击蒙太奇
  -> AnimNotifyState 打开武器碰撞窗口
  -> Weapon overlap 过滤敌对目标
  -> CombatComponent 发送 MeleeHit / HitPause 事件
  -> GameplayEffect ExecutionCalculation 计算伤害
  -> AttributeSet 扣减生命值并广播 UI
  -> HitReact / Death / Gameplay Cue / Camera Shake
```

### 敌人战斗流程

```text
AI Perception 感知目标
  -> Blackboard 写入 TargetActor
  -> Behavior Tree 选择追击、转向、走位或攻击
  -> EQS 查询 Strafe 位置
  -> BTTask / BTService 调整朝向和状态
  -> 按 Gameplay Tag 激活敌人近战 Ability
  -> 复用共享近战命中和伤害链路
  -> 受击、死亡和 UI 反馈
```

### 敌人远程投射物流程

```text
AI Perception 感知目标
  -> Behavior Tree 判断射击时机和射击位置
  -> EQS 查询可用发射位置
  -> 按 Gameplay Tag 激活敌人远程 Ability
  -> 施法蒙太奇发送 SpawnProjectile 事件
  -> Projectile overlap 过滤敌我目标
  -> 命中玩家后执行格挡判断或应用伤害
  -> HitReact / Gameplay Cue / 命中特效
```

## 关键源码入口

```text
Source/RPGDemo/Private/AbilitySystem/
  自定义 ASC、AttributeSet、GameplayAbility 基类、伤害 ExecutionCalculation

Source/RPGDemo/Private/Components/Combat/
  Pawn/Hero/Enemy CombatComponent，武器注册、碰撞窗口、命中事件派发

Source/RPGDemo/Private/Components/Input/
  Enhanced Input 与 Gameplay Tag 绑定封装

Source/RPGDemo/Private/Components/UI/
  角色 UI 数据组件和属性变化委托

Source/RPGDemo/Private/AI/
  原生 Behavior Tree Task / Service

Source/RPGDemo/Private/Controllers/RPGDemoAIController.cpp
  AI 感知、队伍关系、Detour Crowd Avoidance 配置

Source/RPGDemo/Private/Items/Weapons/
  武器基础 Actor、碰撞盒和命中委托

Source/RPGDemo/Private/Items/RPGDemoProjectileBase.cpp
  投射物基础 Actor、飞行组件、命中/重叠策略和伤害派发

Source/RPGDemo/Private/RPGDemoGameplayTags.cpp
  Native Gameplay Tag 定义
```

## 关键内容资产

```text
Content/PlayerCharacter/
  Hero 角色、输入、Ability、GameplayEffect、动画、武器和 UI 数据

Content/EnemyCharacter/
  Enemy 基类资产、Guardian 与 Glacer 敌人、行为树、EQS、受击、攻击、投射物和死亡资产

Content/Shared/
  通用 GameplayAbility、GameplayEffect、投射物蓝图、AnimNotify 和 AnimNotifyState

Content/GameplayCues/
  命中、死亡和敌人攻击预警 Gameplay Cue

Content/Widgets/
  Hero Overlay、敌人生命值条和通用状态栏模板

Content/Maps/FeatureDevMap.umap
  当前功能开发和演示地图

Content/Maps/CombatTestMap.umap
  战斗测试地图，用于集中验证敌人行为和投射物链路
```

## 运行项目

1. 安装 Unreal Engine 5.4。
2. 安装 Git LFS，并拉取二进制资产。
3. 打开 `RPGDemo.uproject`。
4. 如项目文件过期，从 `.uproject` 重新生成 IDE 项目文件。
5. 编译 `RPGDemoEditor` 目标后打开默认地图运行。

首次拉取仓库后建议执行：

```powershell
git lfs install
git lfs pull
```

Windows 下当前配置以 Desktop / DX12 / SM6 为主。IDE 可使用 Visual Studio 或 Rider；Visual Studio 所需组件可参考 `.vsconfig`。

## 版本管理说明

仓库按 Unreal Engine 项目规则组织版本管理：

- `Config/`、`Source/`、`Content/`、`RPGDemo.uproject` 是核心工程内容。
- `.uasset`、`.umap` 等二进制资源通过 Git LFS 跟踪。
- `Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`、`.vs/`、`.idea/`、RiderLink 等本地生成内容不纳入版本管理。
- IDE 工程文件和本地缓存可以重新生成，不应作为理解项目架构的主要入口。

## 当前状态

项目处于功能 Demo 和架构验证阶段，已经完成 Hero 战斗、GAS 属性和伤害管线、武器生成与碰撞命中、UI 状态同步、Guardian 近战敌人 AI、Glacer 远程敌人 AI、EQS 走位、敌人近战与远程 Ability、投射物命中过滤、受击与死亡反馈等核心链路。

后续可继续扩展的方向包括：更多武器类型、格挡和破防、敌人技能组合、更多敌人 Archetype、Boss 行为、数值表完善、UI 菜单流程、存档和关卡目标系统。
