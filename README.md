# RPGDemo

RPGDemo 是一个基于 Unreal Engine 5.4 的第三人称动作 RPG Demo 项目。工程以 C++ Gameplay 框架为核心，配合 Blueprint 资产搭建角色、输入、Gameplay Ability System、战斗组件、武器生成、命中检测、伤害结算、受击与死亡反馈、角色状态 UI、动画层和基础敌人流程。

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
- 近战命中链路已接入：
  - 武器碰撞检测
  - GameplayEvent 驱动的伤害和 Hit Pause
  - GAS ExecutionCalculation 伤害结算
  - 生命值扣减和敌人受击反应
- 角色 UI 链路已接入：
  - Hero Overlay、生命值和怒气状态栏
  - 装备武器图标
  - 敌人头顶生命值条
  - AttributeSet 通过 UI Component 委托广播属性变化
- 敌人死亡流程已接入：
  - 生命值归零后添加 `Shared.Status.Dead`
  - 通用敌人死亡能力和 Guardian 死亡能力
  - Guardian 死亡蒙太奇和 Gameplay Cue 音效

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
- `DamageTaken`

GAS 伤害流程当前包括：

- `URPGDemoGameplayAbility` 支持把已构建的 GameplayEffect Spec 应用到目标 Actor
- `URPGDemoHeroGameplayAbility` 支持根据武器基础伤害、攻击类型和连击段数构建伤害 Spec
- `GE_Shared_DealDamage` 使用 `UGE_ExecCalc_DamageTaken` 计算伤害
- 伤害计算会读取 Source `AttackPower`、Target `DefensePower` 和 SetByCaller 传入的基础伤害
- 轻攻击和重攻击可通过不同 SetByCaller Tag 按连击段数调整基础伤害
- `PostGameplayEffectExecute` 负责夹紧 Health/Rage、把 `DamageTaken` 扣减到 `CurrentHealth`，并广播 UI 百分比变化
- 生命值归零时为目标添加 `Shared.Status.Dead`，供死亡能力响应

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
- HeroUIComponent
- Hero StartUpData 同步加载
- 武器能力按输入 Tag 绑定
- Axe 装备、卸下、轻攻击、重攻击相关能力和蒙太奇资产
- Hero Overlay、生命值/怒气状态栏和装备武器图标

### Enemy

敌人角色当前已有基础 C++ 和 Blueprint 管线：

- `ARPGDemoEnemyCharacter`
- `UEnemyCombatComponent`
- 自动 AI Possess：`PlacedInWorldOrSpawned`
- Enemy StartUpData 异步加载
- Enemy GameplayAbility 基类
- Guardian 敌人 Blueprint、动画蓝图、启动数据、启动 GameplayEffect 和生成武器能力
- Guardian HitReact 能力和受击蒙太奇
- EnemyUIComponent 和敌人头顶生命值 WidgetComponent
- 通用敌人死亡能力、Guardian 死亡能力和死亡蒙太奇

当前主要敌人资产位于：

- `Content/EnemyCharacter/BP_EnemyCharacter_Base.uasset`
- `Content/EnemyCharacter/BP_EnemyWeapon_Base.uasset`
- `Content/EnemyCharacter/Gruntling/BP_Gruntling_Base.uasset`
- `Content/EnemyCharacter/Gruntling/BP_Gruntling_Guardian.uasset`
- `Content/EnemyCharacter/Gruntling/DA_Guardian.uasset`
- `Content/EnemyCharacter/Gruntling/GameplayAbility/GA_Guardian_HitReact.uasset`
- `Content/EnemyCharacter/Gruntling/GameplayAbility/GA_Guardian_Death.uasset`
- `Content/EnemyCharacter/Gruntling/Montages/AM_ExoGame_Gruntling_React_Light_Front.uasset`
- `Content/EnemyCharacter/Gruntling/Montages/AM_ExoGame_Gruntling_React_Light_Front_Alt.uasset`
- `Content/EnemyCharacter/Gruntling/Montages/Death/`

