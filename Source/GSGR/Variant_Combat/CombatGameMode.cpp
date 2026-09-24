// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Combat/CombatGameMode.h"
#include "Variant_Combat/CombatCharacter.h"
#include "Variant_Combat/CombatFTUESaveGame.h"
#include "Variant_Combat/CombatPlayerController.h"
#include "Variant_Combat/AI/CombatEnemy.h"
#include "Variant_Combat/AI/CombatEnemySpawner.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TimerManager.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "GSGR.h"

namespace
{
#if !UE_BUILD_SHIPPING
	void ResetFTUEConsoleCommand(UWorld* World)
	{
		ACombatGameMode* CombatGameMode = World ? World->GetAuthGameMode<ACombatGameMode>() : nullptr;
		if (!CombatGameMode)
		{
			UE_LOG(LogGSGR, Error, TEXT("FTUE.Reset failed: no active CombatGameMode. Run the command from a combat game or PIE session."));
			return;
		}

		CombatGameMode->ResetFTUEProfile();
	}

	FAutoConsoleCommandWithWorld ResetFTUECommand(
		TEXT("FTUE.Reset"),
		TEXT("Resets the existing per-local-player FTUE completion profile. The next startup treats the player as new."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&ResetFTUEConsoleCommand));
#endif
}

ACombatGameMode::ACombatGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
}

void ACombatGameMode::BeginPlay()
{
	Super::BeginPlay();
	const FString LevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	bFullFlowDemo = LevelName == TEXT("Level_Full_Flow");
	bMainMenuLevel = LevelName == TEXT("Level_Main_Menu");
	bShowcaseLevel = LevelName == TEXT("Level_Control_Recap_Showcase");
	if (bFullFlowDemo || bMainMenuLevel || bShowcaseLevel) SetupDemoSpawners();

	// Player zero is created by normal game initialization.
	for (int32 i = 2; i <= NumberOfLocalPlayers; ++i)
	{
		UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	}

	InitializeRunFlow();
}

void ACombatGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetDemoTimers();
	GetWorldTimerManager().ClearTimer(FTUETransitionTimer);
	GetWorldTimerManager().ClearTimer(DodgeAttackStartTimer);
	GetWorldTimerManager().ClearTimer(DodgePromptTimer);
	GetWorldTimerManager().ClearTimer(TutorialCompleteUITimer);
	GetWorldTimerManager().ClearTimer(TutorialDeathRestartTimer);
	GetWorldTimerManager().ClearTimer(DeathPauseTimer);
	CleanupFTUE();

	if (CombatPlayer)
	{
		CombatPlayer->OnPlayerDied.RemoveDynamic(this, &ACombatGameMode::HandlePlayerDied);
		CombatPlayer->OnDodgeStarted.RemoveDynamic(this, &ACombatGameMode::HandlePlayerDodgeStarted);
	}

	Super::EndPlay(EndPlayReason);
}

#if !UE_BUILD_SHIPPING
bool ACombatGameMode::ResetFTUEProfile()
{
	ACombatPlayerController* LocalController = CombatPlayerController.Get();
	if (!LocalController)
	{
		LocalController = Cast<ACombatPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	}

	if (!LocalController)
	{
		UE_LOG(LogGSGR, Error, TEXT("FTUE.Reset failed: the primary local player controller is unavailable."));
		return false;
	}

	UCombatFTUESaveGame* Profile = FTUESaveGame.Get();
	if (!Profile)
	{
		Profile = Cast<UCombatFTUESaveGame>(
			ULocalPlayerSaveGame::LoadOrCreateSaveGameForLocalPlayer(
				UCombatFTUESaveGame::StaticClass(), LocalController, FTUEProfileSlotName));
	}

	if (!Profile)
	{
		UE_LOG(LogGSGR, Error, TEXT("FTUE.Reset failed: profile '%s' could not be loaded or created."), *FTUEProfileSlotName);
		return false;
	}

	Profile->ResetToDefault();
	if (!Profile->SaveGameToSlotForLocalPlayer())
	{
		UE_LOG(LogGSGR, Error, TEXT("FTUE.Reset failed: profile '%s' could not be saved."), *FTUEProfileSlotName);
		return false;
	}

	FTUESaveGame = Profile;
	UE_LOG(LogGSGR, Display, TEXT("FTUE.Reset succeeded: bHasCompletedFTUE is false in local-player profile '%s'."), *FTUEProfileSlotName);
	return true;
}
#endif

void ACombatGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bFullFlowDemo && FullFlowStage == EFullFlowStage::PostShowcaseFlurry
		&& !bReminderShown && IsValid(DemoEnemy) && DemoEnemy->GetFlurryState() == ECombatFlurryState::Windup)
	{
		ShowEvadeReminder();
	}
	if (bFullFlowDemo && FTUEState == ECombatFTUEState::Evade && IsValid(TutorialEnemy)
		&& TutorialEnemy->GetFlurryState() == ECombatFlurryState::Recovering && !bDemoTransitionPending)
	{
		RetryCurrentLesson();
	}
	if (bRunActive)
	{
		SurvivalTime += DeltaSeconds;
	}
}

