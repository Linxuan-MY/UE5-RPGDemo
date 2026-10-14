"""Verify evidence emitted by independent server/client processes, including skipped cases."""
import argparse
import json
import pathlib
import re

parser = argparse.ArgumentParser()
parser.add_argument("run_directory", type=pathlib.Path)
args = parser.parse_args()
run = json.loads((args.run_directory / "run.json").read_text(encoding="utf-8-sig"))
errors = []
counts = {}
requirements = {
    "Server": {"WavePreloadSpawn": 1, "AITracksCurrentPawn": 4, "AIDamagesCurrentPawn": 4,
               "TestCombatGroundAligned": 4, "GroundEnemyStaysUpright": 4,
               "OwnerWeaponDamagesEnemy": 4, "RespawnInitiallyCarried": 3, "RespawnTags": 3,
               "RespawnAvatarAndSpecs": 3, "RealStonesSpawn": 1, "StonesConsumedAndDestroyed": 1,
               "TeamWipeFailsMatch": 1, "TerminalEnemyCallbackIgnored": 1,
               "NewMatchAlive": 1, "NewMatchDifficulty": 1, "NewMatchStartupLevel": 1,
               "DisconnectDuringRespawn": 1, "DisconnectControllerRemoved": 1, "DisconnectWeaponCleanup": 1},
}
for role in range(1, run["Clients"] + 1):
    requirements[f"Client{role}"] = {"VisibleHealthHUD": 4, "VisibleRageHUD": 4,
                                    "TargetLockRejectsPlayer": 4, "PlayerDeathPoseHeld": 3}
    if role <= 2:
        requirements[f"Client{role}"].update(VisibleStonePrompt=1, ClientStonesRemoved=1)
    if role == 1:
        requirements[f"Client{role}"].update(OwnerEquip=4, OwnerLight=4, OwnerHeavy=4,
                                             WeaponInputMapping=8, WaveRespawnLayout=3,
                                             VisibleWeaponAbilityIcons=4, VisibleEquippedWeaponIcon=4)
for process, required in requirements.items():
    log = (args.run_directory / f"{process}.log").read_text(encoding="utf-8-sig", errors="replace")
    errors += [f"{process}: {line}" for line in log.splitlines() if "LIFECYCLE FAIL" in line]
    counts[process] = {}
    for case, minimum in required.items():
        count = len(re.findall(rf"LIFECYCLE PASS case={re.escape(case)}\s", log))
        counts[process][case] = count
        if count < minimum:
            errors.append(f"{process}: {case} ran {count} times, expected at least {minimum}")
    if process == "Client1":
        special = len(re.findall(r"LIFECYCLE PASS case=(?:OwnerSpecial|WeaponCooldownPreserved)\s", log))
        counts[process]["SpecialOrCooldown"] = special
        if special < 8:
            errors.append(f"Client1: special ability checks ran {special} times, expected 8")
    if process == "Server" and "LIFECYCLE COMPLETE failures=0" not in log:
        errors.append("Server: successful completion missing")
result = {"passed": not errors, "errors": errors, "counts": counts, "run": run}
(args.run_directory / "verification.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
print(json.dumps({"passed": result["passed"], "errors": errors}, indent=2))
raise SystemExit(bool(errors))