## 战斗和武器

当前战斗框架以 PawnCombatComponent 为基础：

- 使用 Gameplay Tag 注册角色持有的武器
- 保存当前装备武器 Tag
- 支持按 Tag 查询武器
- 基础武器 Actor 提供 Mesh 和碰撞盒
- 武器碰撞盒通过 BeginOverlap / EndOverlap 回调通知 CombatComponent
- `ToggleWeaponCollision` 支持在蒙太奇窗口中开启或关闭当前装备武器碰撞
- CombatComponent 会记录本次攻击已命中的 Actor，避免同一窗口重复命中
- Hero 和 Enemy 分别有自己的 CombatComponent 派生类
- Hero CombatComponent 可查询当前装备武器和按等级读取武器基础伤害
- 通用武器生成能力位于 Shared GameplayAbility 目录

相关资产包括：

- `Content/PlayerCharacter/HeroWeapon/BP_HeroWeaponBase.uasset`
- `Content/PlayerCharacter/HeroWeapon/BP_HeroAxe.uasset`
- `Content/EnemyCharacter/BP_EnemyWeapon_Base.uasset`
- `Content/EnemyCharacter/Gruntling/BP_Guardian_Weapon.uasset`
- `Content/PlayerCharacter/HeroWeapon/CT_HeroWeaponStats.uasset`
- `Content/Shared/GameplayAbility/GA_Shared_SpawnWeapon_Base.uasset`

## 命中、伤害和反馈

当前近战攻击链路：

- 攻击蒙太奇通过 `ANS_ToggleWeaponCollision` 控制武器碰撞窗口
- `ARPGDemoWeaponBase` 在碰撞开始和结束时派发命中/拔出委托
- `UHeroCombatComponent` 接收命中后发送 `Shared.Event.MeleeHit`
- 命中和武器拔出目标时都会发送 `Player.Event.HitPause`
- Hero 攻击能力根据武器基础伤害、轻/重攻击类型和连击段数构建伤害 Spec
- `GE_Shared_DealDamage` 通过 `UGE_ExecCalc_DamageTaken` 完成最终伤害计算
- `URPGDemoAttributeSet` 将 `DamageTaken` 转换为 `CurrentHealth` 扣减
- 敌人侧通过 Shared/Guardian HitReact 能力播放受击反应
- Hero 命中反馈已接入 Hit Pause GameplayAbility 和近战 Camera Shake
- Axe 命中和 Guardian 死亡音效通过 Gameplay Cue 播放
- `DefaultGame.ini` 已配置 `/Game/GameplayCues` 扫描路径

相关资产包括：

- `Content/Shared/AnimNotifyState/ANS_ToggleWeaponCollision.uasset`
- `Content/Shared/GameplayEffect/GE_Shared_DealDamage.uasset`
- `Content/Shared/GameplayAbility/GA_Enemy_HitReact_Base.uasset`
- `Content/PlayerCharacter/GameplayAbility/HitPause/GA_Hero_HitPause.uasset`
- `Content/PlayerCharacter/GameplayAbility/HitPause/CameraShake_HeroMelee.uasset`
- `Content/GameplayCues/GC_Hero_AxeHit.uasset`
- `Content/GameplayCues/GC_Guardian_DeathSound.uasset`

## UI 系统

角色状态 UI 使用 C++ Component 和 Blueprint Widget 组合：

- `IPawnUIInterface` 为角色提供统一的 UI Component 访问入口
- `UPawnUIComponent` 广播生命值百分比变化
- `UHeroUIComponent` 额外广播怒气百分比和装备武器图标变化
- `UEnemyUIComponent` 为敌人状态 Widget 提供数据源
- `URPGDemoWidgetBase` 在初始化时把 Hero/Enemy UI Component 传递给 Blueprint Widget
- `URPGDemoAttributeSet` 在属性变化和伤害结算后广播 Health/Rage 百分比
- 英雄装备数据通过 `SoftWeaponIconTexture` 配置武器图标

