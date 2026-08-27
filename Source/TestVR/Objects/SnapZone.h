// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "SnapZone.generated.h"

class AInteractiveObject;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class TESTVR_API ASnapZone : public AActor
{
	GENERATED_BODY()

public:
	ASnapZone();

	bool TrySnapOnRelease(AInteractiveObject* Object);

	void ReleaseIfSnapped(AInteractiveObject* Object);

	void RestoreSnappedState(AInteractiveObject* Object);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Snap Zone")
	AInteractiveObject* GetSnappedObject() const { return SnappedObject; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Snap Zone")
	FName GetZoneId() const { return ZoneId; }

	static bool TrySnapReleasedObject(AInteractiveObject* Object);
	static void ReleaseObjectFromAnyZone(AInteractiveObject* Object);
	static ASnapZone* FindZoneById(const UWorld* World, FName InZoneId);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> SnapVolume;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SnapPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ZoneVisual;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Snap Zone")
	FName ZoneId = TEXT("SnapZone");

private:
	bool IsObjectInsideVolume(const AInteractiveObject* Object) const;

	UFUNCTION()
	void HandleSnappedObjectDestroyed(AActor* DestroyedActor);

	UPROPERTY(Transient)
	TObjectPtr<AInteractiveObject> SnappedObject;
};
