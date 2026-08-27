// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interaction/VRInteractionComponent.h"

#include "Components/SceneComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

UVRInteractionComponent::UVRInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> MenuLaser(TEXT("/Game/XRFramework/VFX/NS_MenuLaser.NS_MenuLaser"));
	if (MenuLaser.Succeeded())
	{
		PointerNiagaraSystem = MenuLaser.Object;
	}

	RightHand.bLeftHand = false;
	LeftHand.bLeftHand = true;
}

void UVRInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	CreatePointerVisuals();
}

void UVRInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bTwoHandActive)
	{
		EndTwoHand();
	}
	ReleaseHand(RightHand);
	ReleaseHand(LeftHand);

	for (FVRHandInteraction* Hand : { &RightHand, &LeftHand })
	{
		if (IsValid(Hand->Pointer))
		{
			Hand->Pointer->DestroyComponent();
		}
		Hand->Pointer = nullptr;
		Hand->Source.Reset();
	}

	Super::EndPlay(EndPlayReason);
}

void UVRInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ValidateHandState(RightHand);
	ValidateHandState(LeftHand);

	if (bTwoHandActive)
	{
		UpdateTwoHandTransform();
	}

	// A hand does not hover (and shows no pointer) while it is holding, aiming at the UI, or
	// while the left-hand menu is open.
	const bool bRightSuspended = RightHand.IsHolding() || IsPointerOverUI();
	const bool bLeftSuspended = !bAllowLeftHandGrab || bMenuOpen || bTwoHandActive || LeftHand.IsHolding();

	UpdateHand(RightHand, bRightSuspended);
	UpdateHand(LeftHand, bLeftSuspended);
}

void UVRInteractionComponent::SetRightHandSource(USceneComponent* InSource)
{
	RightHand.Source = InSource;
}

void UVRInteractionComponent::SetLeftHandSource(USceneComponent* InSource)
{
	LeftHand.Source = InSource;
}

void UVRInteractionComponent::SetUIWidgetInteraction(UWidgetInteractionComponent* InWidgetInteraction)
{
	WidgetInteraction = InWidgetInteraction;
}

void UVRInteractionComponent::SetMenuOpen(bool bInMenuOpen)
{
	bMenuOpen = bInMenuOpen;
}

AActor* UVRInteractionComponent::GetCurrentInteractionTarget() const
{
	if (RightHand.IsHolding())
	{
		return RightHand.Target.Get();
	}
	if (LeftHand.IsHolding())
	{
		return LeftHand.Target.Get();
	}
	if (AActor* RightHovered = RightHand.Target.Get())
	{
		return RightHovered;
	}
	return LeftHand.Target.Get();
}

bool UVRInteractionComponent::IsPointerOverUI() const
{
	return WidgetInteraction.IsValid() && WidgetInteraction->GetHoveredWidgetComponent() != nullptr;
}

IInteractable* UVRInteractionComponent::AsInteractable(AActor* Actor)
{
	return IsValid(Actor) ? Cast<IInteractable>(Actor) : nullptr;
}

FVRHandInteraction& UVRInteractionComponent::OtherHand(const FVRHandInteraction& Hand)
{
	return &Hand == &RightHand ? LeftHand : RightHand;
}

void UVRInteractionComponent::RightGrabPressed()
{
	if (IsPointerOverUI() || RightHand.IsHolding())
	{
		return;
	}
	TryGrab(RightHand);
}

void UVRInteractionComponent::RightGrabReleased()
{
	if (!RightHand.IsHolding())
	{
		return;
	}

	if (bTwoHandActive)
	{
		bTwoHandActive = false;
		SetHandRole(LeftHand, EVRHandRole::Idle, nullptr);
	}
	ReleaseHand(RightHand);
}

void UVRInteractionComponent::LeftGrabPressed()
{
	if (bTwoHandActive || bMenuOpen || LeftHand.IsHolding())
	{
		return;
	}

	if (AActor* RightHeld = RightHand.IsHolding() ? RightHand.Target.Get() : nullptr)
	{
		if (CanLeftHandJoinTwoHand(RightHeld))
		{
			BeginTwoHand();
			return;
		}
	}

	if (bAllowLeftHandGrab)
	{
		TryGrab(LeftHand);
	}
}

