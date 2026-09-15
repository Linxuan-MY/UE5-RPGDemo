// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/RPGDemoWidgetBase.h"
#include "RPGDemoResultWidget.generated.h"

UCLASS()
class RPGDEMO_API URPGDemoResultWidget : public URPGDemoWidgetBase
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
};
