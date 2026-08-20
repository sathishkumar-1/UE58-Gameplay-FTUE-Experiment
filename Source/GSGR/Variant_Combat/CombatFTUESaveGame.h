// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "CombatFTUESaveGame.generated.h"

/** Minimal per-local-player profile data for the first-time tutorial. */
UCLASS()
class UCombatFTUESaveGame : public ULocalPlayerSaveGame
{
	GENERATED_BODY()

public:

	/** Written only after every FTUE state has completed successfully. */
	UPROPERTY(SaveGame)
	bool bHasCompletedFTUE = false;

	virtual void ResetToDefault() override;
	virtual int32 GetLatestDataVersion() const override { return 1; }
};
