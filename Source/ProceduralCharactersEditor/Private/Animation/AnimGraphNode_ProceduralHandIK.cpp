#include "Animation/AnimGraphNode_ProceduralHandIK.h"

#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "ProceduralHandIK"

FText UAnimGraphNode_ProceduralHandIK::GetNodeTitle(ENodeTitleType::Type) const
{
    return GetControllerDescription();
}

FText UAnimGraphNode_ProceduralHandIK::GetTooltipText() const
{
    return LOCTEXT("Tooltip", "Solves both FPS arms from weapon hand-grip positions; elbow and shoulder placement are derived procedurally.");
}

FString UAnimGraphNode_ProceduralHandIK::GetNodeCategory() const
{
    return TEXT("Procedural Animation");
}

FText UAnimGraphNode_ProceduralHandIK::GetControllerDescription() const
{
    return LOCTEXT("Title", "Procedural Two-Hand IK");
}

void UAnimGraphNode_ProceduralHandIK::ValidateAnimNodeDuringCompilation(
    USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
    if (Node.RightArm.HandBone.BoneName.IsNone() || Node.LeftArm.HandBone.BoneName.IsNone())
    {
        MessageLog.Warning(TEXT("@@ requires complete left and right arm chains."), this);
    }
    Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

#undef LOCTEXT_NAMESPACE
