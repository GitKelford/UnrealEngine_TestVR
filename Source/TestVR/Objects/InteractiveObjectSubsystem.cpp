// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/InteractiveObjectSubsystem.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/Interactable.h"
#include "Kismet/GameplayStatics.h"
#include "Modules/ModuleManager.h"
#include "Objects/InteractiveObject.h"
#include "Objects/SnapZone.h"
#include "SaveGame/InteractiveSceneSaveGame.h"
#include "TestVR.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectHash.h"

namespace
{
	constexpr float SpawnDistanceCm = 60.0f;
}

UInteractiveObjectSubsystem* UInteractiveObjectSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = IsValid(WorldContext) ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UInteractiveObjectSubsystem>() : nullptr;
}

bool UInteractiveObjectSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UInteractiveObjectSubsystem::Deinitialize()
{
	for (const TWeakObjectPtr<AInteractiveObject>& Object : Objects)
	{
		if (AInteractiveObject* Resolved = Object.Get())
		{
			Resolved->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleObjectDestroyed);
		}
	}
	Objects.Reset();

	Super::Deinitialize();
}

void UInteractiveObjectSubsystem::RegisterObject(AInteractiveObject* Object)
{
	if (!IsValid(Object) || Objects.Contains(Object))
	{
		return;
	}

	Objects.Add(Object);
	Object->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleObjectDestroyed);
	BroadcastRegistryChanged();
}

void UInteractiveObjectSubsystem::UnregisterObject(AInteractiveObject* Object)
{
	if (Objects.Remove(Object) > 0)
	{
		if (IsValid(Object))
		{
			Object->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleObjectDestroyed);
		}
		BroadcastRegistryChanged();
	}
}

void UInteractiveObjectSubsystem::BroadcastRegistryChanged() const
{
	const UWorld* World = GetWorld();
	if (World && !World->bIsTearingDown)
	{
		OnRegistryChanged.Broadcast();
	}
}

AInteractiveObject* UInteractiveObjectSubsystem::SpawnObject(TSubclassOf<AInteractiveObject> ObjectClass, APlayerController* ViewerController)
{
	UWorld* World = GetWorld();
	if (!ObjectClass || !IsValid(World))
	{
		return nullptr;
	}

	FVector SpawnLocation = FVector(0.0f, 0.0f, 100.0f);
	FRotator SpawnRotation = FRotator::ZeroRotator;

	APlayerController* PC = IsValid(ViewerController) ? ViewerController : UGameplayStatics::GetPlayerController(World, 0);
	if (APlayerCameraManager* CameraManager = IsValid(PC) ? PC->PlayerCameraManager : nullptr)
	{
		FVector ViewLocation;
		FRotator ViewRotation;
		CameraManager->GetCameraViewPoint(ViewLocation, ViewRotation);
		SpawnLocation = ViewLocation + ViewRotation.Vector() * SpawnDistanceCm;
		SpawnRotation = FRotator(0.0f, ViewRotation.Yaw, 0.0f);
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AInteractiveObject* NewObject = World->SpawnActor<AInteractiveObject>(ObjectClass, FTransform(SpawnRotation, SpawnLocation), Params);
	if (IsValid(NewObject))
	{
		RegisterObject(NewObject);
	}
	return NewObject;
}

bool UInteractiveObjectSubsystem::RequestDeleteObject(AInteractiveObject* Object)
{
	IInteractable* Interactable = Cast<IInteractable>(Object);
	return Interactable && Interactable->RequestDestroy();
}

int32 UInteractiveObjectSubsystem::GetObjectCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AInteractiveObject>& Object : Objects)
	{
		Count += Object.IsValid() ? 1 : 0;
	}
	return Count;
}

TArray<AInteractiveObject*> UInteractiveObjectSubsystem::GetObjects() const
{
	TArray<AInteractiveObject*> Result;
	Result.Reserve(Objects.Num());
	for (const TWeakObjectPtr<AInteractiveObject>& Object : Objects)
	{
		if (AInteractiveObject* Resolved = Object.Get())
		{
			Result.Add(Resolved);
		}
	}
	return Result;
}

const TArray<TSubclassOf<AInteractiveObject>>& UInteractiveObjectSubsystem::GetSpawnableClasses() const
{
	// Discovered once on first use (menu open), not per frame.
	if (!bSpawnableClassesCached)
	{
		DiscoverSpawnableClasses();
		bSpawnableClassesCached = true;
	}
	return CachedSpawnableClasses;
}