void ACombatGameMode::InitializeRunFlow()
{
	CombatPlayerController = Cast<ACombatPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	CombatPlayer = CombatPlayerController ? Cast<ACombatCharacter>(CombatPlayerController->GetPawn()) : nullptr;
	if (!CombatPlayerController || !CombatPlayer)
	{
		UE_LOG(LogGSGR, Error, TEXT("Combat run flow could not find the primary combat player/controller."));
		return;
	}

	CombatPlayer->OnPlayerDied.AddUniqueDynamic(this, &ACombatGameMode::HandlePlayerDied);
	CombatPlayer->OnDodgeStarted.AddUniqueDynamic(this, &ACombatGameMode::HandlePlayerDodgeStarted);
	LoadFTUEProfile();
	if (bShowcaseLevel)
	{
		FullFlowStage = EFullFlowStage::Showcase;
		SetPlayerCombatPermissions(false, false, false);
		CombatPlayerController->ShowShowcase();
		UGameplayStatics::SetGamePaused(this, true);
		return;
	}
	if (bFullFlowDemo)
	{
		if (UGameplayStatics::HasOption(OptionsString, TEXT("FromShowcase")))
		{
			OnShowcaseReturned.Broadcast();
			// Let the newly loaded player's BeginPlay create its widget before resetting HP.
			GetWorldTimerManager().SetTimerForNextTick(this, &ACombatGameMode::StartPostShowcaseFlurry);
		}
		else BeginFTUE();
		return;
	}

	if (UGameplayStatics::HasOption(OptionsString, TEXT("RestartRun")))
	{
		StartNormalGameplay();
	}
	else
	{
		ShowStartupMenu();
	}
}

void ACombatGameMode::ShowStartupMenu()
{
	bRunActive = false;
	bGameOver = false;
	SurvivalTime = 0.0f;
	FinalSurvivalTime = 0.0f;
	FTUEState = ECombatFTUEState::None;
	SetPlayerCombatPermissions(false, false, false);
	CombatPlayerController->SetAwaitingWelcomeInput(false);
	CombatPlayerController->ShowStartupMenu();
	UGameplayStatics::SetGamePaused(this, true);
}

void ACombatGameMode::HandlePlaySelected()
{
	if (bMainMenuLevel)
	{
		UGameplayStatics::SetGamePaused(this, false);
		UGameplayStatics::OpenLevel(this, TEXT("Level_Full_Flow"));
		return;
	}
	if (bRunActive || bGameOver || FTUEState != ECombatFTUEState::None)
	{
		return;
	}

	UGameplayStatics::SetGamePaused(this, false);
	CombatPlayerController->HideRunFlowUI();
	if (bFullFlowDemo || !FTUESaveGame || !FTUESaveGame->bHasCompletedFTUE)
	{
		BeginFTUE();
	}
	else
	{
		StartNormalGameplay();
	}
}

void ACombatGameMode::BeginFTUE()
{
	if (bFullFlowDemo) FullFlowStage = EFullFlowStage::Tutorial;
	bRunActive = false;
	bGameOver = false;
	SurvivalTime = 0.0f;
	FinalSurvivalTime = 0.0f;
	if (!bFullFlowDemo) BindTutorialActors();
	EnterFTUEState(ECombatFTUEState::Welcome);

	if (!bFTUEStartedHookFired)
	{
		bFTUEStartedHookFired = true;
		UE_LOG(LogGSGR, Display, TEXT("FTUE Started integration hook fired"));
		OnFTUEStarted.Broadcast();
	}
}

