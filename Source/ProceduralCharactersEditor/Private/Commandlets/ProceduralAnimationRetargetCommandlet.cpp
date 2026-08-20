#include "Commandlets/ProceduralAnimationRetargetCommandlet.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

UProceduralAnimationRetargetCommandlet::UProceduralAnimationRetargetCommandlet()
{
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
}

int32 UProceduralAnimationRetargetCommandlet::Main(const FString& Params)
{
    FString SourcePath;
    FString TargetSkeletonPath;
    FString DestinationPath;
    FParse::Value(*Params, TEXT("Source="), SourcePath);
    FParse::Value(*Params, TEXT("TargetSkeleton="), TargetSkeletonPath);
    FParse::Value(*Params, TEXT("Destination="), DestinationPath);

    UAnimSequence* Source = LoadObject<UAnimSequence>(nullptr, *SourcePath);
    USkeleton* TargetSkeleton = LoadObject<USkeleton>(nullptr, *TargetSkeletonPath);
    if (!Source || !TargetSkeleton || !FPackageName::IsValidLongPackageName(DestinationPath))
    {
        UE_LOG(LogTemp, Error, TEXT("PROC_RETARGET invalid Source=%s TargetSkeleton=%s Destination=%s"),
            *SourcePath, *TargetSkeletonPath, *DestinationPath);
        return 1;
    }

    const FString AssetName = FPackageName::GetLongPackageAssetName(DestinationPath);
    UPackage* Package = CreatePackage(*DestinationPath);
    if (!Package)
    {
        UE_LOG(LogTemp, Error, TEXT("PROC_RETARGET could not create package %s"), *DestinationPath);
        return 1;
    }

    UAnimSequence* Retargeted = DuplicateObject<UAnimSequence>(Source, Package, *AssetName);
    if (!Retargeted)
    {
        UE_LOG(LogTemp, Error, TEXT("PROC_RETARGET could not duplicate %s"), *SourcePath);
        return 1;
    }

    Retargeted->SetSkeleton(TargetSkeleton);
    Retargeted->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Retargeted);

    FString Filename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(
            DestinationPath, Filename, FPackageName::GetAssetPackageExtension()))
    {
        UE_LOG(LogTemp, Error, TEXT("PROC_RETARGET could not resolve filename for %s"), *DestinationPath);
        return 1;
    }

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;
    if (!UPackage::SavePackage(Package, Retargeted, *Filename, SaveArgs))
    {
        UE_LOG(LogTemp, Error, TEXT("PROC_RETARGET failed saving %s"), *Filename);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("PROC_RETARGET_OK Source=%s Target=%s Destination=%s"),
        *SourcePath, *TargetSkeletonPath, *DestinationPath);
    return 0;
}
