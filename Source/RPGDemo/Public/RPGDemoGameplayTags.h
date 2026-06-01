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

	/** Player Tags **/
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Weapon_Axe);

	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Event_Equip_Axe);
	RPGDEMO_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Player_Event_UnEquip_Axe);

}