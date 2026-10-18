#include "Input/RPGDemoLocalInput.h"

#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

namespace RPGDemoLocalInput
{
    bool OwnsActiveViewport(const APlayerController* Controller)
    {
        const ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
        const UGameViewportClient* Viewport = Player ? Player->ViewportClient.Get() : nullptr;
        const TSharedPtr<SViewport> Widget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
        if (!Widget || !FSlateApplication::IsInitialized()) return false;
        FSlateApplication& Slate = FSlateApplication::Get();
        const TSharedPtr<SWindow> Window = Slate.FindWidgetWindow(Widget.ToSharedRef());
        return Window && Window->IsActive() && Slate.GetActiveTopLevelWindow() == Window;
    }

    void SetInputMode(APlayerController* Controller, ERPGDemoInputMode Mode, bool bRequestFocus, TSharedPtr<SWidget> WidgetToFocus)
    {
        ULocalPlayer* Player = Controller ? Controller->GetLocalPlayer() : nullptr;
        UGameViewportClient* Viewport = Player ? Player->ViewportClient : nullptr;
        if (!Controller || !Controller->IsLocalController() || !Viewport) return;

        const bool bGameOnly = Mode == ERPGDemoInputMode::GameOnly;
        const bool bUIOnly = !bGameOnly && Controller->GetNetMode() == NM_Standalone;
        // These settings belong to this GameViewportClient and do not change the
        // shared Slate user's focused widget, mouse capture or mouse lock.
        Viewport->SetIgnoreInput(bUIOnly);
        Viewport->SetMouseCaptureMode(bGameOnly ? EMouseCaptureMode::CapturePermanently :
            (bUIOnly ? EMouseCaptureMode::NoCapture : EMouseCaptureMode::CaptureDuringMouseDown));
        Viewport->SetMouseLockMode(bGameOnly ? EMouseLockMode::LockOnCapture : EMouseLockMode::DoNotLock);
        Viewport->SetHideCursorDuringCapture(bGameOnly);

        if (!bRequestFocus || !OwnsActiveViewport(Controller)) return;

        if (bGameOnly) Controller->SetInputMode(FInputModeGameOnly());
        else if (bUIOnly)
        {
            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(WidgetToFocus);
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            Controller->SetInputMode(InputMode);
        }
        else
        {
            FInputModeGameAndUI InputMode;
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            InputMode.SetHideCursorDuringCapture(false);
            InputMode.SetWidgetToFocus(WidgetToFocus);
            Controller->SetInputMode(InputMode);
        }
    }
}
