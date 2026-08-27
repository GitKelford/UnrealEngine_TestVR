// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "InteractiveObjectTypes.generated.h"

UENUM(BlueprintType)
enum class EInteractiveObjectType : uint8
{
	Generic,
	Tool,
	Container,
	Device
};

USTRUCT(BlueprintType)
struct TESTVR_API FInteractiveObjectData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	EInteractiveObjectType Type = EInteractiveObjectType::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float Value = 0.0f;
};