void UVRInteractionComponent::LeftGrabReleased()
{
	if (bTwoHandActive)
	{
		EndTwoHand();
	}
	else if (LeftHand.IsHolding())
	{
		ReleaseHand(LeftHand);
	}
}

bool UVRInteractionComponent::TryGrab(FVRHandInteraction& Hand)
{
	AActor* Target = Hand.Target.Get();
	IInteractable* Interactable = AsInteractable(Target);
	USceneComponent* Source = Hand.Source.Get();
	if (!Interactable || !Source || Hand.Role != EVRHandRole::Hovering)
	{
		return false;
	}

	if (OtherHand(Hand).IsHolding() && OtherHand(Hand).Target.Get() == Target)
	{
		BeginTwoHand();
		return true;
	}

	if (!Interactable->CanGrab() || !Interactable->BeginGrab(Source) || !IsValid(Target))
	{
		return false;
	}

	SetHandRole(Hand, EVRHandRole::Holding, Target);
	return true;
}

void UVRInteractionComponent::ReleaseHand(FVRHandInteraction& Hand)
{
	if (!Hand.IsHolding())
	{
		return;
	}

	if (IInteractable* Interactable = AsInteractable(Hand.Target.Get()))
	{
		Interactable->EndGrab();
	}
	SetHandRole(Hand, EVRHandRole::Idle, nullptr);
}

bool UVRInteractionComponent::CanLeftHandJoinTwoHand(const AActor* HeldActor) const
{
	if (!HeldActor)
	{
		return false;
	}
	if (LeftHand.Target.Get() == HeldActor)
	{
		return true;
	}
	const USceneComponent* LeftSource = LeftHand.Source.Get();
	return LeftSource
		&& FVector::DistSquared(LeftSource->GetComponentLocation(), HeldActor->GetActorLocation())
		   <= FMath::Square(TwoHandActivationDistance);
}

void UVRInteractionComponent::BeginTwoHand()
{
	if (bTwoHandActive)
	{
		return;
	}

	AActor* Target = RightHand.IsHolding() ? RightHand.Target.Get()
		: (LeftHand.IsHolding() ? LeftHand.Target.Get() : nullptr);
	IInteractable* Interactable = AsInteractable(Target);
	USceneComponent* Right = RightHand.Source.Get();
	USceneComponent* Left = LeftHand.Source.Get();
	if (!Interactable || !Right || !Left)
	{
		return;
	}

	Interactable->BeginSecondHandGrab();

	// Reference frame captured once: the object then follows the midpoint of both controllers,
	// rotates with the line between them and scales with their distance.
	const FVector RightLoc = Right->GetComponentLocation();
	const FVector LeftLoc = Left->GetComponentLocation();
	InitialControllerDistance = FVector::Dist(RightLoc, LeftLoc);
	InitialObjectScale = Target->GetActorScale3D();
	InitialHandsVector = LeftLoc - RightLoc;
	InitialObjectRotation = Target->GetActorQuat();
	ObjectOffsetFromMidpoint = Target->GetActorLocation() - (RightLoc + LeftLoc) * 0.5;

	SetHandRole(RightHand, EVRHandRole::Holding, Target);
	SetHandRole(LeftHand, EVRHandRole::Holding, Target);
	bTwoHandActive = true;
}

void UVRInteractionComponent::EndTwoHand()
{
	bTwoHandActive = false;
	SetHandRole(LeftHand, EVRHandRole::Idle, nullptr);

	IInteractable* Interactable = AsInteractable(RightHand.Target.Get());
	if (!Interactable)
	{
		return;
	}

	if (USceneComponent* Right = RightHand.Source.Get())
	{
		Interactable->EndSecondHandGrab(Right);   // right stays the holder
	}
	else
	{
		// No hand to hand back to: release fully so the object is never left frozen.
		Interactable->EndGrab();
		SetHandRole(RightHand, EVRHandRole::Idle, nullptr);
	}
}

