#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ProceduralAnimationRetargetCommandlet.generated.h"

/** Duplicates an animation sequence onto a compatible target skeleton. */
UCLASS()
class UProceduralAnimationRetargetCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UProceduralAnimationRetargetCommandlet();
    virtual int32 Main(const FString& Params) override;
};
