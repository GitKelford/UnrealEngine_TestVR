// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "Interaction/Interactable.h"

#include "InteractiveObject.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class TESTVR_API AInteractiveObject : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AInteractiveObject();

	//~ IInteractable
	virtual FInteractiveObjectData GetInteractionData() const override;
	virtual void BeginHover() override;
	virtual void EndHover() override;
	virtual bool CanGrab() const override;
	virtual bool BeginGrab(USceneComponent* AttachTo) override;
	virtual void BeginSecondHandGrab() override;
	virtual void EndSecondHandGrab(USceneComponent* ReattachTo) override;
	virtual void EndGrab() override;
	virtual bool CanDelete() const override;
	virtual bool RequestDestroy() override;
	//~ End IInteractable

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
	UStaticMeshComponent* GetObjectMesh() const { return ObjectMesh; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
	bool IsGrabbed() const { return bIsGrabbed; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction")
	FInteractiveObjectData GetData() const { return Data; }

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void SetData(const FInteractiveObjectData& NewData) { Data = NewData; }

	UFUNCTION(BlueprintCallable, Category = "Interaction|Physics")
	void SetPhysicsEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Interaction|Physics")
	bool IsPhysicsEnabled() const { return bPhysicsEnabled; }

	void SetSnappedState(bool bInSnapped);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintNativeEvent, Category = "Interaction|Highlight")
	void SetHighlighted(bool bHighlighted);
	virtual void SetHighlighted_Implementation(bool bHighlighted);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> ObjectMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	FInteractiveObjectData Data;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Capabilities")
	bool bCanBeGrabbed = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Capabilities")
	bool bCanBeDeleted = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Grab")
	bool bSnapToHandOnGrab = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Grab", meta = (Units = "cm"))
	float GrabForwardOffset = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Highlight")
	TObjectPtr<UMaterialInterface> HighlightGlowMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Highlight")
	FName HighlightEnableParam = TEXT("Enabled");

private:
	static FAttachmentTransformRules GetGrabAttachmentRules();

	/** Unsnap and make the mesh kinematic so it can be carried. */
	void PrepareForGrab();
	/** Attach to a hand component and (optionally) snap into it. */
	bool AttachToHand(USceneComponent* Hand);
	/** Simulate physics only while the object is free (not held, not snapped). */
	void RefreshPhysicsSimulation();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HighlightMID;

	// This object's own physical status (not interaction state, which lives in UVRInteractionComponent).
	bool bIsGrabbed = false;           // attached to a hand
	bool bIsSecondHandGrabbed = false; // detached, driven by the two-hand gesture
	bool bIsSnapped = false;           // parked in a Snap Zone (set by ASnapZone)
	bool bPhysicsEnabled = false;      // simulate physics while free
};
