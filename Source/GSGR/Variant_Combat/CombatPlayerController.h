// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CombatPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ACombatCharacter;
class UCombatRunWidget;
struct FInputKeyEventArgs;

/**
 *  Simple Player Controller for a third person combat game
 *  Manages input mappings
 *  Respawns the player character at the checkpoint when it's destroyed
 */
UCLASS(abstract, Config="Game")
class ACombatPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input mapping context for this player */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Character class to respawn when the possessed pawn is destroyed */
	UPROPERTY(EditAnywhere, Category="Respawn")
	TSubclassOf<ACombatCharacter> CharacterClass;

	/** Transform to respawn the character at. Can be set to create checkpoints */
	FTransform RespawnTransform;

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Initialize input bindings */
	virtual void SetupInputComponent() override;

	/** Captures one deliberate keyboard/controller press-release pair for Welcome. */
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;

	/** Pawn initialization */
	virtual void OnPossess(APawn* InPawn) override;

public:

	/** Updates the character respawn transform */
	void SetRespawnTransform(const FTransform& NewRespawn);

	/** Run-flow presentation used by the existing CombatGameMode. */
	void ShowStartupMenu();
	void ShowTutorialMessage(const FText& Heading, const FText& Message);
	void ShowTutorialComplete();
	void ShowGameOver(float FinalSurvivalTime);
	void HideRunFlowUI();
	void SetAwaitingWelcomeInput(bool bAwaiting);
	FText GetBindingDisplayText(const UInputAction* Action) const;

	/** Widget callbacks routed to the authoritative run flow. */
	void HandlePlaySelected();
	void HandleRestartSelected();
	void HandleQuitSelected();

protected:

	/** Called if the possessed pawn is destroyed */
	UFUNCTION()
	void OnPawnDestroyed(AActor* DestroyedActor);

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Lazily creates the shared startup/tutorial/game-over overlay. */
	void EnsureRunFlowWidget();

	/** Switches between menu navigation and normal game input. */
	void SetMenuInputMode(bool bMenuActive);

	UPROPERTY(Transient)
	TObjectPtr<UCombatRunWidget> RunFlowWidget;

	bool bAwaitingWelcomeInput = false;
	bool bWelcomeKeyPressed = false;
	FKey WelcomeContinuationKey;

};
