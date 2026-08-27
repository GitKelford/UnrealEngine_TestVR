// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/SnapZone.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Objects/InteractiveObject.h"
#include "UObject/ConstructorHelpers.h"

ASnapZone::ASnapZone()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SnapVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SnapVolume"));
	SnapVolume->SetupAttachment(SceneRoot);
	SnapVolume->SetBoxExtent(FVector(35.0f));
	SnapVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SnapVolume->SetCollisionResponseToAllChannels(ECR_Overlap);

	SnapPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SnapPoint"));
	SnapPoint->SetupAttachment(SceneRoot);

	ZoneVisual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZoneVisual"));
	ZoneVisual->SetupAttachment(SceneRoot);
	ZoneVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ZoneVisual->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.05f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		ZoneVisual->SetStaticMesh(CylinderMesh.Object);
	}
}

bool ASnapZone::IsObjectInsideVolume(const AInteractiveObject* Object) const
{
	if (!IsValid(Object) || !IsValid(SnapVolume))
	{
		return false;
	}

	const FBox ZoneBounds = SnapVolume->CalcBounds(SnapVolume->GetComponentTransform()).GetBox();
	return ZoneBounds.IsInsideOrOn(Object->GetActorLocation());
}

bool ASnapZone::TrySnapOnRelease(AInteractiveObject* Object)
{
	if (!IsValid(Object) || (SnappedObject.Get() && SnappedObject.Get() != Object) || !IsObjectInsideVolume(Object))
	{
		return false;
	}

	FVector SnapLocation = SnapPoint->GetComponentLocation();
	SnapLocation.Z = Object->GetActorLocation().Z;

	Object->SetActorLocationAndRotation(SnapLocation, SnapPoint->GetComponentRotation(), false, nullptr, ETeleportType::TeleportPhysics);

	SnappedObject = Object;
	Object->SetSnappedState(true);
	Object->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleSnappedObjectDestroyed);
	return true;
}

void ASnapZone::ReleaseIfSnapped(AInteractiveObject* Object)
{
	if (SnappedObject.Get() != Object)
	{
		return;
	}

	if (IsValid(Object))
	{
		Object->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleSnappedObjectDestroyed);
		Object->SetSnappedState(false);
	}
	SnappedObject = nullptr;
}

void ASnapZone::RestoreSnappedState(AInteractiveObject* Object)
{
	if (!IsValid(Object))
	{
		return;
	}

	SnappedObject = Object;
	Object->SetSnappedState(true);
	Object->OnDestroyed.AddUniqueDynamic(this, &ThisClass::HandleSnappedObjectDestroyed);
}

bool ASnapZone::TrySnapReleasedObject(AInteractiveObject* Object)
{
	if (!IsValid(Object) || !IsValid(Object->GetWorld()))
	{
		return false;
	}

	for (TActorIterator<ASnapZone> It(Object->GetWorld()); It; ++It)
	{
		if (IsValid(*It) && It->TrySnapOnRelease(Object))
		{
			return true;
		}
	}
	return false;
}

void ASnapZone::ReleaseObjectFromAnyZone(AInteractiveObject* Object)
{
	if (!IsValid(Object) || !IsValid(Object->GetWorld()))
	{
		return;
	}

	for (TActorIterator<ASnapZone> It(Object->GetWorld()); It; ++It)
	{
		if (IsValid(*It))
		{
			It->ReleaseIfSnapped(Object);
		}
	}
}

ASnapZone* ASnapZone::FindZoneById(const UWorld* World, FName InZoneId)
{
	if (!IsValid(World) || InZoneId == NAME_None)
	{
		return nullptr;
	}

	for (TActorIterator<ASnapZone> It(World); It; ++It)
	{
		if (IsValid(*It) && It->GetZoneId() == InZoneId)
		{
			return *It;
		}
	}
	return nullptr;
}

void ASnapZone::HandleSnappedObjectDestroyed(AActor* DestroyedActor)
{
	if (SnappedObject.Get() == DestroyedActor)
	{
		SnappedObject = nullptr;
	}
}