void UVRInteractionComponent::UpdateTwoHandTransform()
{
	AActor* Target = RightHand.Target.Get();
	USceneComponent* Right = RightHand.Source.Get();
	USceneComponent* Left = LeftHand.Source.Get();
	if (!Target || !Right || !Left)
	{
		EndTwoHand();
		return;
	}

	const FVector RightLoc = Right->GetComponentLocation();
	const FVector LeftLoc = Left->GetComponentLocation();

	const double CurrentDistance = FVector::Dist(RightLoc, LeftLoc);
	const double RawScale = InitialControllerDistance > UE_KINDA_SMALL_NUMBER
		? CurrentDistance / InitialControllerDistance
		: 1.0;
	const FVector NewScale = InitialObjectScale * FMath::Clamp(RawScale, (double)MinTwoHandScaleFactor, (double)MaxTwoHandScaleFactor);

	const FQuat RotationDelta = FQuat::FindBetweenVectors(InitialHandsVector.GetSafeNormal(), (LeftLoc - RightLoc).GetSafeNormal());
	const FQuat NewRotation = RotationDelta * InitialObjectRotation;

	const FVector NewLocation = (RightLoc + LeftLoc) * 0.5 + RotationDelta.RotateVector(ObjectOffsetFromMidpoint);

	Target->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
	Target->SetActorScale3D(NewScale);
}

void UVRInteractionComponent::UpdateHand(FVRHandInteraction& Hand, bool bSuspended)
{
	const USceneComponent* Source = Hand.Source.Get();
	if (!Source)
	{
		ResolveHover(Hand, nullptr, true);
		UpdatePointer(Hand, false, FVector::ZeroVector, FVector::ZeroVector);
		return;
	}

	FVector Start, End, HitLoc;
	bool bHit = false;
	AActor* Candidate = TraceInteractable(*Source, Start, End, HitLoc, bHit);

	ResolveHover(Hand, Candidate, bSuspended);

	const bool bShowPointer = !bSuspended && Hand.Role == EVRHandRole::Hovering;
	UpdatePointer(Hand, bShowPointer, Start, bHit ? HitLoc : End);
}

AActor* UVRInteractionComponent::TraceInteractable(
	const USceneComponent& Source, FVector& OutStart, FVector& OutEnd, FVector& OutHit, bool& bOutHit) const
{
	bOutHit = false;
	OutStart = Source.GetComponentLocation();
	OutEnd = OutStart + Source.GetForwardVector() * TraceDistance;
	OutHit = OutEnd;

	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VRInteractionTrace), false, GetOwner());
	if (!World->LineTraceSingleByChannel(Hit, OutStart, OutEnd, TraceChannel, QueryParams))
	{
		return nullptr;
	}

	bOutHit = true;
	OutHit = Hit.ImpactPoint;
	AActor* HitActor = Hit.GetActor();
	return AsInteractable(HitActor) ? HitActor : nullptr;
}

