#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ProceduralAnimGraphCommandlet.generated.h"

/** Inspects and, with -Apply, safely inserts the standard procedural chain into an AnimBP. */
UCLASS()
class UProceduralAnimGraphCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    UProceduralAnimGraphCommandlet();
    virtual int32 Main(const FString& Params) override;
};
