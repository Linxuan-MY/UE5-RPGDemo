// Fill out your copyright notice in the Description page of Project Settings.


#include "RPGDemoGameplayTags.h"

namespace RPGDemoGameplayTags
{
	/** Input Tags **/
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Move, "Input.Move");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Look, "Input.Look");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_EquipAxe, "Input.EquipAxe");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_UnEquipAxe, "Input.UnEquipAxe");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_LightAttack_Axe, "Input.LightAttack.Axe");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_HeavyAttack_Axe, "Input.HeavyAttack.Axe");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Avoid, "Input.Avoid");

	/** Player Tags **/
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Equip_Axe, "Player.Ability.Equip.Axe");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_UnEquip_Axe, "Player.Ability.UnEquip.Axe");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_LightAttack_Axe, "Player.Ability.Attack.Light.Axe");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_HeavyAttack_Axe, "Player.Ability.Attack.Heavy.Axe");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_HitPause, "Player.Ability.HitPause");
	UE_DEFINE_GAMEPLAY_TAG(Player_Ability_Avoid, "Player.Ability.Avoid");

	UE_DEFINE_GAMEPLAY_TAG(Player_Weapon_Axe, "Player.Weapon.Axe");

	UE_DEFINE_GAMEPLAY_TAG(Player_Event_Equip_Axe, "Player.Event.Equip.Axe");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_UnEquip_Axe, "Player.Event.UnEquip.Axe");
	UE_DEFINE_GAMEPLAY_TAG(Player_Event_HitPause, "Player.Event.HitPause");

	UE_DEFINE_GAMEPLAY_TAG(Player_Status_JumpToFinisher, "Player.Status.JumpToFinisher");
	UE_DEFINE_GAMEPLAY_TAG(Player_Status_Avoid, "Player.Status.Avoid");

	UE_DEFINE_GAMEPLAY_TAG(Player_SetByCaller_AttackType_Light, "Player.SetByCaller.AttackType.Light");
	UE_DEFINE_GAMEPLAY_TAG(Player_SetByCaller_AttackType_Heavy, "Player.SetByCaller.AttackType.Heavy");

	/** Enemy Tags **/
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Ability_Melee, "Enemy.Ability.Melee");
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Ability_Ranged, "Enemy.Ability.Ranged");

	UE_DEFINE_GAMEPLAY_TAG(Enemy_Weapon, "Enemy.Weapon");

	UE_DEFINE_GAMEPLAY_TAG(Enemy_Status_Strafe, "Enemy.Status.Strafe");
	UE_DEFINE_GAMEPLAY_TAG(Enemy_Status_UnderAttack, "Enemy.Status.UnderAttack");

	/** Shared Tags **/
	UE_DEFINE_GAMEPLAY_TAG(Shared_Ability_HitReact, "Shared.Ability.HitReact");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Ability_Death, "Shared.Ability.Death");

	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_MeleeHit, "Shared.Event.MeleeHit");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Event_HitReact, "Shared.Event.HitReact");

	UE_DEFINE_GAMEPLAY_TAG(Shared_SetByCaller_BaseDamage, "Shared.SetByCaller.BaseDamage");

	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_Dead, "Shared.Status.Dead");

	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_HitReact_Front, "Shared.Status.HitReact.Front");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_HitReact_Back, "Shared.Status.HitReact.Back");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_HitReact_Left, "Shared.Status.HitReact.Left");
	UE_DEFINE_GAMEPLAY_TAG(Shared_Status_HitReact_Right, "Shared.Status.HitReact.Right");
}
