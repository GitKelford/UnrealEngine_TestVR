// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/VRInfoWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interaction/Interactable.h"
#include "Interaction/VRInteractionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Objects/InteractiveObject.h"
#include "Objects/InteractiveObjectSubsystem.h"
#include "UI/VRInfoEntryWidget.h"

namespace
{
	bool IsWorldTearingDown(const UObject* Context)
	{
		const UWorld* World = IsValid(Context) ? Context->GetWorld() : nullptr;
		return !World || World->bIsTearingDown;
	}
}

UInteractiveObjectSubsystem* UVRInfoWidget::GetRegistry() const
{
	return UInteractiveObjectSubsystem::Get(this);
}

void UVRInfoWidget::NativeConstruct()
{
	Super::NativeConstruct();

	CurrentNameText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CurrentNameText")));
	CurrentTypeText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CurrentTypeText")));
	CurrentValueText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CurrentValueText")));
	ObjectCountText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("ObjectCountText")));
	AddObjectList = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("AddObjectList")));
	DeleteObjectList = Cast<UPanelWidget>(WidgetTree->FindWidget(TEXT("DeleteObjectList")));
	SaveButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("SaveButton")));
	LoadButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("LoadButton")));

	if (SaveButton)
	{
		SaveButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSaveClicked);
	}
	if (LoadButton)
	{
		LoadButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLoadClicked);
	}

	// Self-wire from the local pawn unless a Blueprint already called InitializeContext.
	UVRInteractionComponent* ResolvedInteraction = Interaction;
	if (!IsValid(ResolvedInteraction))
	{
		const APawn* OwningPawn = GetOwningPlayerPawn();
		if (!OwningPawn)
		{
			OwningPawn = UGameplayStatics::GetPlayerPawn(this, 0);
		}
		if (OwningPawn)
		{
			ResolvedInteraction = OwningPawn->FindComponentByClass<UVRInteractionComponent>();
		}
	}

	InitializeContext(ResolvedInteraction);
}

void UVRInfoWidget::InitializeContext(UVRInteractionComponent* InInteraction)
{
	UnbindContext();

	Interaction = InInteraction;

	if (IsValid(Interaction))
	{
		Interaction->OnInteractionTargetChanged.AddUniqueDynamic(this, &ThisClass::HandleInteractionTargetChanged);
	}
	if (UInteractiveObjectSubsystem* Registry = GetRegistry())
	{
		Registry->OnRegistryChanged.AddUniqueDynamic(this, &ThisClass::HandleRegistryChanged);
	}

	RebuildAddList();
	if (DeleteObjectList)
	{
		DeleteObjectList->ClearChildren();
	}
	DeleteEntries.Reset();
	SyncDeleteList();
	RefreshDisplay();
}

void UVRInfoWidget::NativeDestruct()
{
	if (SaveButton)
	{
		SaveButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleSaveClicked);
	}
	if (LoadButton)
	{
		LoadButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleLoadClicked);
	}
	UnbindContext();
	DeleteEntries.Reset();
	Super::NativeDestruct();
}

TArray<TSubclassOf<AInteractiveObject>> UVRInfoWidget::GetSpawnableCatalog() const
{
	if (UInteractiveObjectSubsystem* Registry = GetRegistry())
	{
		return Registry->GetSpawnableClasses();
	}
	return {};
}

void UVRInfoWidget::RequestAddObject(TSubclassOf<AInteractiveObject> ObjectClass)
{
	if (UInteractiveObjectSubsystem* Registry = GetRegistry())
	{
		Registry->SpawnObject(ObjectClass);
	}
}

TArray<AInteractiveObject*> UVRInfoWidget::GetDeletableObjects() const
{
	if (UInteractiveObjectSubsystem* Registry = GetRegistry())
	{
		return Registry->GetObjects();
	}
	return {};
}

void UVRInfoWidget::RequestDeleteObject(AInteractiveObject* ObjectToDelete)
{
	if (UInteractiveObjectSubsystem* Registry = GetRegistry())
	{
		Registry->RequestDeleteObject(ObjectToDelete);
	}
}

bool UVRInfoWidget::RequestSaveScene()
{
	UInteractiveObjectSubsystem* Registry = GetRegistry();
	return Registry && Registry->SaveScene();
}

bool UVRInfoWidget::RequestLoadScene()
{
	UInteractiveObjectSubsystem* Registry = GetRegistry();
	return Registry && Registry->LoadScene();
}

void UVRInfoWidget::RefreshDisplay()
{
	AActor* Target = IsValid(Interaction) ? Interaction->GetCurrentInteractionTarget() : nullptr;

	bHasDisplayTarget = false;
	DisplayData = FInteractiveObjectData();
	if (IInteractable* Interactable = Cast<IInteractable>(Target))
	{
		DisplayData = Interactable->GetInteractionData();
		bHasDisplayTarget = true;
	}

	const UInteractiveObjectSubsystem* Registry = GetRegistry();
	ObjectCount = Registry ? Registry->GetObjectCount() : 0;

	RefreshVisuals();
	OnInfoUpdated.Broadcast();
}

