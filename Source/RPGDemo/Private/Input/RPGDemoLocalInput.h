#pragma once

#include "CoreMinimal.h"
#include "RPGDemoTypes/RPGDemoEnumTypes.h"

class APlayerController;
class SWidget;

namespace RPGDemoLocalInput
{
    // Lifecycle callers configure only their viewport. Only an explicit operation
    // in the active player window may submit focus/capture operations to Slate.
    bool OwnsActiveViewport(const APlayerController* Controller);
    void SetInputMode(APlayerController* Controller, ERPGDemoInputMode Mode, bool bRequestFocus,
        TSharedPtr<SWidget> WidgetToFocus = nullptr);
}
