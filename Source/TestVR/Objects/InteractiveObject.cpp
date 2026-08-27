// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/InteractiveObject.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Objects/InteractiveObjectSubsystem.h"
#include "Objects/SnapZone.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "InteractiveObject"

AInteractiveObject::AInteractiveObject()
{
	PrimaryActorTick.bCanEverTick = false;

	ObjectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ObjectMesh"));
	SetRootComponent(ObjectMesh);
	ObjectMesh->SetMobility(EComponentMobility::Movable);
	ObjectMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	ObjectMesh->SetGenerateOverlapEvents(true);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded())
	{
		ObjectMesh->SetStaticMesh(DefaultMesh.Object);
	}

	Data.Name = LOCTEXT("DefaultInteractionName", "Interactive Object");
}

void AInteractiveObject::BeginPlay()
{
	Super::BeginPlay();

	if (IsValid(ObjectMesh) && IsValid(HighlightGlowMaterial))
	{
		HighlightMID = UMaterialInstanceDynamic::Create(HighlightGlowMaterial, this);
		if (HighlightMID)
		{
			HighlightMID->SetScalarParameterValue(HighlightEnableParam, 0.0f);
			ObjectMesh->SetOverlayMaterial(HighlightMID);
		}
	}

	bPhysicsEnabled = IsValid(ObjectMesh) && ObjectMesh->IsSimulatingPhysics();

	if (UInteractiveObjectSubsystem* Registry = UInteractiveObjectSubsystem::Get(this))
	{
		Registry->RegisterObject(this);
	}
}

void AInteractiveObject::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (UInteractiveObjectSubsystem* Registry = UInteractiveObjectSubsystem::Get(this))
	{
		Registry->UnregisterObject(this);
	}

	Super::EndPlay(EndPlayReason);
}

FInteractiveObjectData AInteractiveObject::GetInteractionData() const
{
	return Data;
}

void AInteractiveObject::BeginHover()
{
	SetHighlighted(true);
}

void AInteractiveObject::EndHover()
{
	SetHighlighted(false);
}

bool AInteractiveObject::CanGrab() const
{
	return bCanBeGrabbed && !bIsGrabbed && !IsActorBeingDestroyed();
}

bool AInteractiveObject::BeginGrab(USceneComponent* AttachTo)
{
	if (!CanGrab() || !IsValid(AttachTo))
	{
		return false;
	}

	bIsGrabbed = true;
	PrepareForGrab();

	if (!AttachToHand(AttachTo))
	{
		bIsGrabbed = false;
		RefreshPhysicsSimulation();
		return false;
	}
	return true;
}

void AInteractiveObject::BeginSecondHandGrab()
{
	if (!bIsGrabbed || bIsSecondHandGrabbed)
	{
		return;
	}

	// Detach: UVRInteractionComponent now drives the transform from both controllers.
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bIsSecondHandGrabbed = true;
}

void AInteractiveObject::EndSecondHandGrab(USceneComponent* ReattachTo)
{
	if (!bIsSecondHandGrabbed)
	{
		return;
	}

	bIsSecondHandGrabbed = false;
	if (IsValid(ReattachTo))
	{
		// Keep the transform the gesture produced; do not re-snap into the hand.
		AttachToComponent(ReattachTo, GetGrabAttachmentRules());
	}
}

void AInteractiveObject::EndGrab()
{
	if (!bIsGrabbed)
	{
		return;
	}

	bIsSecondHandGrabbed = false;
	bIsGrabbed = false;

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	ASnapZone::TrySnapReleasedObject(this);
	RefreshPhysicsSimulation();
}

void AInteractiveObject::PrepareForGrab()
{
	ASnapZone::ReleaseObjectFromAnyZone(this);

	// Zero velocities and drop to kinematic before attaching, so a body that was simulating
	// does not drift for a frame while Chaos processes the state change.
	if (IsValid(ObjectMesh) && ObjectMesh->IsSimulatingPhysics())
	{
		bPhysicsEnabled = true;
		ObjectMesh->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		ObjectMesh->SetAllPhysicsAngularVelocityInRadians(FVector::ZeroVector);
		ObjectMesh->SetSimulatePhysics(false);
	}
}

bool AInteractiveObject::AttachToHand(USceneComponent* Hand)
{
	if (!AttachToComponent(Hand, GetGrabAttachmentRules()))
	{
		return false;
	}

	if (bSnapToHandOnGrab)
	{
		SetActorLocation(
			Hand->GetComponentLocation() + Hand->GetForwardVector() * GrabForwardOffset,
			false, nullptr, ETeleportType::TeleportPhysics);
	}
	return true;
}

bool AInteractiveObject::CanDelete() const
{
	return bCanBeDeleted && !IsActorBeingDestroyed();
}

bool AInteractiveObject::RequestDestroy()
{
	return CanDelete() && Destroy();
}

void AInteractiveObject::SetPhysicsEnabled(bool bEnabled)
{
	bPhysicsEnabled = bEnabled;
	RefreshPhysicsSimulation();
}

void AInteractiveObject::SetSnappedState(bool bInSnapped)
{
	if (bIsSnapped == bInSnapped)
	{
		return;
	}

	bIsSnapped = bInSnapped;
	RefreshPhysicsSimulation();
}

FAttachmentTransformRules AInteractiveObject::GetGrabAttachmentRules()
{
	// bWeldSimulatedBodies must be true: without it a mesh that was simulating physics keeps its
	// own body authority and does not follow the hand after being attached.
	return FAttachmentTransformRules(
		EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld,
		/*bWeldSimulatedBodies=*/true);
}

void AInteractiveObject::RefreshPhysicsSimulation()
{
	if (!IsValid(ObjectMesh))
	{
		return;
	}

	const bool bShouldSimulate = bPhysicsEnabled && !bIsGrabbed && !bIsSnapped;
	if (ObjectMesh->IsSimulatingPhysics() != bShouldSimulate)
	{
		ObjectMesh->SetSimulatePhysics(bShouldSimulate);
	}
}

void AInteractiveObject::SetHighlighted_Implementation(bool bHighlighted)
{
	if (HighlightMID)
	{
		HighlightMID->SetScalarParameterValue(HighlightEnableParam, bHighlighted ? 1.0f : 0.0f);
	}
}

#undef LOCTEXT_NAMESPACE