void ACombatGameMode::EnterFTUEState(ECombatFTUEState NewState)
{
	if (bFullFlowDemo) ResetDemoTimers();
	GetWorldTimerManager().ClearTimer(FTUETransitionTimer);
	GetWorldTimerManager().ClearTimer(DodgeAttackStartTimer);
	GetWorldTimerManager().ClearTimer(DodgePromptTimer);
	bFTUETransitionPending = false;
	bDodgeAttackStartDelayElapsed = false;
	bDodgePromptPending = false;
	PendingFTUEState = ECombatFTUEState::None;
	FTUEState = NewState;
	CombatPlayerController->SetAwaitingWelcomeInput(false);

	switch (FTUEState)
	{
	case ECombatFTUEState::Welcome:
		SetPlayerCombatPermissions(false, false, false);
		CombatPlayerController->ShowTutorialMessage(
			NSLOCTEXT("CombatFTUE", "WelcomeHeading", "Welcome"),
			NSLOCTEXT("CombatFTUE", "WelcomePrompt", "Press any button to continue"));
		CombatPlayerController->SetAwaitingWelcomeInput(true);
		break;

	case ECombatFTUEState::LightAttack:
		if (bFullFlowDemo && !IsValid(TutorialEnemy)) SpawnTutorialDemoEnemy(false);
		SetPlayerCombatPermissions(true, false, false);
		CombatPlayerController->ShowTutorialMessage(
			NSLOCTEXT("CombatFTUE", "LightHeading", "Light Attack"),
			FText::Format(
				NSLOCTEXT("CombatFTUE", "LightPrompt", "Hit the enemy with Light Attack\n{0}"),
				CombatPlayerController->GetBindingDisplayText(CombatPlayer->GetLightAttackAction())));
		break;

	case ECombatFTUEState::HeavyAttack:
		SetPlayerCombatPermissions(false, true, false);
		CombatPlayerController->ShowTutorialMessage(
			NSLOCTEXT("CombatFTUE", "HeavyHeading", "Heavy Attack"),
			FText::Format(
				NSLOCTEXT("CombatFTUE", "HeavyPrompt", "Hit the enemy with Heavy Attack\n{0}"),
				CombatPlayerController->GetBindingDisplayText(CombatPlayer->GetHeavyAttackAction())));
		break;

	case ECombatFTUEState::Dodge:
		if (bFullFlowDemo && !IsValid(TutorialEnemy)) SpawnTutorialDemoEnemy(false);
		bWaitingForDodgeInput = false;
		SetPlayerCombatPermissions(false, false, false);
		CombatPlayerController->ShowTutorialMessage(
			NSLOCTEXT("CombatFTUE", "DodgeHeading", "Dodge"),
			NSLOCTEXT("CombatFTUE", "DodgeWatch", "Watch the enemy's attack..."));
		if (DodgeAttackStartDelay <= 0.0f)
		{
			HandleDodgeAttackStartDelayElapsed();
		}
		else
		{
			GetWorldTimerManager().SetTimer(
				DodgeAttackStartTimer, this, &ACombatGameMode::HandleDodgeAttackStartDelayElapsed, DodgeAttackStartDelay, false);
		}
		break;

	case ECombatFTUEState::Evade:
		if (bFullFlowDemo)
		{
			ClearDemoEnemy();
			SpawnTutorialDemoEnemy(true);
			SetPlayerCombatPermissions(false, false, false);
			CombatPlayer->SetEvadingAllowed(true);
			CombatPlayerController->ShowTutorialMessage(
				NSLOCTEXT("CombatFTUE", "EvadeHeading", "Evade the Flurry"),
				FText::Format(NSLOCTEXT("CombatFTUE", "EvadePrompt", "Flurry incoming - hold {0} to evade a hit"),
					CombatPlayerController->GetBindingDisplayText(CombatPlayer->GetEvadeAction())));
			if (TutorialEnemy) TutorialEnemy->StartTutorialFlurry(CombatPlayer, EvadePromptDelay);
		}
		break;

	case ECombatFTUEState::Complete:
		CompleteFTUE();
		break;

	case ECombatFTUEState::None:
	default:
		break;
	}
}

void ACombatGameMode::ScheduleFTUETransition(ECombatFTUEState NewState, float Delay)
{
	bFTUETransitionPending = true;
	PendingFTUEState = NewState;

	if (Delay <= 0.0f)
	{
		ApplyPendingFTUEState();
		return;
	}

	GetWorldTimerManager().SetTimer(
		FTUETransitionTimer, this, &ACombatGameMode::ApplyPendingFTUEState, Delay, false);
}

void ACombatGameMode::HandleWelcomeContinue()
{
	if (FTUEState == ECombatFTUEState::Welcome && !bFTUETransitionPending)
	{
		CombatPlayerController->SetAwaitingWelcomeInput(false);
		ScheduleFTUETransition(ECombatFTUEState::LightAttack, WelcomeToLightDelay);
	}
}

void ACombatGameMode::HandleEnemyDamaged(ACombatEnemy* Enemy, AActor* DamageCauser)
{
	if (bFTUETransitionPending || Enemy != TutorialEnemy || DamageCauser != CombatPlayer)
	{
		return;
	}

	const ECombatPlayerAttackType AttackType = CombatPlayer->GetActiveAttackType();
	if (FTUEState == ECombatFTUEState::LightAttack && AttackType == ECombatPlayerAttackType::Light)
	{
		SetPlayerCombatPermissions(false, false, false, false);
		CombatPlayerController->ShowTutorialMessage(
			NSLOCTEXT("CombatFTUE", "GoodJobHeading", "Good Job"),
			NSLOCTEXT("CombatFTUE", "GoodJobMessage", "Now get ready for a Heavy Attack."));
		ScheduleFTUETransition(ECombatFTUEState::HeavyAttack, LightSuccessFeedbackDuration);
	}
	else if (FTUEState == ECombatFTUEState::HeavyAttack && AttackType == ECombatPlayerAttackType::Heavy)
	{
		SetPlayerCombatPermissions(false, false, false, false);
		CombatPlayerController->ShowTutorialMessage(
			NSLOCTEXT("CombatFTUE", "HeavySuccessHeading", "Good Job"),
			NSLOCTEXT("CombatFTUE", "HeavySuccessMessage", "Next, avoid the incoming attack."));
		ScheduleFTUETransition(ECombatFTUEState::Dodge, HeavySuccessFeedbackDuration);
	}
}

void ACombatGameMode::ApplyPendingFTUEState()
{
	const ECombatFTUEState NextState = PendingFTUEState;
	PendingFTUEState = ECombatFTUEState::None;
	if (bFTUETransitionPending && NextState != ECombatFTUEState::None)
	{
		EnterFTUEState(NextState);
	}
}

