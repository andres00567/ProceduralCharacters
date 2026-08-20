#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/ProceduralCharacterTypes.h"
#include "ProceduralAnimationBlueprintLibrary.generated.h"

class UAnimInstance;

UCLASS()
class PROCEDURALCHARACTERSRUNTIME_API UProceduralAnimationBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Call during AnimBP Update Animation, cache the value, and wire it to native procedural nodes. */
    UFUNCTION(BlueprintPure, Category = "Procedural Animation", meta = (BlueprintThreadSafe = false))
    static FProceduralCharacterState GetProceduralStateFromAnimInstance(const UAnimInstance* AnimInstance);
};
