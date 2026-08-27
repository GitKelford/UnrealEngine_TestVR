// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"

#include "InteractiveObjectTypes.h"

#include "Interactable.generated.h"

class USceneComponent;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class TESTVR_API UInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Contract between UVRInteractionComponent and any grabbable world object.
 * The interaction code only ever talks to this interface, never to a concrete class.
 */
class TESTVR_API IInteractable
{
	GENERATED_BODY()

public:
	/** Data shown in the VR panel (name / type / value). */
	virtual FInteractiveObjectData GetInteractionData() const = 0;

	virtual void BeginHover() = 0;
	virtual void EndHover() = 0;

	virtual bool CanGrab() const = 0;
	/** One-hand grab: attach to AttachTo, keeping the current world transform. */
	virtual bool BeginGrab(USceneComponent* AttachTo) = 0;
	/** Second controller joins an object already held by the first. */
	virtual void BeginSecondHandGrab() = 0;
	/** Second controller lets go: re-attach to the primary hand. */
	virtual void EndSecondHandGrab(USceneComponent* ReattachTo) = 0;
	/** Final release by the primary hand. */
	virtual void EndGrab() = 0;

	virtual bool CanDelete() const = 0;
	/** Returns true only once the object has actually been destroyed. */
	virtual bool RequestDestroy() = 0;
};