void ACombatGameMode::HandleTutorialDodgeWindow(ACombatEnemy* Enemy)
{
	if (FTUEState != ECombatFTUEState::Dodge || Enemy != TutorialEnemy || bWaitingForDodgeInput
		|| bDodgePromptPending || bFTUETransitionPending)
	{
		return;
	}

	bDodgePromptPending = true;
	if (DodgePromptDelay <= 0.0f)
	{
		ShowDodgePrompt();
	}
	else
	{
		GetWorldTimerManager().SetTimer(DodgePromptTimer, this, &ACombatGameMode::ShowDodgePrompt, DodgePromptDelay, false);
	}
}

void ACombatGameMode::ShowDodgePrompt()
{
	if (FTUEState != ECombatFTUEState::Dodge || !bDodgePromptPending || bFTUETransitionPending)
	{
		bDodgePromptPending = false;
		return;
	}

	bDodgePromptPending = false;
	bWaitingForDodgeInput = true;
	SetPlayerCombatPermissions(false, false, true);
	CombatPlayerController->ShowTutorialMessage(
		NSLOCTEXT("CombatFTUE", "DodgeNowHeading", "Dodge Now"),
		FText::Format(
			NSLOCTEXT("CombatFTUE", "DodgeNowPrompt", "Avoid the attack\n{0}"),
			CombatPlayerController->GetBindingDisplayText(CombatPlayer->GetDodgeAction())));
	if (bFullFlowDemo)
	{
		GetWorldTimerManager().SetTimer(DodgeResolveTimer, this, &ACombatGameMode::ResolveDodgeAttempt, 1.2f, false);
	}
}

void ACombatGameMode::HandlePlayerDodgeStarted()
{
	if (FTUEState != ECombatFTUEState::Dodge || !bWaitingForDodgeInput || bFTUETransitionPending)
	{
		return;
	}

	bWaitingForDodgeInput = false;
	bDodgePromptPending = false;
	GetWorldTimerManager().ClearTimer(DodgePromptTimer);
	if (bFullFlowDemo)
	{
		GetWorldTimerManager().ClearTimer(DodgeResolveTimer);
		if (TutorialEnemy) TutorialEnemy->ResolveTutorialDodgeAttack();
		if (bDemoTransitionPending) return;
		SetPlayerCombatPermissions(false, false, false, false);
		CombatPlayerController->ShowTutorialMessage(
			NSLOCTEXT("CombatFTUE", "DodgeSuccessHeading", "Good Job"),
			NSLOCTEXT("CombatFTUE", "DodgeSuccessMessage", "Attack avoided."));
		ScheduleFTUETransition(ECombatFTUEState::Evade, DodgeSuccessFeedbackDuration);
		return;
	}
	SetPlayerCombatPermissions(false, false, false, false);
	if (TutorialEnemy)
	{
		TutorialEnemy->ResolveTutorialDodgeAttack();
	}
	if (bDemoTransitionPending) return;

	CombatPlayerController->ShowTutorialMessage(
		NSLOCTEXT("CombatFTUE", "DodgeSuccessHeading", "Good Job"),
		NSLOCTEXT("CombatFTUE", "DodgeSuccessMessage", "Attack avoided."));
	ScheduleFTUETransition(bFullFlowDemo ? ECombatFTUEState::Evade : ECombatFTUEState::Complete,
		DodgeSuccessFeedbackDuration);
}

void ACombatGameMode::CompleteFTUE()
{
	SaveFTUECompletion();
	CleanupFTUE();
	if (bFullFlowDemo) ClearDemoEnemy();
	StartNormalGameplay(true);
	if (bFullFlowDemo) StartBasicEncounter();

	if (!bFTUECompletedHookFired)
	{
		bFTUECompletedHookFired = true;
		UE_LOG(LogGSGR, Display, TEXT("FTUE Completed integration hook fired"));
		OnFTUECompleted.Broadcast();
	}
}

void ACombatGameMode::StartNormalGameplay(bool bShowTutorialComplete)
{
	UGameplayStatics::SetGamePaused(this, false);
	bGameOver = false;
	bRunActive = true;
	SurvivalTime = 0.0f;
	FinalSurvivalTime = 0.0f;
	SetPlayerCombatPermissions(true, true, true);
	if (CombatPlayer) CombatPlayer->SetEvadingAllowed(true);
	CombatPlayerController->SetAwaitingWelcomeInput(false);

	if (bShowTutorialComplete)
	{
		CombatPlayerController->ShowTutorialComplete();
		GetWorldTimerManager().SetTimer(
			TutorialCompleteUITimer, this, &ACombatGameMode::HideTutorialComplete, TutorialCompleteFeedbackDuration, false);
	}
	else
	{
		FTUEState = ECombatFTUEState::None;
		CombatPlayerController->HideRunFlowUI();
	}
}

void ACombatGameMode::HideTutorialComplete()
{
	if (bRunActive && !bGameOver)
	{
		CombatPlayerController->HideRunFlowUI();
	}
}

