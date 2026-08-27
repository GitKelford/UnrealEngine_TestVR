// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "VRInfoEntryWidget.generated.h"

class AInteractiveObject;
class UButton;
class UTextBlock;
class UVRInfoWidget;

UCLASS(Abstract, Blueprintable)
class TESTVR_API UVRInfoEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeAdd(UVRInfoWidget* InOwner, TSubclassOf<AInteractiveObject> InClass, const FText& InLabel);

	void InitializeDelete(UVRInfoWidget* InOwner, AInteractiveObject* InObject, const FText& InLabel);

	UPROPERTY(BlueprintReadOnly, Category = "VR UI")
	FText EntryText;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "VR UI")
	bool IsDeleteEntry() const { return !SpawnClass && DeleteObject.IsValid(); }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "VR UI")
	TObjectPtr<UButton> EntryButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "VR UI")
	TObjectPtr<UTextBlock> EntryLabel;

private:
	UFUNCTION()
	void HandleClicked();

	void ApplyLabel();

	TWeakObjectPtr<UVRInfoWidget> OwnerWidget;
	TSubclassOf<AInteractiveObject> SpawnClass;
	TWeakObjectPtr<AInteractiveObject> DeleteObject;
};
