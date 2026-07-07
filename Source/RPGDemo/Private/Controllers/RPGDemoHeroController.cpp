// Fill out your copyright notice in the Description page of Project Settings.


#include "Controllers/RPGDemoHeroController.h"

ARPGDemoHeroController::ARPGDemoHeroController()
{
	HeroTeamId = FGenericTeamId(0);
}

FGenericTeamId ARPGDemoHeroController::GetGenericTeamId() const
{
    return HeroTeamId;
}
