// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

#include "VRInteractionComponent.generated.h"

class IInteractable;
class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;
class UWidgetInteractionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInteractionTargetChanged, AActor*, CurrentTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnPointerUpdated, bool, bLeftHand, bool, bVisible, FVector, Start, FVector, End);

UENUM()
enum class EVRHandRole : uint8
{
	Idle,      // not pointing at an interactable
	Hovering,  // pointing at an interactable, not holding
	Holding    // holding an object (one hand, or one hand of a two-hand grab)
};

/** All interaction state for one controller. */
USTRUCT()
struct FVRHandInteraction
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> Source;   // aim component on the pawn

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> Target;            // hovered or held actor

	UPROPERTY(Transient)
	EVRHandRole Role = EVRHandRole::Idle;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Pointer = nullptr;

	double LastHitTime = 0.0;                 // last time the ray hit Target (hover hysteresis)
	bool bLeftHand = false;

	bool IsHolding() const { return Role == EVRHandRole::Holding; }
};

/**
 * Holds the VR interaction state (hover / hold / two-hand) for a Pawn and drives it each tick.
 * Talks to world objects only through IInteractable.
 */
UCLASS(ClassGroup = (VR), meta = (BlueprintSpawnableComponent))
class TESTVR_API UVRInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVRInteractionComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "VR Interaction|Setup")
	void SetRightHandSource(USceneComponent* InSource);

	UFUNCTION(BlueprintCallable, Category = "VR Interaction|Setup")
	void SetLeftHandSource(USceneComponent* InSource);

	/** Right-hand widget interaction component, so world grab yields to UI clicks. */
	UFUNCTION(BlueprintCallable, Category = "VR Interaction|Setup")
	void SetUIWidgetInteraction(UWidgetInteractionComponent* InWidgetInteraction);

	/** Call when the left-hand VR menu opens/closes so left-hand interaction is suppressed. */
	UFUNCTION(BlueprintCallable, Category = "VR Interaction|Setup")
	void SetMenuOpen(bool bInMenuOpen);

	//~ Input: call from the Pawn's grab Enhanced Input events (see README for the binding).
	UFUNCTION(BlueprintCallable, Category = "VR Interaction|Input")
	void RightGrabPressed();

	UFUNCTION(BlueprintCallable, Category = "VR Interaction|Input")
	void RightGrabReleased();

	UFUNCTION(BlueprintCallable, Category = "VR Interaction|Input")
	void LeftGrabPressed();

	UFUNCTION(BlueprintCallable, Category = "VR Interaction|Input")
	void LeftGrabReleased();

	/** Held object (either hand) if any, otherwise the hovered object (either hand). */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VR Interaction")
	AActor* GetCurrentInteractionTarget() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VR Interaction")
	bool IsTwoHandActive() const { return bTwoHandActive; }

	/** Broadcast whenever the current interaction target changes (drives the VR panel refresh). */
	UPROPERTY(BlueprintAssignable, Category = "VR Interaction|Events")
	FOnInteractionTargetChanged OnInteractionTargetChanged;

	/** Per-hand ray start/end and visibility, broadcast each tick for an optional custom pointer. */
	UPROPERTY(BlueprintAssignable, Category = "VR Interaction|Events")
	FOnPointerUpdated OnPointerUpdated;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	static IInteractable* AsInteractable(AActor* Actor);

	FVRHandInteraction& OtherHand(const FVRHandInteraction& Hand);

	void UpdateHand(FVRHandInteraction& Hand, bool bSuspended);
	AActor* TraceInteractable(const USceneComponent& Source, FVector& OutStart, FVector& OutEnd, FVector& OutHit, bool& bOutHit) const;
	void ResolveHover(FVRHandInteraction& Hand, AActor* Candidate, bool bSuspended);
	void ValidateHandState(FVRHandInteraction& Hand);
	void UpdatePointer(FVRHandInteraction& Hand, bool bVisible, const FVector& Start, const FVector& End);

	/** The single place a hand's role changes: fires hover begin/end, tracking and the delegate. */
	void SetHandRole(FVRHandInteraction& Hand, EVRHandRole NewRole, AActor* NewTarget);
	bool TryGrab(FVRHandInteraction& Hand);
	void ReleaseHand(FVRHandInteraction& Hand);

	/** The second hand may only join a two-hand grab when on / close to the held object. */
	bool CanLeftHandJoinTwoHand(const AActor* HeldActor) const;
	void BeginTwoHand();
	void EndTwoHand();
	void UpdateTwoHandTransform();

	void TrackActor(AActor* Actor);
	void StopTrackingIfUnused(AActor* Actor, const FVRHandInteraction* ExcludeHand);
	bool IsActorEngagedElsewhere(const AActor* Actor, const FVRHandInteraction* ExcludeHand) const;

	UFUNCTION()
	void HandleTrackedActorDestroyed(AActor* DestroyedActor);

	bool IsPointerOverUI() const;
	void CreatePointerVisuals();

	UPROPERTY(EditAnywhere, Category = "VR Interaction|Trace", meta = (ClampMin = "1.0", Units = "cm", AllowPrivateAccess = "true"))
	float TraceDistance = 200.0f;

	UPROPERTY(EditAnywhere, Category = "VR Interaction|Trace", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** When true the left controller can also hover and grab world objects on its own. */
	UPROPERTY(EditAnywhere, Category = "VR Interaction|Trace", meta = (AllowPrivateAccess = "true"))
	bool bAllowLeftHandGrab = true;

	/** Seconds a hover survives the ray missing before it drops (kills mesh-edge flicker). */
	UPROPERTY(EditAnywhere, Category = "VR Interaction|Trace", meta = (ClampMin = "0.0", Units = "s", AllowPrivateAccess = "true"))
	float HoverReleaseGrace = 0.12f;

	/** Max distance from the held object at which the second hand may start a two-hand grab. */
	UPROPERTY(EditAnywhere, Category = "VR Interaction|Two-Hand", meta = (ClampMin = "0.0", Units = "cm", AllowPrivateAccess = "true"))
	float TwoHandActivationDistance = 25.0f;

	UPROPERTY(EditAnywhere, Category = "VR Interaction|Two-Hand", meta = (ClampMin = "0.01", AllowPrivateAccess = "true"))
	float MinTwoHandScaleFactor = 0.25f;

	UPROPERTY(EditAnywhere, Category = "VR Interaction|Two-Hand", meta = (ClampMin = "0.01", AllowPrivateAccess = "true"))
	float MaxTwoHandScaleFactor = 3.0f;

	UPROPERTY(EditAnywhere, Category = "VR Interaction|Pointer", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraSystem> PointerNiagaraSystem;

	UPROPERTY(EditAnywhere, Category = "VR Interaction|Pointer", meta = (AllowPrivateAccess = "true"))
	FName PointArrayParamName = TEXT("User.PointArray");

	TWeakObjectPtr<UWidgetInteractionComponent> WidgetInteraction;

	bool bMenuOpen = false;
	bool bTwoHandActive = false;

	UPROPERTY(Transient)
	FVRHandInteraction RightHand;

	UPROPERTY(Transient)
	FVRHandInteraction LeftHand;

	double InitialControllerDistance = 0.0;
	FVector InitialObjectScale = FVector::OneVector;
	FVector InitialHandsVector = FVector::ForwardVector;
	FQuat InitialObjectRotation = FQuat::Identity;
	FVector ObjectOffsetFromMidpoint = FVector::ZeroVector;
};