void ACombatGameMode::BindTutorialActors()
{
	TutorialSpawners.Reset();
	TArray<AActor*> SpawnerActors;
	UGameplayStatics::GetAllActorsOfClass(this, ACombatEnemySpawner::StaticClass(), SpawnerActors);
	for (AActor* Actor : SpawnerActors)
	{
		if (ACombatEnemySpawner* Spawner = Cast<ACombatEnemySpawner>(Actor))
		{
			TutorialSpawners.Add(Spawner);
			Spawner->OnEnemySpawned.AddUniqueDynamic(this, &ACombatGameMode::HandleEnemySpawned);
			if (ACombatEnemy* ActiveEnemy = Spawner->GetActiveEnemy())
			{
				ConfigureTutorialEnemy(ActiveEnemy);
			}
		}
	}

	TArray<AActor*> ExistingEnemies;
	UGameplayStatics::GetAllActorsOfClass(this, ACombatEnemy::StaticClass(), ExistingEnemies);
	for (AActor* Actor : ExistingEnemies)
	{
		ConfigureTutorialEnemy(Cast<ACombatEnemy>(Actor));
	}
}

void ACombatGameMode::HandleEnemySpawned(ACombatEnemy* Enemy)
{
	if (FTUEState != ECombatFTUEState::None && FTUEState != ECombatFTUEState::Complete)
	{
		ConfigureTutorialEnemy(Enemy);
	}
}

void ACombatGameMode::ConfigureTutorialEnemy(ACombatEnemy* Enemy)
{
	if (!IsValid(Enemy))
	{
		return;
	}

	Enemy->SetTutorialControlled(true);
	if (!IsValid(TutorialEnemy))
	{
		TutorialEnemy = Enemy;
		TutorialEnemy->OnDamageReceived.AddUniqueDynamic(this, &ACombatGameMode::HandleEnemyDamaged);
		TutorialEnemy->OnTutorialDodgeWindow.AddUniqueDynamic(this, &ACombatGameMode::HandleTutorialDodgeWindow);
		StartTutorialDodgeAttackIfReady();
	}
}

void ACombatGameMode::CleanupFTUE()
{
	GetWorldTimerManager().ClearTimer(FTUETransitionTimer);
	GetWorldTimerManager().ClearTimer(DodgeAttackStartTimer);
	GetWorldTimerManager().ClearTimer(DodgePromptTimer);
	bFTUETransitionPending = false;
	bWaitingForDodgeInput = false;
	bDodgeAttackStartDelayElapsed = false;
	bDodgePromptPending = false;
	PendingFTUEState = ECombatFTUEState::None;

	if (TutorialEnemy)
	{
		TutorialEnemy->OnDamageReceived.RemoveDynamic(this, &ACombatGameMode::HandleEnemyDamaged);
		TutorialEnemy->OnTutorialDodgeWindow.RemoveDynamic(this, &ACombatGameMode::HandleTutorialDodgeWindow);
	}

	for (ACombatEnemySpawner* Spawner : TutorialSpawners)
	{
		if (Spawner)
		{
			Spawner->OnEnemySpawned.RemoveDynamic(this, &ACombatGameMode::HandleEnemySpawned);
		}
	}
	TutorialSpawners.Reset();

	TArray<AActor*> Enemies;
	UGameplayStatics::GetAllActorsOfClass(this, ACombatEnemy::StaticClass(), Enemies);
	for (AActor* Actor : Enemies)
	{
		if (ACombatEnemy* Enemy = Cast<ACombatEnemy>(Actor))
		{
			Enemy->SetTutorialControlled(false);
		}
	}
	TutorialEnemy = nullptr;
}

void ACombatGameMode::HandleDodgeAttackStartDelayElapsed()
{
	bDodgeAttackStartDelayElapsed = true;
	StartTutorialDodgeAttackIfReady();
}

void ACombatGameMode::StartTutorialDodgeAttackIfReady()
{
	if (FTUEState == ECombatFTUEState::Dodge && bDodgeAttackStartDelayElapsed && TutorialEnemy && CombatPlayer)
	{
		TutorialEnemy->StartTutorialDodgeAttack(CombatPlayer);
	}
}

void ACombatGameMode::SetPlayerCombatPermissions(bool bLight, bool bHeavy, bool bDodge, bool bCancelDisallowed)
{
	if (CombatPlayer)
	{
		CombatPlayer->SetCombatInputPermissions(bLight, bHeavy, bDodge, bCancelDisallowed);
	}
}

void ACombatGameMode::HandlePlayerDied()
{
	if (bRunActive)
	{
		bRunActive = false;
		bGameOver = true;
		FinalSurvivalTime = SurvivalTime;
		SetPlayerCombatPermissions(false, false, false);
		const float DeathDuration = CombatPlayer ? CombatPlayer->GetDeathAnimationDuration() : 0.0f;
		if (DeathDuration > 0.0f)
		{
			GetWorldTimerManager().SetTimer(DeathPauseTimer, this, &ACombatGameMode::PauseAfterDeathAnimation,
				DeathDuration, false);
		}
		else PauseAfterDeathAnimation();
		return;
	}

	if (FTUEState != ECombatFTUEState::None && FTUEState != ECombatFTUEState::Complete)
	{
		FTUEState = ECombatFTUEState::None;
		CleanupFTUE();
		SetPlayerCombatPermissions(false, false, false);
		const float DeathDuration = CombatPlayer ? CombatPlayer->GetDeathAnimationDuration() : 0.0f;
		if (DeathDuration > 0.0f)
		{
			GetWorldTimerManager().SetTimer(TutorialDeathRestartTimer, this,
				&ACombatGameMode::RestartAfterIncompleteFTUE, DeathDuration, false);
		}
		else TutorialDeathRestartTimer = GetWorldTimerManager().SetTimerForNextTick(
			this, &ACombatGameMode::RestartAfterIncompleteFTUE);
	}
}

