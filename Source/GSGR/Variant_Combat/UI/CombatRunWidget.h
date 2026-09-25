// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatRunWidget.generated.h"

class UBackgroundBlur;
class UBorder;
class UButton;
class UTextBlock;
class UTexture2D;

/** Compact, code-built overlay shared by startup, FTUE messaging, and game over. */
UCLASS()
class UCombatRunWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UCombatRunWidget(const FObjectInitializer& ObjectInitializer);

	void ShowStartupMenu();
	void ShowTutorialMessage(const FText& Heading, const FText& Message);
	void ShowTutorialComplete();
	void ShowGameOver(float FinalSurvivalTime);
	void HideOverlay();
	void ShowShowcase();
	void ShowEvadeReminder();
	void BeginHideEvadeReminder();
	void HideEvadeReminder();
	UButton* GetReturnButton() const { return ReturnButton; }
	UButton* GetPlayButton() const { return PlayButton; }

protected:

	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION()
	void HandlePlayClicked();

	UFUNCTION()
	void HandleRestartClicked();

	UFUNCTION()
	void HandleQuitClicked();
	UFUNCTION()
	void HandleReturnClicked();
	UFUNCTION()
	void HandleReminderCloseClicked();

private:

	void BuildWidgetTree();
	void SetTutorialPanelLayout(bool bTutorial);
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
	TObjectPtr<UTextBlock> CombatCueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EvadeStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PlayButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RestartButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QuitButton;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ReturnButton;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> ReminderPanel;
	UPROPERTY(Transient)
	TObjectPtr<UButton> ReminderCloseButton;
	UPROPERTY()
	TObjectPtr<UTexture2D> ReminderTexture;
	bool bReminderVisible = false;
	bool bReminderClosing = false;
	double ReminderAnimationStart = 0.0;
};
