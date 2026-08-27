// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"

#include "Interaction/InteractiveObjectTypes.h"

#include "InteractiveSceneSaveGame.generated.h"

class AInteractiveObject;

USTRUCT()
struct FSavedInteractiveObjectRecord
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftClassPtr<AInteractiveObject> ObjectClass;

	UPROPERTY()
	FTransform Transform = FTransform::Identity;

	UPROPERTY()
	FInteractiveObjectData Data;

	UPROPERTY()
	FName SnapZoneId = NAME_None;
};

UCLASS()
class TESTVR_API UInteractiveSceneSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString& GetSlotName();
	static int32 GetUserIndex() { return 0; }

	UPROPERTY()
	TArray<FSavedInteractiveObjectRecord> SavedObjects;
};
