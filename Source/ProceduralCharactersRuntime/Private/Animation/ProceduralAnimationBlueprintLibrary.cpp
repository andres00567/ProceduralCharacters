#include "Animation/ProceduralAnimationBlueprintLibrary.h"

#include "Animation/AnimInstance.h"
#include "Components/ProceduralCharacterComponent.h"

FProceduralCharacterState UProceduralAnimationBlueprintLibrary::GetProceduralStateFromAnimInstance(
    const UAnimInstance* AnimInstance)
{
    const APawn* Pawn = AnimInstance ? AnimInstance->TryGetPawnOwner() : nullptr;
    const UProceduralCharacterComponent* Component =
        Pawn ? Pawn->FindComponentByClass<UProceduralCharacterComponent>() : nullptr;
    return Component ? Component->GetThreadSafeSnapshot() : FProceduralCharacterState();
}
