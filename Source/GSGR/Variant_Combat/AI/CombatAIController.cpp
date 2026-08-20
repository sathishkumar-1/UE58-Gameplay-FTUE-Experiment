// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatAIController.h"
#include "Components/StateTreeAIComponent.h"
#include "CombatEnemy.h"

ACombatAIController::ACombatAIController()
{
	// create the StateTree AI Component
	StateTreeAI = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTreeAI"));
	check(StateTreeAI);

	// ensure we start the StateTree when we possess the pawn
	bStartAILogicOnPossess = false;
	StateTreeAI->SetStartLogicAutomatically(false);

	// ensure we're attached to the possessed character.
	// this is necessary for EnvQueries to work correctly
	bAttachToPawn = true;
}

void ACombatAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// Stationary arena enemies use the intentionally small approach/attack loop
	// implemented by ACombatEnemy instead of the roaming/flanking StateTree.
	if (const ACombatEnemy* Enemy = Cast<ACombatEnemy>(InPawn))
	{
		if (Enemy->UsesStationaryArenaAI())
		{
			return;
		}
	}

	// start AI logic
	StateTreeAI->StartLogic();
}
