// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatRunWidget.generated.h"

class UBackgroundBlur;
class UBorder;
class UButton;
class UTextBlock;

/** Compact, code-built overlay shared by startup, FTUE messaging, and game over. */
UCLASS()
class UCombatRunWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	void ShowStartupMenu();
	void ShowTutorialMessage(const FText& Heading, const FText& Message);
	void ShowTutorialComplete();
	void ShowGameOver(float FinalSurvivalTime);
	void HideOverlay();
	UButton* GetPlayButton() const { return PlayButton; }

protected:

	virtual void NativeOnInitialized() override;

	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleRestartClicked();

	UFUNCTION()
	void HandleQuitClicked();

private:

	void BuildWidgetTree();
	void SetMenuInteractionEnabled(bool bEnabled);

	UPROPERTY(Transient)
	TObjectPtr<UBackgroundBlur> BackgroundBlur;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> MessagePanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeadingText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ScoreText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PlayButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RestartButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QuitButton;
};