void ACombatGameMode::PauseAfterDeathAnimation()
{
	if (!bGameOver) return;
	UGameplayStatics::SetGamePaused(this, true);
	if (CombatPlayerController) CombatPlayerController->ShowGameOver(FinalSurvivalTime);
}

void ACombatGameMode::HandleRestartSelected()
{
	if (!bGameOver)
	{
		return;
	}

	bGameOver = false;
	GetWorldTimerManager().ClearTimer(DeathPauseTimer);
	UGameplayStatics::SetGamePaused(this, false);
	if (bFullFlowDemo)
	{
		UGameplayStatics::OpenLevel(this, TEXT("Level_Main_Menu"));
		return;
	}
	RestartLevel(true);
}

void ACombatGameMode::HandleQuitSelected()
{
	UKismetSystemLibrary::QuitGame(this, CombatPlayerController, EQuitPreference::Quit, false);
}

void ACombatGameMode::RestartAfterIncompleteFTUE()
{
	RestartLevel(false);
}

void ACombatGameMode::RestartLevel(bool bSkipStartupMenu)
{
	const FName LevelName(*UGameplayStatics::GetCurrentLevelName(this, true));
	const FString TravelOptions = bSkipStartupMenu ? TEXT("RestartRun=1") : TEXT("");
	UGameplayStatics::OpenLevel(this, LevelName, true, TravelOptions);
}

void ACombatGameMode::LoadFTUEProfile()
{
	FTUESaveGame = Cast<UCombatFTUESaveGame>(
		ULocalPlayerSaveGame::LoadOrCreateSaveGameForLocalPlayer(
			UCombatFTUESaveGame::StaticClass(), CombatPlayerController, FTUEProfileSlotName));

	if (!FTUESaveGame)
	{
		UE_LOG(LogGSGR, Warning, TEXT("FTUE profile could not be loaded; treating this player as new."));
	}
}

void ACombatGameMode::SaveFTUECompletion()
{
	if (!FTUESaveGame)
	{
		return;
	}

	FTUESaveGame->bHasCompletedFTUE = true;
	if (!FTUESaveGame->SaveGameToSlotForLocalPlayer())
	{
		UE_LOG(LogGSGR, Error, TEXT("Failed to request FTUE profile save."));
	}
}

void ACombatGameMode::SetupDemoSpawners()
{
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(this, ACombatEnemySpawner::StaticClass(), Actors);
	for (AActor* Actor : Actors)
	{
		ACombatEnemySpawner* Spawner = Cast<ACombatEnemySpawner>(Actor);
		if (!Spawner) continue;
		Spawner->SetDemoManaged(true);
		Spawner->ClearDemoEnemy();
		if (bFullFlowDemo && Spawner->HasDemoClasses()
			&& (!DemoSpawner || (Spawner->IsImmediateSpawner() && !DemoSpawner->IsImmediateSpawner())))
		{
			DemoSpawner = Spawner;
		}
	}
	if (bFullFlowDemo && !DemoSpawner)
	{
		UE_LOG(LogGSGR, Error, TEXT("Full Flow needs a combat spawner with basic and flurry classes."));
	}
}

void ACombatGameMode::SpawnTutorialDemoEnemy(bool bFlurry)
{
	if (!DemoSpawner) return;
	DemoEnemy = DemoSpawner->SpawnDemoEnemy(bFlurry);
	if (DemoEnemy)
	{
		// Place lesson targets within the player's melee trace and the enemy's
		// scripted attack reach so each lesson can resolve from the fixed anchor.
		if (CombatPlayer)
		{
			const FVector TutorialLocation = CombatPlayer->GetActorLocation()
				+ CombatPlayer->GetActorForwardVector() * 160.0f;
			DemoEnemy->SetActorLocation(TutorialLocation, false, nullptr, ETeleportType::TeleportPhysics);
		}
		ConfigureTutorialEnemy(DemoEnemy);
	}
}

void ACombatGameMode::ClearDemoEnemy()
{
	if (TutorialEnemy)
	{
		TutorialEnemy->OnDamageReceived.RemoveDynamic(this, &ACombatGameMode::HandleEnemyDamaged);
		TutorialEnemy->OnTutorialDodgeWindow.RemoveDynamic(this, &ACombatGameMode::HandleTutorialDodgeWindow);
		TutorialEnemy = nullptr;
	}
	if (DemoEnemy) DemoEnemy->OnEnemyDied.RemoveDynamic(this, &ACombatGameMode::HandleDemoEnemyDied);
	if (DemoSpawner) DemoSpawner->ClearDemoEnemy();
	TArray<AActor*> RemainingEnemies;
	UGameplayStatics::GetAllActorsOfClass(this, ACombatEnemy::StaticClass(), RemainingEnemies);
	for (AActor* Actor : RemainingEnemies)
	{
		if (IsValid(Actor)) Actor->Destroy();
	}
	DemoEnemy = nullptr;
}

