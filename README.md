# RPGDemo

RPGDemo 是一个基于 Unreal Engine 5.4 的第三人称动作 RPG Demo 项目。当前工程重点是搭建 C++ Gameplay 基础框架，并配合 Blueprint 资产实现角色、输入、能力、动画层和武器相关流程。

## 环境要求

- Unreal Engine：5.4
- 目标平台：桌面端，当前配置包含 Windows / DX12 相关设置
- IDE：Visual Studio，所需组件见 `.vsconfig`
- 项目插件：
  - GameplayAbilities
  - ModelingToolsEditorMode，仅编辑器使用
- 版本管理：
  - 项目中的 `.uasset`、`.umap` 等 Unreal 二进制资产需要使用 Git LFS

## 当前内容

- 基础角色类实现 `IAbilitySystemInterface`
- 自定义 `URPGDemoAbilitySystemComponent`
- 初步 Gameplay Ability System 流程：
  - 角色启动能力数据资产
  - 使用 Gameplay Tag 绑定英雄能力
  - 输入触发能力激活
  - 支持 `OnTriggered` 和 `OnGiven` 两种能力激活策略
- Enhanced Input 集成：
  - 自定义 `URPGDemoInputComponent`
  - 按 Gameplay Tag 绑定原生输入
  - 按 Gameplay Tag 转发能力输入按下/释放事件
- 英雄角色基础配置：
  - Capsule、Spring Arm、Follow Camera
  - 移动和视角输入
  - 英雄战斗组件
- 战斗和武器基础框架：
  - Pawn 战斗组件
  - 使用 Gameplay Tag 管理角色携带武器
  - 当前装备武器标记
  - 基础武器 Actor，包含 Mesh 和碰撞盒
  - 英雄斧头相关 Blueprint 资产
- 动画相关基础：
  - 基础 AnimInstance
  - 角色移动动画数据
  - 英雄 AnimInstance
  - 支持武器动画 Linked Anim Layer
- 原生 Gameplay Tags：
  - 输入标签
  - 玩家武器标签
  - 玩家事件标签

## 项目结构

```text
RPGDemo.uproject              项目描述文件
Config/                       项目、输入、地图和渲染配置
Source/RPGDemo/               Runtime C++ 模块
Content/Maps/                 当前功能开发地图
Content/GameModes/            Blueprint GameMode
Content/PlayerCharacter/      英雄角色、输入、能力、动画和武器资产
Content/Shared/               通用 Blueprint Gameplay 资产
Content/ThirdPerson/          第三人称模板内容
Content/Assets/               导入的动作 RPG 美术和动画资源
```

## 关键资产

- 默认编辑器/游戏地图：`Content/Maps/FeatureDevMap.umap`
- 默认 GameMode：`Content/GameModes/BP_BaseGameMode.uasset`
- 英雄角色 Blueprint：`Content/PlayerCharacter/BP_HeroCharacter.uasset`
- 英雄控制器 Blueprint：`Content/PlayerCharacter/BP_HeroController.uasset`
- 英雄启动数据：`Content/PlayerCharacter/DA_Hero.uasset`
- 输入配置数据：`Content/PlayerCharacter/DA_InputConfig.uasset`
- 默认输入映射：`Content/PlayerCharacter/Input/IMC_Default.uasset`
- 英雄能力：
  - `Content/PlayerCharacter/GameplayAbility/GA_Hero_SpawnAxe.uasset`
  - `Content/PlayerCharacter/GameplayAbility/GA_Hero_EquipAxe.uasset`
- 通用生成武器能力：`Content/Shared/GameplayAbility/GA_Shared_SpawnWeapon.uasset`

## C++ 模块概览

Runtime 模块名为 `RPGDemo`。

主要代码区域：

- `Characters/`：基础角色和英雄角色
- `AbilitySystem/`：AttributeSet、AbilitySystemComponent 和 GameplayAbility 基类
- `Components/Input/`：Enhanced Input 绑定辅助组件
- `Components/Combat/`：战斗组件和英雄战斗组件
- `Items/Weapons/`：基础武器和英雄武器类
- `DataAssets/`：输入配置和角色启动能力数据
- `AnimInstances/`：动画实例和英雄 Linked Anim Layer
- `RPGDemoGameplayTags`：原生 Gameplay Tag 声明和定义

## 打开项目

1. 安装 Unreal Engine 5.4。
2. 安装 Git LFS，确保能正确拉取 Unreal 二进制资产。
3. 打开 `RPGDemo.uproject`。
4. 如果 Visual Studio 项目文件缺失或过期，从 `.uproject` 重新生成项目文件。
5. 编译 `RPGDemoEditor` 目标。

## Git 说明

本仓库已按 Unreal Engine 项目配置版本管理规则：

- `Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`、`.vs/` 等生成目录会被忽略。
- Unreal 二进制资产通过 Git LFS 跟踪。
- `RPGDemo.sln` 会被忽略，因为它可以重新生成。
- 建议纳入版本管理的核心内容包括：`Config/`、`Source/`、`Content/`、`RPGDemo.uproject`、`.gitignore`、`.gitattributes`、`.editorconfig`。

新机器首次使用仓库前，先执行：

```powershell
git lfs install
```

## 当前状态

项目仍处于原型开发阶段。当前代码和资产主要完成了英雄角色、输入到能力的基础管线、初步 GAS 集成，以及后续战斗系统所需的武器和动画基础结构。