当前 UI 资产包括：

- `Content/Widgets/HeroWidgets/WBP_HeroOverlay.uasset`
- `Content/Widgets/EnemyWidgets/WBP_DefaultEnemyHealthBar.uasset`
- `Content/Widgets/TemplateWidgets/TPWBP_StatusBar.uasset`
- `Content/Widgets/TemplateWidgets/TPWBP_IconSlot.uasset`
- `Content/Widgets/TemplateWidgets/RPGDemoSizeBox.uasset`
- `Content/Assets/Textures/UI/Material/MI_HeroHealthBar.uasset`

## 敌人死亡

当前敌人死亡链路：

- `URPGDemoAttributeSet` 检测生命值归零并添加 `Shared.Status.Dead`
- `GA_Enemy_Death_Base` 提供共享死亡能力流程
- `GA_Guardian_Death` 负责 Guardian 的具体死亡表现
- Guardian 使用独立死亡蒙太奇目录提供多种死亡动画
- `GC_Guardian_DeathSound` 播放死亡音效
- `BPI_EnemyDeath` 为 Blueprint 死亡事件提供接口

相关资产包括：

- `Content/Shared/GameplayAbility/GA_Enemy_Death_Base.uasset`
- `Content/EnemyCharacter/BPI_EnemyDeath.uasset`
- `Content/EnemyCharacter/Gruntling/GameplayAbility/GA_Guardian_Death.uasset`
- `Content/EnemyCharacter/Gruntling/Montages/Death/`
- `Content/GameplayCues/GC_Guardian_DeathSound.uasset`

## 动画

动画侧当前包含：

- 基础 AnimInstance
- 角色移动状态读取
- Hero AnimInstance
- Hero Linked Anim Layer
- Axe 装备状态下的 BlendSpace 和 AnimLayer
- Enemy 基础动画蓝图
- Guardian 动画蓝图和默认 BlendSpace
- Guardian 受击反应蒙太奇
- Guardian 死亡蒙太奇
- AnimNotify / AnimNotifyState 辅助资产

相关目录：

- `Content/PlayerCharacter/AnimBP/`
- `Content/EnemyCharacter/ABP_Enemy_Base.uasset`
- `Content/EnemyCharacter/Gruntling/AnimBP/`
- `Content/EnemyCharacter/Gruntling/Montages/`
- `Content/Shared/AnimNotify/`
- `Content/Shared/AnimNotifyState/`

## Gameplay Tags

Native Gameplay Tags 当前覆盖：

- 输入标签
- 玩家能力标签
- 玩家武器标签
- 玩家事件标签
- 玩家状态标签
- 玩家 Hit Pause 和攻击类型 SetByCaller 标签
- 共享能力、事件、状态和 SetByCaller 标签
- 敌人能力标签
- 敌人武器标签
- Gameplay Cue 音效标签

主要定义文件：

- `Source/RPGDemo/Public/RPGDemoGameplayTags.h`
- `Source/RPGDemo/Private/RPGDemoGameplayTags.cpp`

当前新增的关键标签覆盖：

- `Shared.Event.MeleeHit`
- `Shared.Event.HitReact`
- `Shared.SetByCaller.BaseDamage`
- `Shared.Ability.Death`
- `Shared.Status.Dead`
- `Player.Event.HitPause`
- `Player.Ability.HitPause`
- `Player.SetByCaller.AttackType.Light`
- `Player.SetByCaller.AttackType.Heavy`
- `Shared.Ability.HitReact`
- `GameplayCue.Sounds.MeleeHit.Axe`
- `GameplayCue.Sounds.Death.Guardian`

## 项目结构

