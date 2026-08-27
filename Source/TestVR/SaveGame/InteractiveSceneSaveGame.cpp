// Copyright Epic Games, Inc. All Rights Reserved.

#include "SaveGame/InteractiveSceneSaveGame.h"

const FString& UInteractiveSceneSaveGame::GetSlotName()
{
	static const FString SlotName = TEXT("TestVRInteractiveScene");
	return SlotName;
}