void ACombatGameMode::ResetDemoTimers()
{
	GetWorldTimerManager().ClearTimer(DemoEventTimer);
	GetWorldTimerManager().ClearTimer(DodgeResolveTimer);
	if (ReminderTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReminderTickerHandle);
		ReminderTickerHandle.Reset();
	}
}

void ACombatGameMode::HandleTutorialPlayerHit(ACombatEnemy* Enemy, bool bDodgeProtected, bool bEvadeProtected)
{
	if (!bFullFlowDemo || Enemy != TutorialEnemy || bDemoTransitionPending) return;
	if (FTUEState == ECombatFTUEState::Dodge && !bDodgeProtected)
	{
		RetryCurrentLesson();
	}
	else if (FTUEState == ECombatFTUEState::Evade && Enemy->GetFlurryState() == ECombatFlurryState::Flurry)
	{
		if (!bEvadeProtected)
		{
			RetryCurrentLesson();
		}
		else
		{
			bDemoTransitionPending = true;
			SetPlayerCombatPermissions(false, false, false);
			CombatPlayerController->ShowTutorialMessage(
				NSLOCTEXT("CombatFTUE", "EvadeSuccessHeading", "Good Job"),
				NSLOCTEXT("CombatFTUE", "EvadeSuccessMessage", "You evaded the flurry hit."));
			ScheduleFTUETransition(ECombatFTUEState::Complete, EvadeSuccessFeedbackDuration);
		}
	}
}

void ACombatGameMode::ResolveDodgeAttempt()
{
	if (FTUEState != ECombatFTUEState::Dodge || !bWaitingForDodgeInput) return;
	if (TutorialEnemy) TutorialEnemy->ResolveTutorialDodgeAttack();
	RetryCurrentLesson();
}

void ACombatGameMode::RetryCurrentLesson()
{
	if (!bFullFlowDemo || bDemoTransitionPending
		|| (FTUEState != ECombatFTUEState::Dodge && FTUEState != ECombatFTUEState::Evade)) return;
	bDemoTransitionPending = true;
	GetWorldTimerManager().ClearTimer(FTUETransitionTimer);
	GetWorldTimerManager().ClearTimer(DodgeResolveTimer);
	GetWorldTimerManager().ClearTimer(DodgePromptTimer);
	GetWorldTimerManager().ClearTimer(DodgeAttackStartTimer);
	if (TutorialEnemy) TutorialEnemy->AbortTutorialAction();
	SetPlayerCombatPermissions(false, false, false);
	CombatPlayerController->ShowTutorialMessage(
		NSLOCTEXT("CombatFTUE", "RetryHeading", "Try Again"),
		NSLOCTEXT("CombatFTUE", "RetryPrompt", "Watch the attack and try the same lesson again."));
	GetWorldTimerManager().SetTimer(DemoEventTimer, this, &ACombatGameMode::PerformLessonRetry, 0.5f, false);
}

void ACombatGameMode::PerformLessonRetry()
{
	const ECombatFTUEState Lesson = FTUEState;
	ClearDemoEnemy();
	bDemoTransitionPending = false;
	EnterFTUEState(Lesson);
}

void ACombatGameMode::HandleDemoSkip()
{
	if (!bFullFlowDemo || bReminderOpen) return;
	if (FullFlowStage == EFullFlowStage::BasicEncounter)
	{
		DepartForShowcase();
		return;
	}
	if (FullFlowStage != EFullFlowStage::Tutorial) return;
	ECombatFTUEState Next = ECombatFTUEState::None;
	switch (FTUEState)
	{
	case ECombatFTUEState::LightAttack: Next = ECombatFTUEState::HeavyAttack; break;
	case ECombatFTUEState::HeavyAttack: Next = ECombatFTUEState::Dodge; break;
	case ECombatFTUEState::Dodge: Next = ECombatFTUEState::Evade; break;
	case ECombatFTUEState::Evade: Next = ECombatFTUEState::Complete; break;
	default: return;
	}
	bDemoTransitionPending = false;
	EnterFTUEState(Next);
}

void ACombatGameMode::JumpToShowcase()
{
	if (bFullFlowDemo) DepartForShowcase();
}

void ACombatGameMode::DepartForShowcase()
{
	ResetDemoTimers();
	CloseEvadeReminder();
	GetWorldTimerManager().ClearTimer(FTUETransitionTimer);
	GetWorldTimerManager().ClearTimer(DodgeAttackStartTimer);
	GetWorldTimerManager().ClearTimer(DodgePromptTimer);
	ClearDemoEnemy();
	CleanupFTUE();
	FullFlowStage = EFullFlowStage::Showcase;
	bRunActive = false;
	FTUEState = ECombatFTUEState::None;
	SetPlayerCombatPermissions(false, false, false);
	OnShowcaseDeparted.Broadcast();
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, TEXT("Level_Control_Recap_Showcase"));
}

