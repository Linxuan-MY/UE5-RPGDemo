// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimInstances/Hero/RPGDemoHeroLinkedAnimLayer.h"
#include "AnimInstances/Hero/RPGDemoHeroAnimInstance.h"

URPGDemoHeroAnimInstance* URPGDemoHeroLinkedAnimLayer::GetHeroAnimInstance() const
{
    return Cast<URPGDemoHeroAnimInstance>(GetOwningComponent()->GetAnimInstance());
}
