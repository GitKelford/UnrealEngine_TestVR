// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "InteractiveObjectSubsystem.generated.h"

class AInteractiveObject;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractiveRegistryChanged);

/**
 * Per-world registry, runtime factory and Save/Load authority for AInteractiveObject.
 * Objects self-register in BeginPlay and unregister in EndPlay; nothing polls the world.
 */
UCLASS()
class TESTVR_API UInteractiveObjectSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UInteractiveObjectSubsystem* Get(const UObject* WorldContext);

	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

	void RegisterObject(AInteractiveObject* Object);
	void UnregisterObject(AInteractiveObject* Object);

	UFUNCTION(BlueprintCallable, Category = "Interactive Objects")
	AInteractiveObject* SpawnObject(TSubclassOf<AInteractiveObject> ObjectClass, APlayerController* ViewerController = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Interactive Objects")
	bool RequestDeleteObject(AInteractiveObject* Object);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interactive Objects")
	int32 GetObjectCount() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interactive Objects")
	TArray<AInteractiveObject*> GetObjects() const;

	const TArray<TSubclassOf<AInteractiveObject>>& GetSpawnableClasses() const;

	UPROPERTY(BlueprintAssignable, Category = "Interactive Objects")
	FOnInteractiveRegistryChanged OnRegistryChanged;

	UFUNCTION(BlueprintCallable, Category = "Interactive Objects|Save")
	bool SaveScene() const;

	UFUNCTION(BlueprintCallable, Category = "Interactive Objects|Save")
	bool LoadScene();

private:
	UFUNCTION()
	void HandleObjectDestroyed(AActor* DestroyedActor);

	void ClearAllObjects();
	void DiscoverSpawnableClasses() const;
	void BroadcastRegistryChanged() const;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AInteractiveObject>> Objects;

	mutable TArray<TSubclassOf<AInteractiveObject>> CachedSpawnableClasses;
	mutable bool bSpawnableClassesCached = false;
};
