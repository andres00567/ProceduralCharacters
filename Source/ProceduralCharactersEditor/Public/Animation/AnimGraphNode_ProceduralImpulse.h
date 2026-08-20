#pragma once

#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "Animation/AnimNode_ProceduralImpulse.h"
#include "AnimGraphNode_ProceduralImpulse.generated.h"

UCLASS()
class PROCEDURALCHARACTERSEDITOR_API UAnimGraphNode_ProceduralImpulse : public UAnimGraphNode_SkeletalControlBase
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Settings") FAnimNode_ProceduralImpulse Node;

    virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
    virtual FText GetTooltipText() const override;
    virtual FString GetNodeCategory() const override;
    virtual void ValidateAnimNodeDuringCompilation(USkeleton* ForSkeleton, FCompilerResultsLog& MessageLog) override;

protected:
    virtual FText GetControllerDescription() const override;
    virtual const FAnimNode_SkeletalControlBase* GetNode() const override { return &Node; }
};