void ACombatGameMode::HandleShowcaseReturn()
{
	if (!bShowcaseLevel) return;
	UGameplayStatics::SetGamePaused(this, false);
	UGameplayStatics::OpenLevel(this, TEXT("Level_Full_Flow"), true, TEXT("FromShowcase=1"));
}

void ACombatGameMode::JumpToPostShowcase()
{
	if (!bFullFlowDemo) return;
	ResetDemoTimers();
	CloseEvadeReminder();
	GetWorldTimerManager().ClearTimer(FTUETransitionTimer);
	GetWorldTimerManager().ClearTimer(DodgeAttackStartTimer);
	GetWorldTimerManager().ClearTimer(DodgePromptTimer);
	ClearDemoEnemy();
	CleanupFTUE();
	FTUEState = ECombatFTUEState::None;
	StartPostShowcaseFlurry();
}

void ACombatGameMode::StartBasicEncounter()
{
	FullFlowStage = EFullFlowStage::BasicEncounter;
	if (!DemoSpawner) return;
	DemoEnemy = DemoSpawner->SpawnDemoEnemy(false);
	if (DemoEnemy) DemoEnemy->OnEnemyDied.AddUniqueDynamic(this, &ACombatGameMode::HandleDemoEnemyDied);
}

void ACombatGameMode::StartPostShowcaseFlurry()
{
	FullFlowStage = EFullFlowStage::PostShowcaseFlurry;
	bReminderOpen = false;
	if (CombatPlayer) CombatPlayer->ResetHP();
	StartNormalGameplay();
	if (!DemoSpawner) return;
	DemoEnemy = DemoSpawner->SpawnDemoEnemy(true);
	if (DemoEnemy) DemoEnemy->OnEnemyDied.AddUniqueDynamic(this, &ACombatGameMode::HandleDemoEnemyDied);
}

void ACombatGameMode::HandleDemoEnemyDied()
{
	if (!bFullFlowDemo) return;
	if (DemoEnemy) DemoEnemy->OnEnemyDied.RemoveDynamic(this, &ACombatGameMode::HandleDemoEnemyDied);
	DemoEnemy = nullptr;
	if (FullFlowStage == EFullFlowStage::BasicEncounter)
	{
		if (BasicEncounterToShowcaseDelay <= 0.0f)
			DemoEventTimer = GetWorldTimerManager().SetTimerForNextTick(this, &ACombatGameMode::DepartForShowcase);
		else GetWorldTimerManager().SetTimer(DemoEventTimer, this, &ACombatGameMode::DepartForShowcase,
			BasicEncounterToShowcaseDelay, false);
	}
	else if (FullFlowStage == EFullFlowStage::PostShowcaseFlurry || FullFlowStage == EFullFlowStage::Mixed)
	{
		FullFlowStage = EFullFlowStage::Mixed;
		GetWorldTimerManager().SetTimer(DemoEventTimer, this, &ACombatGameMode::SpawnMixedEnemy, 2.0f, false);
	}
}

void ACombatGameMode::SpawnMixedEnemy()
{
	if (FullFlowStage != EFullFlowStage::Mixed || !IsRunActive() || !DemoSpawner) return;
	DemoEnemy = DemoSpawner->SpawnDemoEnemy(FMath::RandBool());
	if (DemoEnemy) DemoEnemy->OnEnemyDied.AddUniqueDynamic(this, &ACombatGameMode::HandleDemoEnemyDied);
}

void ACombatGameMode::ShowEvadeReminder()
{
	if (bReminderShown || !CombatPlayerController) return;
	bReminderShown = true;
	bReminderOpen = true;
	UGameplayStatics::SetGamePaused(this, true);
	CombatPlayerController->ShowEvadeReminder();
	ReminderTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this,
		[this](float) { FinishEvadeReminder(); return false; }), 3.0f);
}

void ACombatGameMode::FinishEvadeReminder()
{
	ReminderTickerHandle.Reset();
	RequestCloseEvadeReminder();
}

void ACombatGameMode::RequestCloseEvadeReminder()
{
	if (!bReminderOpen) return;
	if (ReminderTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReminderTickerHandle);
		ReminderTickerHandle.Reset();
	}
	if (CombatPlayerController) CombatPlayerController->BeginHideEvadeReminder();
}

void ACombatGameMode::CloseEvadeReminder()
{
	if (!bReminderOpen) return;
	bReminderOpen = false;
	if (ReminderTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReminderTickerHandle);
		ReminderTickerHandle.Reset();
	}
	if (CombatPlayerController) CombatPlayerController->HideEvadeReminder();
	UGameplayStatics::SetGamePaused(this, false);
}

AActor* ACombatGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const FName PlayerTag(*FString::Printf(TEXT("Player%d"), CurrentPlayerStartAssignment));
	TArray<AActor*> PlayerStarts;
	UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), APlayerStart::StaticClass(), PlayerTag, PlayerStarts);
	++CurrentPlayerStartAssignment;

	if (PlayerStarts.IsEmpty())
	{
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);
	}

	return PlayerStarts.IsEmpty() ? nullptr : PlayerStarts[FMath::RandRange(0, PlayerStarts.Num() - 1)];
}