void UVRInteractionComponent::ResolveHover(FVRHandInteraction& Hand, AActor* Candidate, bool bSuspended)
{
	if (Hand.IsHolding())
	{
		return;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	if (Candidate && !bSuspended)
	{
		if (Hand.Target.Get() != Candidate)
		{
			SetHandRole(Hand, EVRHandRole::Hovering, Candidate);
		}
		Hand.LastHitTime = Now;
		return;
	}

	// Delay hover loss briefly to avoid flicker at mesh edges.
	if (Hand.Role == EVRHandRole::Hovering && (Now - Hand.LastHitTime) < HoverReleaseGrace)
	{
		return;
	}
	if (Hand.Role == EVRHandRole::Hovering)
	{
		SetHandRole(Hand, EVRHandRole::Idle, nullptr);
	}
}

void UVRInteractionComponent::SetHandRole(FVRHandInteraction& Hand, EVRHandRole NewRole, AActor* NewTarget)
{
	AActor* OldTarget = Hand.Target.Get();
	const EVRHandRole OldRole = Hand.Role;
	if (OldTarget == NewTarget && OldRole == NewRole)
	{
		return;
	}

	// Only clear the highlight if the other hand is not still hovering / holding the same object.
	if (OldRole == EVRHandRole::Hovering && OldTarget && !IsActorEngagedElsewhere(OldTarget, &Hand))
	{
		if (IInteractable* Interactable = AsInteractable(OldTarget))
		{
			Interactable->EndHover();
		}
	}

	Hand.Role = NewRole;
	Hand.Target = NewTarget;

	if (OldTarget && OldTarget != NewTarget)
	{
		StopTrackingIfUnused(OldTarget, &Hand);
	}

	if (NewTarget && NewRole != EVRHandRole::Idle)
	{
		TrackActor(NewTarget);
		if (NewRole == EVRHandRole::Hovering)
		{
			if (IInteractable* Interactable = AsInteractable(NewTarget))
			{
				Interactable->BeginHover();
			}
		}
	}

	OnInteractionTargetChanged.Broadcast(GetCurrentInteractionTarget());
}

void UVRInteractionComponent::ValidateHandState(FVRHandInteraction& Hand)
{
	if (Hand.Role != EVRHandRole::Idle && Hand.Target.IsStale())
	{
		const bool bWasHolding = Hand.IsHolding();
		Hand.Role = EVRHandRole::Idle;
		Hand.Target.Reset();
		if (bWasHolding)
		{
			bTwoHandActive = false;
		}
		OnInteractionTargetChanged.Broadcast(GetCurrentInteractionTarget());
	}
}

void UVRInteractionComponent::TrackActor(AActor* Actor)
{
	if (IsValid(Actor))
	{
		Actor->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleTrackedActorDestroyed);
	}
}

void UVRInteractionComponent::StopTrackingIfUnused(AActor* Actor, const FVRHandInteraction* ExcludeHand)
{
	if (IsValid(Actor) && !IsActorEngagedElsewhere(Actor, ExcludeHand))
	{
		Actor->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleTrackedActorDestroyed);
	}
}

bool UVRInteractionComponent::IsActorEngagedElsewhere(const AActor* Actor, const FVRHandInteraction* ExcludeHand) const
{
	if (!Actor)
	{
		return false;
	}
	auto Engaged = [Actor, ExcludeHand](const FVRHandInteraction& Hand)
	{
		return &Hand != ExcludeHand && Hand.Role != EVRHandRole::Idle && Hand.Target.Get() == Actor;
	};
	return Engaged(RightHand) || Engaged(LeftHand);
}

void UVRInteractionComponent::HandleTrackedActorDestroyed(AActor* DestroyedActor)
{
	bool bChanged = false;
	for (FVRHandInteraction* Hand : { &RightHand, &LeftHand })
	{
		if (Hand->Target.Get() == DestroyedActor)
		{
			if (Hand->IsHolding())
			{
				bTwoHandActive = false;
			}
			Hand->Role = EVRHandRole::Idle;
			Hand->Target.Reset();
			bChanged = true;
		}
	}

	if (bChanged)
	{
		OnInteractionTargetChanged.Broadcast(GetCurrentInteractionTarget());
	}
}

void UVRInteractionComponent::CreatePointerVisuals()
{
	USceneComponent* Root = IsValid(GetOwner()) ? GetOwner()->GetRootComponent() : nullptr;
	if (!IsValid(PointerNiagaraSystem) || !Root)
	{
		return;
	}

	for (FVRHandInteraction* Hand : { &RightHand, &LeftHand })
	{
		Hand->Pointer = UNiagaraFunctionLibrary::SpawnSystemAttached(
			PointerNiagaraSystem, Root, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
			false, false);
	}
}

void UVRInteractionComponent::UpdatePointer(FVRHandInteraction& Hand, bool bVisible, const FVector& Start, const FVector& End)
{
	OnPointerUpdated.Broadcast(Hand.bLeftHand, bVisible, Start, End);

	UNiagaraComponent* Visual = Hand.Pointer;
	if (!IsValid(Visual))
	{
		return;
	}

	if (!bVisible)
	{
		if (Visual->IsActive())
		{
			Visual->Deactivate();
		}
		return;
	}

	const TArray<FVector> BeamPoints = { Start, End };
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(Visual, PointArrayParamName, BeamPoints);
	if (!Visual->IsActive())
	{
		Visual->Activate(true);
	}
}
