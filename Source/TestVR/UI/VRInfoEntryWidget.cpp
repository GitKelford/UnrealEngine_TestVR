// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VRInfoEntryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Objects/InteractiveObject.h"
#include "UI/VRInfoWidget.h"

void UVRInfoEntryWidget::InitializeAdd(UVRInfoWidget* InOwner, TSubclassOf<AInteractiveObject> InClass, const FText& InLabel)
{
	OwnerWidget = InOwner;
	SpawnClass = InClass;
	DeleteObject.Reset();
	EntryText = InLabel;
	ApplyLabel();
}

void UVRInfoEntryWidget::InitializeDelete(UVRInfoWidget* InOwner, AInteractiveObject* InObject, const FText& InLabel)
{
	OwnerWidget = InOwner;
	SpawnClass = nullptr;
	DeleteObject = InObject;
	EntryText = InLabel;
	ApplyLabel();
}

void UVRInfoEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (EntryButton)
	{
		EntryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);
	}
	ApplyLabel();
}

void UVRInfoEntryWidget::NativeDestruct()
{
	if (EntryButton)
	{
		EntryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleClicked);
	}
	Super::NativeDestruct();
}

void UVRInfoEntryWidget::ApplyLabel()
{
	if (EntryLabel)
	{
		EntryLabel->SetText(EntryText);
	}
}

void UVRInfoEntryWidget::HandleClicked()
{
	UVRInfoWidget* Owner = OwnerWidget.Get();
	if (!Owner)
	{
		return;
	}

	if (SpawnClass)
	{
		Owner->RequestAddObject(SpawnClass);
	}
	else if (AInteractiveObject* Object = DeleteObject.Get())
	{
		Owner->RequestDeleteObject(Object);
	}
}
