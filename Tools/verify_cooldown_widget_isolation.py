import unreal


ASSET_PATH = "/Game/Widgets/TemplateWidgets/TPWBP_AbilityIconSlot"
GENERATED_CLASS_NAME = "TPWBP_AbilityIconSlot_C"


widget_blueprint = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
if widget_blueprint is None:
    unreal.log_error(
        f"COOLDOWN_WIDGET_CHECK_ASSET_LOAD_FAILED: could not load {ASSET_PATH}"
    )
else:
    unreal.log("COOLDOWN_WIDGET_CHECK_COMPILATION_BEGIN")
    unreal.BlueprintEditorLibrary.compile_blueprint(widget_blueprint)
    unreal.log("COOLDOWN_WIDGET_CHECK_COMPILATION_END")
    unreal.log("COOLDOWN_WIDGET_CHECK_DISASSEMBLY_BEGIN")
    unreal.SystemLibrary.execute_console_command(
        widget_blueprint,
        f"DISASMSCRIPT {GENERATED_CLASS_NAME}",
    )
    unreal.log("COOLDOWN_WIDGET_CHECK_DISASSEMBLY_END")

unreal.SystemLibrary.execute_console_command(widget_blueprint, "QUIT_EDITOR")
