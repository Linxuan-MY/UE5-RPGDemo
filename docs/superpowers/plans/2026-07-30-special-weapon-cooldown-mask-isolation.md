# Special Weapon Cooldown Mask Isolation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Prevent an unrelated special-weapon cooldown broadcast from changing another ability slot's in-progress cooldown-mask percentage.

**Architecture:** Keep the shared `OnAbilityCooldownBegin` multicast and the existing per-slot gameplay-tag filter. Once an event passes that filter, copy its total cooldown into a widget member owned by that slot. The latent countdown may continue across frames, but its mask calculation must read this slot-owned snapshot rather than the custom-event parameter stored on the Blueprint persistent frame.

**Tech Stack:** Unreal Engine 5.4, UMG Widget Blueprint, Gameplay Tags, Blueprint bytecode disassembly, PowerShell regression harness, temporary editor-only C++ asset migration helper.

---

### Task 1: Add a compiled-Blueprint regression check

**Files:**
- Create: `Tools/verify_cooldown_widget_isolation.py`
- Create: `Tools/Test-CooldownWidgetIsolation.ps1`
- Test: `Content/Widgets/TemplateWidgets/TPWBP_AbilityIconSlot.uasset`

**Step 1: Write the failing check**

Create an Unreal Python script which loads
`/Game/Widgets/TemplateWidgets/TPWBP_AbilityIconSlot`, disassembles
`TPWBP_AbilityIconSlot_C`, and exits the editor. Create a PowerShell wrapper which launches a full editor process, reads the isolated log, and checks that:

- compiled bytecode contains the instance float `ActiveCooldownTotalTime`;
- the value converted for `MapRangeClamped.InRangeA` comes from
  `ActiveCooldownTotalTime`;
- that conversion block does not read
  `K2Node_CustomEvent_TotalCooldownTime`.

**Step 2: Run the check and confirm RED**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File .\Tools\Test-CooldownWidgetIsolation.ps1
```

Expected: non-zero exit with a focused message explaining that the slot-owned
total-time snapshot is absent or is not the `MapRangeClamped` denominator.

### Task 2: Apply the Widget Blueprint migration

**Files:**
- Modify temporarily: `Source/RPGDemo/RPGDemo.Build.cs`
- Create temporarily: `Source/RPGDemo/Private/Editor/CooldownWidgetBlueprintFix.cpp`
- Modify asset: `Content/Widgets/TemplateWidgets/TPWBP_AbilityIconSlot.uasset`

**Step 1: Add an editor-only migration command**

Under `Target.bBuildEditor`, add only the editor modules required to edit and
compile Blueprints. Implement an idempotent editor command which:

1. loads `TPWBP_AbilityIconSlot`;
2. adds a float member named `ActiveCooldownTotalTime`;
3. locates `OnAbilityCooldownBegin`, the matching-tag branch, `CountDown`, and
   `MapRangeClamped`;
4. inserts
   `Set ActiveCooldownTotalTime = TotalCooldownTime` on the branch's true
   execution path before `CountDown`;
5. replaces the `MapRangeClamped.InRangeA` data source with
   `Get ActiveCooldownTotalTime`;
6. compiles the Widget Blueprint and saves it only when compilation succeeds.

Do not change the branch condition, `CountDown.TotalTime`, gameplay effects, or
the multicast declaration.

**Step 2: Build and run the migration**

Build `RPGDemoEditor Win64 Development`, run the command in
`UnrealEditor-Cmd.exe`, and require explicit success/compile messages in the
editor log.

**Step 3: Remove migration-only source**

After the `.uasset` is saved, delete the temporary editor command and restore
`RPGDemo.Build.cs` to its original dependency set. The compiled asset and the
regression check are the durable changes.

### Task 3: Verify behavior and project health

**Files:**
- Test: `Tools/Test-CooldownWidgetIsolation.ps1`
- Test: `Content/Widgets/TemplateWidgets/TPWBP_AbilityIconSlot.uasset`
- Test: `Source/RPGDemo/RPGDemo.Build.cs`

**Step 1: Run the regression check and confirm GREEN**

Run the same PowerShell check. Expected: it proves that the mask denominator is
the widget member, while the shared event parameter is confined to the accepted
event path.

**Step 2: Compile/validate the Blueprint**

Load and compile `TPWBP_AbilityIconSlot` in an editor command session. Treat any
Blueprint compiler error as failure.

**Step 3: Build the editor target**

Run:

```powershell
& 'E:\UE engine\UE_5.4\Engine\Build\BatchFiles\Build.bat' RPGDemoEditor Win64 Development 'E:\UE_Projects\RPGDemo\RPGDemo.uproject' -WaitMutex -NoHotReload
```

Expected: successful build after all migration-only source has been removed.

**Step 4: Review the final diff**

Confirm that the final change set contains the Widget Blueprint, regression
scripts, and this plan only, in addition to the user's pre-existing changes.
Confirm that no generated logs, temporary migration source, or build artifacts
are staged as task output.

