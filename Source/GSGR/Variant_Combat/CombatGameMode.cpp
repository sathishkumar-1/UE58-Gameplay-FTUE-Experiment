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

	// Player zero is created by normal game initialization.
	for (int32 i = 2; i <= NumberOfLocalPlayers; ++i)
	{
		UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
	}

	InitializeRunFlow();
}

void ACombatGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(FTUETransitionTimer);
	GetWorldTimerManager().ClearTimer(DodgeAttackStartTimer);
	GetWorldTimerManager().ClearTimer(DodgePromptTimer);
	GetWorldTimerManager().ClearTimer(TutorialCompleteUITimer);
	GetWorldTimerManager().ClearTimer(TutorialDeathRestartTimer);
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
	if (bRunActive || bGameOver || FTUEState != ECombatFTUEState::None)
	{
		return;
	}

	UGameplayStatics::SetGamePaused(this, false);
	CombatPlayerController->HideRunFlowUI();
	if (!FTUESaveGame || !FTUESaveGame->bHasCompletedFTUE)
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
	bRunActive = false;
	bGameOver = false;
	SurvivalTime = 0.0f;
	FinalSurvivalTime = 0.0f;
	BindTutorialActors();
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
	SetPlayerCombatPermissions(false, false, false, false);
	if (TutorialEnemy)
	{
		TutorialEnemy->ResolveTutorialDodgeAttack();
	}

	CombatPlayerController->ShowTutorialMessage(
		NSLOCTEXT("CombatFTUE", "DodgeSuccessHeading", "Good Job"),
		NSLOCTEXT("CombatFTUE", "DodgeSuccessMessage", "Attack avoided."));
	ScheduleFTUETransition(ECombatFTUEState::Complete, DodgeSuccessFeedbackDuration);
}

void ACombatGameMode::CompleteFTUE()
{
	SaveFTUECompletion();
	CleanupFTUE();
	StartNormalGameplay(true);

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
		UGameplayStatics::SetGamePaused(this, true);
		CombatPlayerController->ShowGameOver(FinalSurvivalTime);
		return;
	}

	if (FTUEState != ECombatFTUEState::None && FTUEState != ECombatFTUEState::Complete)
	{
		FTUEState = ECombatFTUEState::None;
		CleanupFTUE();
		SetPlayerCombatPermissions(false, false, false);
		GetWorldTimerManager().SetTimerForNextTick(this, &ACombatGameMode::RestartAfterIncompleteFTUE);
	}
}

void ACombatGameMode::HandleRestartSelected()
{
	if (!bGameOver)
	{
		return;
	}

	bGameOver = false;
	UGameplayStatics::SetGamePaused(this, false);
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
