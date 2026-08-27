// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "Interaction/InteractiveObjectTypes.h"

#include "VRInfoWidget.generated.h"

class AInteractiveObject;
class UButton;
class UInteractiveObjectSubsystem;
class UPanelWidget;
class UTextBlock;
class UVRInfoEntryWidget;
class UVRInteractionComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVRInfoUpdated);

UCLASS(Abstract, Blueprintable)
class TESTVR_API UVRInfoWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "VR UI")
	void InitializeContext(UVRInteractionComponent* InInteraction);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VR UI")
	TArray<TSubclassOf<AInteractiveObject>> GetSpawnableCatalog() const;

	UFUNCTION(BlueprintCallable, Category = "VR UI")
	void RequestAddObject(TSubclassOf<AInteractiveObject> ObjectClass);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VR UI")
	TArray<AInteractiveObject*> GetDeletableObjects() const;

	UFUNCTION(BlueprintCallable, Category = "VR UI")
	void RequestDeleteObject(AInteractiveObject* ObjectToDelete);

	UFUNCTION(BlueprintCallable, Category = "VR UI")
	bool RequestSaveScene();

	UFUNCTION(BlueprintCallable, Category = "VR UI")
	bool RequestLoadScene();

	UPROPERTY(BlueprintReadOnly, Category = "VR UI")
	FInteractiveObjectData DisplayData;

	UPROPERTY(BlueprintReadOnly, Category = "VR UI")
	bool bHasDisplayTarget = false;

	UPROPERTY(BlueprintReadOnly, Category = "VR UI")
	int32 ObjectCount = 0;

	UPROPERTY(BlueprintAssignable, Category = "VR UI|Events")
	FOnVRInfoUpdated OnInfoUpdated;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VR UI")
	TSubclassOf<UVRInfoEntryWidget> EntryWidgetClass;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void RefreshDisplay();
	void RefreshVisuals();
	void RebuildAddList();
	void SyncDeleteList();
	UVRInfoEntryWidget* CreateEntryWidget();
	void UnbindContext();

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleLoadClicked();

	UFUNCTION()
	void HandleInteractionTargetChanged(AActor* CurrentTarget);

	UFUNCTION()
	void HandleRegistryChanged();

	UInteractiveObjectSubsystem* GetRegistry() const;

	UPROPERTY(Transient)
	TObjectPtr<UVRInteractionComponent> Interaction;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CurrentNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CurrentTypeText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CurrentValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectCountText;

	UPROPERTY(Transient)
	TObjectPtr<UPanelWidget> AddObjectList;

	UPROPERTY(Transient)
	TObjectPtr<UPanelWidget> DeleteObjectList;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LoadButton;

	TMap<TWeakObjectPtr<AInteractiveObject>, TWeakObjectPtr<UVRInfoEntryWidget>> DeleteEntries;
};
