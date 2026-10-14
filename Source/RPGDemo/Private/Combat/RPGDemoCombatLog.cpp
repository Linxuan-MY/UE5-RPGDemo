#include "Combat/RPGDemoCombatLog.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY(LogRPGDemoCombat);

static TAutoConsoleVariable<int32> CVarCombatDebug(
    TEXT("rpgdemo.Combat.Debug"), 0,
    TEXT("Log authoritative collision, damage and local hit presentation. Disabled by default."));

bool RPGDemoCombat::IsDebugEnabled()
{
    return CVarCombatDebug.GetValueOnGameThread() != 0;
}
