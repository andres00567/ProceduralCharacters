#include "Animation/AnimGraphNode_ProceduralLookChain.h"

#include "Kismet2/CompilerResultsLog.h"

#define LOCTEXT_NAMESPACE "ProceduralLookChain"

FText UAnimGraphNode_ProceduralLookChain::GetNodeTitle(ENodeTitleType::Type) const
{
    return GetControllerDescription();
}

FText UAnimGraphNode_ProceduralLookChain::GetTooltipText() const
{
    return LOCTEXT("Tooltip", "Distributes a budgeted native look-target correction over a semantic bone chain.");
}

FString UAnimGraphNode_ProceduralLookChain::GetNodeCategory() const
{
    return TEXT("Procedural Animation");
}

FText UAnimGraphNode_ProceduralLookChain::GetControllerDescription() const
{
    return LOCTEXT("Title", "Procedural Look Chain");
}

void UAnimGraphNode_ProceduralLookChain::ValidateAnimNodeDuringCompilation(
    USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog)
{
    if (Node.LookBones.IsEmpty())
    {
        MessageLog.Warning(TEXT("@@ requires at least one look bone."), this);
    }
    Super::ValidateAnimNodeDuringCompilation(ForSkeleton, MessageLog);
}

#undef LOCTEXT_NAMESPACE
