#include "Animation/AnimGraphNode_ProceduralImpulse.h"

#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "ProceduralImpulse"

FText UAnimGraphNode_ProceduralImpulse::GetNodeTitle(ENodeTitleType::Type) const
{
    return GetControllerDescription();
}

FText UAnimGraphNode_ProceduralImpulse::GetTooltipText() const
{
    return LOCTEXT("Tooltip", "Applies budgeted recoil, landing, and hit impulses from the fixed component buffer.");
}

FString UAnimGraphNode_ProceduralImpulse::GetNodeCategory() const
{
    return TEXT("Procedural Animation");
}

FText UAnimGraphNode_ProceduralImpulse::GetControllerDescription() const
{
    return LOCTEXT("Title", "Procedural Impulse");
}

void UAnimGraphNode_ProceduralImpulse::ValidateAnimNodeDuringCompilation(
    USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
    if (Node.ImpulseBones.IsEmpty())
    {
        MessageLog.Warning(TEXT("@@ requires at least one impulse bone."), this);
    }
    Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

#undef LOCTEXT_NAMESPACE