void UInteractiveObjectSubsystem::DiscoverSpawnableClasses() const
{
	CachedSpawnableClasses.Reset();
	UClass* const BaseClass = AInteractiveObject::StaticClass();

	auto Consider = [BaseClass, this](UClass* Candidate)
	{
		if (!Candidate
			|| Candidate == BaseClass
			|| !Candidate->IsChildOf(BaseClass)
			|| Candidate->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
		{
			return;
		}

		const FString Name = Candidate->GetName();
		if (Name.StartsWith(TEXT("SKEL_")) || Name.StartsWith(TEXT("REINST_")) || Name.Contains(TEXT("TRASHCLASS_")))
		{
			return;
		}
		CachedSpawnableClasses.AddUnique(Candidate);
	};

	// Loaded classes only. In a packaged build every C++/Blueprint subclass the level references is
	// already loaded by the time the menu opens, so no package has to be pulled in synchronously
	// here (a sync load on the game thread stalls hard on standalone Android).
	TArray<UClass*> LoadedDerived;
	GetDerivedClasses(BaseClass, LoadedDerived, /*bRecursive=*/true);
	for (UClass* Class : LoadedDerived)
	{
		Consider(Class);
	}

#if WITH_EDITOR
	// Editor only: also surface subclasses that exist as assets but are not loaded yet, so a newly
	// created Blueprint appears in the menu without having to be placed in the level first.
	if (const FAssetRegistryModule* Module = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
	{
		const IAssetRegistry& AssetRegistry = Module->Get();

		const FTopLevelAssetPath BasePath(BaseClass->GetOutermost()->GetFName(), BaseClass->GetFName());
		TArray<FTopLevelAssetPath> BaseNames = { BasePath };
		TSet<FTopLevelAssetPath> Excluded;
		TSet<FTopLevelAssetPath> DerivedNames;
		AssetRegistry.GetDerivedClassNames(BaseNames, Excluded, DerivedNames);

		for (const FTopLevelAssetPath& ClassPathName : DerivedNames)
		{
			if (ClassPathName == BasePath)
			{
				continue;
			}

			const FSoftObjectPath ClassRef(ClassPathName, FString());
			UClass* Class = Cast<UClass>(ClassRef.ResolveObject());
			if (!Class)
			{
				Class = Cast<UClass>(ClassRef.TryLoad());
			}
			Consider(Class);
		}
	}
#endif // WITH_EDITOR

	CachedSpawnableClasses.Sort([](const TSubclassOf<AInteractiveObject>& A, const TSubclassOf<AInteractiveObject>& B)
	{
		auto Label = [](const UClass* Class)
		{
			const AInteractiveObject* CDO = Class ? Class->GetDefaultObject<AInteractiveObject>() : nullptr;
			return CDO ? CDO->GetData().Name.ToString() : (Class ? Class->GetName() : FString());
		};
		return Label(*A) < Label(*B);
	});
}

bool UInteractiveObjectSubsystem::SaveScene() const
{
	const FString SlotName = UInteractiveSceneSaveGame::GetSlotName();
	const int32 UserIndex = UInteractiveSceneSaveGame::GetUserIndex();

	UInteractiveSceneSaveGame* SaveGameObject = Cast<UInteractiveSceneSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UInteractiveSceneSaveGame::StaticClass()));
	if (!IsValid(SaveGameObject))
	{
		return false;
	}

	SaveGameObject->SavedObjects.Reserve(Objects.Num());
	for (const TWeakObjectPtr<AInteractiveObject>& ObjectPtr : Objects)
	{
		const AInteractiveObject* Object = ObjectPtr.Get();
		if (!IsValid(Object))
		{
			continue;
		}

		FSavedInteractiveObjectRecord Record;
		Record.ObjectClass = TSoftClassPtr<AInteractiveObject>(Object->GetClass());
		Record.Transform = Object->GetActorTransform();
		Record.Data = Object->GetData();

		for (TActorIterator<ASnapZone> ZoneIt(GetWorld()); ZoneIt; ++ZoneIt)
		{
			if (ZoneIt->GetSnappedObject() == Object)
			{
				Record.SnapZoneId = ZoneIt->GetZoneId();
				break;
			}
		}

		SaveGameObject->SavedObjects.Add(Record);
	}

	return UGameplayStatics::SaveGameToSlot(SaveGameObject, SlotName, UserIndex);
}

bool UInteractiveObjectSubsystem::LoadScene()
{
	const FString SlotName = UInteractiveSceneSaveGame::GetSlotName();
	const int32 UserIndex = UInteractiveSceneSaveGame::GetUserIndex();

	if (!UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		return false;
	}

	UInteractiveSceneSaveGame* SaveGameObject = Cast<UInteractiveSceneSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	UWorld* World = GetWorld();
	if (!IsValid(SaveGameObject) || !IsValid(World))
	{
		return false;
	}

	ClearAllObjects();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Params.TransformScaleMethod = ESpawnActorScaleMethod::OverrideRootScale;
	for (const FSavedInteractiveObjectRecord& Record : SaveGameObject->SavedObjects)
	{
		TSubclassOf<AInteractiveObject> ObjectClass = Record.ObjectClass.LoadSynchronous();
		if (!ObjectClass)
		{
			UE_LOG(LogTestVRInteraction, Warning, TEXT("LoadScene: saved class '%s' could not be resolved, skipping."), *Record.ObjectClass.ToString());
			continue;
		}

		AInteractiveObject* NewObject = World->SpawnActor<AInteractiveObject>(ObjectClass, Record.Transform, Params);
		if (!IsValid(NewObject))
		{
			continue;
		}

		NewObject->SetData(Record.Data);
		RegisterObject(NewObject);

		if (Record.SnapZoneId != NAME_None)
		{
			if (ASnapZone* Zone = ASnapZone::FindZoneById(World, Record.SnapZoneId))
			{
				Zone->RestoreSnappedState(NewObject);
			}
		}
	}

	return true;
}

void UInteractiveObjectSubsystem::ClearAllObjects()
{
	// Destroy callbacks remove entries from Objects.
	TArray<TWeakObjectPtr<AInteractiveObject>> ObjectsCopy = Objects;
	for (const TWeakObjectPtr<AInteractiveObject>& ObjectPtr : ObjectsCopy)
	{
		RequestDeleteObject(ObjectPtr.Get());
	}
	Objects.Reset();
}

void UInteractiveObjectSubsystem::HandleObjectDestroyed(AActor* DestroyedActor)
{
	UnregisterObject(Cast<AInteractiveObject>(DestroyedActor));
}
