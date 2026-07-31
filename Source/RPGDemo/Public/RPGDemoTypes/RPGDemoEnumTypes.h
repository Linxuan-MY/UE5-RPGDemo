#pragma once

UENUM()
enum class ERPGDemoConfirmType : uint8
{
	Yes,
	No
};

UENUM()
enum class ERPGDemoValidType : uint8
{
	Valid,
	Invalid
};

UENUM()
enum class ERPGDemoSuccessType : uint8
{
	Successful,
	Failed
};

UENUM()
enum class ERPGDemoCountDownActionInput : uint8
{
	Start,
	Cancel
};

UENUM()
enum class ERPGDemoCountDownActionOutput : uint8
{
	Updated,
	Completed,
	Cancelled
};

UENUM(BlueprintType)
enum class ERPGDemoGameDifficulty : uint8
{
	Easy,
	Normal,
	Hard,
	ExtremelyHard
};

