// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatMainMenuWidget.generated.h"

class UButton;

/** Designer-owned main menu. Extra buttons can call the controller's Blueprint menu functions. */
UCLASS()
class UCombatMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UButton* GetPlayButton() const { return PlayButton; }

protected:
	virtual void NativeOnInitialized() override;

	/** Designer button, or the built-in layout until a Designer tree is authored. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> PlayButton;

	void BuildDefaultLayout();

	UFUNCTION()
	void HandlePlayClicked();
};