void UVRInfoWidget::RefreshVisuals()
{
	const FText EmptyValue = FText::FromString(TEXT("-"));
	const UEnum* TypeEnum = StaticEnum<EInteractiveObjectType>();
	const FText NameValue = bHasDisplayTarget ? DisplayData.Name : EmptyValue;
	const FText TypeValue = bHasDisplayTarget && TypeEnum
		? TypeEnum->GetDisplayNameTextByValue(static_cast<int64>(DisplayData.Type))
		: EmptyValue;
	const FText ValueText = bHasDisplayTarget ? FText::AsNumber(DisplayData.Value) : EmptyValue;

	if (CurrentNameText)
	{
		CurrentNameText->SetText(FText::Format(NSLOCTEXT("VRInfo", "Name", "Name: {0}"), NameValue));
	}
	if (CurrentTypeText)
	{
		CurrentTypeText->SetText(FText::Format(NSLOCTEXT("VRInfo", "Type", "Type: {0}"), TypeValue));
	}
	if (CurrentValueText)
	{
		CurrentValueText->SetText(FText::Format(NSLOCTEXT("VRInfo", "Value", "Value: {0}"), ValueText));
	}
	if (ObjectCountText)
	{
		ObjectCountText->SetText(FText::Format(NSLOCTEXT("VRInfo", "Count", "Objects: {0}"), FText::AsNumber(ObjectCount)));
	}
}

UVRInfoEntryWidget* UVRInfoWidget::CreateEntryWidget()
{
	if (IsWorldTearingDown(this) || !EntryWidgetClass)
	{
		return nullptr;
	}
	return CreateWidget<UVRInfoEntryWidget>(this, EntryWidgetClass);
}

void UVRInfoWidget::RebuildAddList()
{
	if (!AddObjectList)
	{
		return;
	}

	AddObjectList->ClearChildren();
	for (const TSubclassOf<AInteractiveObject>& ObjectClass : GetSpawnableCatalog())
	{
		if (!ObjectClass)
		{
			continue;
		}

		const AInteractiveObject* Defaults = ObjectClass->GetDefaultObject<AInteractiveObject>();
		const FText ClassLabel = Defaults ? Defaults->GetData().Name : FText::FromString(ObjectClass->GetName());

		if (UVRInfoEntryWidget* Entry = CreateEntryWidget())
		{
			Entry->InitializeAdd(this, ObjectClass, FText::Format(NSLOCTEXT("VRInfo", "AddEntry", "+  {0}"), ClassLabel));
			AddObjectList->AddChild(Entry);
		}
	}
}

void UVRInfoWidget::SyncDeleteList()
{
	if (!DeleteObjectList)
	{
		return;
	}

	// Diff against the existing rows: keep unchanged entries, add / remove only what changed.
	TArray<AInteractiveObject*> DesiredObjects;
	TSet<TWeakObjectPtr<AInteractiveObject>> DesiredSet;
	for (AInteractiveObject* Object : GetDeletableObjects())
	{
		if (IsValid(Object) && Object->CanDelete())
		{
			DesiredObjects.Add(Object);
			DesiredSet.Add(Object);
		}
	}

	for (auto It = DeleteEntries.CreateIterator(); It; ++It)
	{
		UVRInfoEntryWidget* Entry = It.Value().Get();
		if (!DesiredSet.Contains(It.Key()) || !IsValid(Entry))
		{
			if (IsValid(Entry))
			{
				DeleteObjectList->RemoveChild(Entry);
			}
			It.RemoveCurrent();
		}
	}

	for (AInteractiveObject* Object : DesiredObjects)
	{
		if (DeleteEntries.Contains(Object))
		{
			continue;
		}

		if (UVRInfoEntryWidget* Entry = CreateEntryWidget())
		{
			Entry->InitializeDelete(this, Object, FText::Format(NSLOCTEXT("VRInfo", "DeleteEntry", "Delete  {0}"), Object->GetData().Name));
			DeleteObjectList->AddChild(Entry);
			DeleteEntries.Add(Object, Entry);
		}
	}
}

void UVRInfoWidget::HandleSaveClicked()
{
	RequestSaveScene();
}

void UVRInfoWidget::HandleLoadClicked()
{
	RequestLoadScene();
}

void UVRInfoWidget::HandleInteractionTargetChanged(AActor* CurrentTarget)
{
	RefreshDisplay();
}

void UVRInfoWidget::HandleRegistryChanged()
{
	if (IsWorldTearingDown(this))
	{
		return;
	}
	SyncDeleteList();
	RefreshDisplay();
}

void UVRInfoWidget::UnbindContext()
{
	if (IsValid(Interaction))
	{
		Interaction->OnInteractionTargetChanged.RemoveDynamic(this, &ThisClass::HandleInteractionTargetChanged);
	}
	if (UInteractiveObjectSubsystem* Registry = GetRegistry())
	{
		Registry->OnRegistryChanged.RemoveDynamic(this, &ThisClass::HandleRegistryChanged);
	}
	Interaction = nullptr;
}
