// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

namespace RPGDemoGameplayTags
{
	/** Input Tags **/
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Move);
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Look);
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_EquipAxe);
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_UnEquipAxe);
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_LightAttack_Axe);
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_HeavyAttack_Axe);

	/** Player Tags **/
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Ability_Equip_Axe);
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Ability_UnEquip_Axe);
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Ability_LightAttack_Axe);
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Ability_HeavyAttack_Axe);

	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Weapon_Axe);

	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Event_Equip_Axe);
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Event_UnEquip_Axe);

	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Status_JumpToFinisher);

	/** Enemy Tags **/
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Weapon);



}