```text
RPGDemo.uproject              项目描述文件
Config/                       项目、地图、输入、渲染和 GAS 配置
Source/RPGDemo/               Runtime C++ 模块
Content/Maps/                 当前功能开发地图
Content/GameModes/            Blueprint GameMode
Content/PlayerCharacter/      英雄角色、输入、能力、GE、动画和武器资产
Content/EnemyCharacter/       敌人角色、敌人武器、Guardian 资产、受击和死亡资产
Content/Shared/               通用 GameplayAbility、GameplayEffect 和动画通知资产
Content/Widgets/              Hero/Enemy 状态 UI 和通用 Widget 模板
Content/GameplayCues/         命中和死亡 Gameplay Cue
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
- Guardian HitReact 能力：`Content/EnemyCharacter/Gruntling/GameplayAbility/GA_Guardian_HitReact.uasset`
- Guardian 死亡能力：`Content/EnemyCharacter/Gruntling/GameplayAbility/GA_Guardian_Death.uasset`
- 通用敌人死亡能力：`Content/Shared/GameplayAbility/GA_Enemy_Death_Base.uasset`
- 共享伤害 GE：`Content/Shared/GameplayEffect/GE_Shared_DealDamage.uasset`
- Hero Hit Pause 能力：`Content/PlayerCharacter/GameplayAbility/HitPause/GA_Hero_HitPause.uasset`
- Hero 近战 Camera Shake：`Content/PlayerCharacter/GameplayAbility/HitPause/CameraShake_HeroMelee.uasset`
- Hero Overlay：`Content/Widgets/HeroWidgets/WBP_HeroOverlay.uasset`
- 敌人生命值条：`Content/Widgets/EnemyWidgets/WBP_DefaultEnemyHealthBar.uasset`
- Hero Axe 命中 Gameplay Cue：`Content/GameplayCues/GC_Hero_AxeHit.uasset`
- Guardian 死亡 Gameplay Cue：`Content/GameplayCues/GC_Guardian_DeathSound.uasset`
- 武器碰撞窗口 ANS：`Content/Shared/AnimNotifyState/ANS_ToggleWeaponCollision.uasset`
- Hero 武器数值表：`Content/PlayerCharacter/HeroWeapon/CT_HeroWeaponStats.uasset`

## C++ 模块概览

主要代码区域：

- `Characters/`：基础角色、英雄角色、敌人角色
- `AbilitySystem/`：AttributeSet、AbilitySystemComponent、GameplayAbility 基类、Hero/Enemy Ability 基类
- `AbilitySystem/GE_ExecCalc/`：GAS 自定义伤害执行计算
- `Components/Input/`：Enhanced Input 绑定辅助组件
- `Components/Combat/`：Pawn、Hero、Enemy 战斗组件
- `Components/UI/`：Pawn、Hero、Enemy UI 数据和委托组件
- `Interfaces/`：角色战斗组件和 UI 组件访问接口
- `Items/Weapons/`：基础武器和英雄武器类
- `DataAssets/`：输入配置和角色启动数据
- `AnimInstances/`：基础动画实例、角色动画实例、Hero 动画实例和 Linked Anim Layer
- `Controllers/`：Hero PlayerController
- `GameModes/`：基础 GameMode
- `RPGDemoTypes/`：项目通用枚举和结构体
- `RPGDemoGameplayTags`：Native Gameplay Tag 声明和定义
- `RPGDemoFunctionLibrary`：项目通用 Blueprint/C++ 辅助函数
- `Widgets/`：C++ UserWidget 基类和 UI Component 初始化桥接

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

项目仍处于原型开发阶段。当前主要完成了 Hero 输入到能力的基础管线、GAS Attribute/GameplayEffect 启动流程、武器生成和装备基础、Hero/Enemy 战斗组件骨架、近战命中检测、伤害计算、血量扣减、Hit Pause/Camera Shake/Gameplay Cue 战斗反馈、Guardian 受击与死亡流程，以及 Hero Overlay、角色状态栏、装备武器图标和敌人头顶生命值条。
