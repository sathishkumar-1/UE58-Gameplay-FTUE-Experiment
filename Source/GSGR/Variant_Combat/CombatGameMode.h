// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CombatGameMode.generated.h"

class ACombatCharacter;
class ACombatEnemy;
class ACombatEnemySpawner;
class ACombatPlayerController;
class UCombatFTUESaveGame;

/** Explicit first-time-user-experience states. */
UENUM(BlueprintType)
enum class ECombatFTUEState : uint8
{
	None,
	Welcome,
	LightAttack,
	HeavyAttack,
	Dodge,
	Complete
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCombatFTUEIntegrationHook);

/**
 * Combat run authority. It orchestrates the existing character, enemy, input,
 * damage, spawning, and death systems without implementing parallel gameplay.
 */
UCLASS(abstract)
class ACombatGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	ACombatGameMode();
	virtual void Tick(float DeltaSeconds) override;

	/** Reusable integration point fired once when an incomplete player's FTUE flow actually begins. */
	UPROPERTY(BlueprintAssignable, Category="FTUE|Integration")
	FCombatFTUEIntegrationHook OnFTUEStarted;

	/** Reusable integration point fired once after the full FTUE flow succeeds. */
	UPROPERTY(BlueprintAssignable, Category="FTUE|Integration")
	FCombatFTUEIntegrationHook OnFTUECompleted;

	/** UI/controller entry points. */
	void HandlePlaySelected();
	void HandleWelcomeContinue();
	void HandleRestartSelected();
	void HandleQuitSelected();

	UFUNCTION(BlueprintPure, Category="Run Flow")
	ECombatFTUEState GetFTUEState() const { return FTUEState; }

	UFUNCTION(BlueprintPure, Category="Run Flow")
	float GetSurvivalTime() const { return SurvivalTime; }

#if !UE_BUILD_SHIPPING
	/** Resets and persists the existing local-player FTUE profile for FTUE.Reset. */
	bool ResetFTUEProfile();
#endif

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** Determines how many local players should be spawned on game start. */
	UPROPERTY(EditDefaultsOnly, Category="Local Multiplayer", meta = (ClampMin = 1, ClampMax = 4))
	int32 NumberOfLocalPlayers = 1;

	/** Delay after continuing past Welcome before the Light Attack lesson appears. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FTUE|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float WelcomeToLightDelay = 0.0f;

	/** How long Light Attack success feedback remains before the Heavy Attack lesson. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FTUE|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float LightSuccessFeedbackDuration = 0.7f;

	/** How long Heavy Attack success feedback remains before the Dodge lesson. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FTUE|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float HeavySuccessFeedbackDuration = 0.7f;

	/** Delay after entering Dodge before the tutorial enemy starts its normal attack montage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FTUE|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DodgeAttackStartDelay = 0.0f;

	/** Deliberate frozen-pose hold before showing and enabling the Dodge Now prompt. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FTUE|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DodgePromptDelay = 0.0f;

	/** How long dodge-success feedback remains before FTUE completion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FTUE|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float DodgeSuccessFeedbackDuration = 0.7f;

	/** How long the Tutorial Complete feedback remains visible. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FTUE|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float TutorialCompleteFeedbackDuration = 1.25f;

	/** Local-player save slot used for FTUE completion persistence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="FTUE|Persistence")
	FString FTUEProfileSlotName = TEXT("GSGRPlayerProfile");

	/** Current explicit tutorial state. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Run Flow")
	ECombatFTUEState FTUEState = ECombatFTUEState::None;

	/** Assigns players to different PlayerStarts in the level. */
	int32 CurrentPlayerStartAssignment = 0;

private:

	void InitializeRunFlow();
	void ShowStartupMenu();
	void BeginFTUE();
	void EnterFTUEState(ECombatFTUEState NewState);
	void ScheduleFTUETransition(ECombatFTUEState NewState, float Delay);
	void ApplyPendingFTUEState();
	void CompleteFTUE();
	void StartNormalGameplay(bool bShowTutorialComplete = false);
	void HideTutorialComplete();
	void BindTutorialActors();
	void ConfigureTutorialEnemy(ACombatEnemy* Enemy);
	void CleanupFTUE();
	void HandleDodgeAttackStartDelayElapsed();
	void StartTutorialDodgeAttackIfReady();
	void ShowDodgePrompt();
	void SetPlayerCombatPermissions(bool bLight, bool bHeavy, bool bDodge, bool bCancelDisallowed = true);
	void RestartLevel(bool bSkipStartupMenu);
	void RestartAfterIncompleteFTUE();
	void LoadFTUEProfile();
	void SaveFTUECompletion();

	UFUNCTION()
	void HandlePlayerDied();

	UFUNCTION()
	void HandleEnemySpawned(ACombatEnemy* Enemy);

	UFUNCTION()
	void HandleEnemyDamaged(ACombatEnemy* Enemy, AActor* DamageCauser);

	UFUNCTION()
	void HandleTutorialDodgeWindow(ACombatEnemy* Enemy);

	UFUNCTION()
	void HandlePlayerDodgeStarted();

	UPROPERTY(Transient)
	TObjectPtr<ACombatPlayerController> CombatPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<ACombatCharacter> CombatPlayer;

	UPROPERTY(Transient)
	TObjectPtr<ACombatEnemy> TutorialEnemy;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ACombatEnemySpawner>> TutorialSpawners;

	UPROPERTY(Transient)
	TObjectPtr<UCombatFTUESaveGame> FTUESaveGame;

	ECombatFTUEState PendingFTUEState = ECombatFTUEState::None;
	bool bFTUETransitionPending = false;
	bool bWaitingForDodgeInput = false;
	bool bDodgeAttackStartDelayElapsed = false;
	bool bDodgePromptPending = false;
	bool bFTUEStartedHookFired = false;
	bool bFTUECompletedHookFired = false;
	bool bRunActive = false;
	bool bGameOver = false;
	float SurvivalTime = 0.0f;
	float FinalSurvivalTime = 0.0f;

	FTimerHandle FTUETransitionTimer;
	FTimerHandle DodgeAttackStartTimer;
	FTimerHandle DodgePromptTimer;
	FTimerHandle TutorialCompleteUITimer;
	FTimerHandle TutorialDeathRestartTimer;
};
