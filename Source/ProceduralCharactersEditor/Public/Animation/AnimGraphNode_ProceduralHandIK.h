#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_ProceduralHandIK.h"
#include "AnimGraphNode_ProceduralHandIK.generated.h"

UCLASS()
class PROCEDURALCHARACTERSEDITOR_API UAnimGraphNode_ProceduralHandIK : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Settings") FAnimNode_ProceduralHandIK Node;

    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FString GetNodeCategory() const override;
    virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;

protected:
    virtual FText GetControllerDescription() const override;
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
