// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatAttacker.h"
#include "CombatDamageable.h"
#include "Animation/AnimMontage.h"
#include "Engine/TimerHandle.h"
#include "CombatEnemy.generated.h"

class UWidgetComponent;
class UCombatLifeBar;
class UAnimMontage;
class ACombatCharacter;
class ACombatEnemy;

/** Completed attack animation delegate for StateTree */
DECLARE_DELEGATE(FOnEnemyAttackCompleted);

/** Landed delegate for StateTree */
DECLARE_DELEGATE(FOnEnemyLanded);

/** Enemy died delegate */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnEnemyDied);

/** Confirms that the existing enemy damage path accepted a real hit. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCombatEnemyDamaged, ACombatEnemy*, Enemy, AActor*, DamageCauser);

/** Fired from the normal attack trace notify at the FTUE dodge timing. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTutorialDodgeWindow, ACombatEnemy*, Enemy);

/**
 *  An AI-controlled character with combat capabilities.
 *  Its bundled AI Controller runs logic through StateTree
 */
UCLASS(abstract)
class ACombatEnemy : public ACharacter, public ICombatAttacker, public ICombatDamageable
{
	GENERATED_BODY()

	/** Life bar widget component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* LifeBar;

public:
	
	/** Constructor */
	ACombatEnemy();

	/** Drives the lightweight stationary-arena approach and attack behavior. */
	virtual void Tick(float DeltaSeconds) override;

	/** Used by the AI controller to decide whether to start the general StateTree. */
	bool UsesStationaryArenaAI() const { return bUseStationaryArenaAI; }

	/** Returns whether this enemy can still fight. */
	bool IsAlive() const { return CurrentHP > 0.0f; }

	/** Returns whether run flow currently owns this enemy for the FTUE. */
	bool IsTutorialControlled() const { return bTutorialControlled; }

protected:

	/** Uses a simple one-on-one arena brain: approach on the X axis, stop, and attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stationary Arena")
	bool bUseStationaryArenaAI = true;

	/** Center-to-center distance at which the enemy stops and starts attacking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stationary Arena")
	float ArenaAttackRange = 140.0f;

	/** Recovery pause after an arena attack completes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stationary Arena")
	float ArenaAttackCooldown = 2.5f;

	/** Readable pause after reaching attack range and before committing to a strike. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stationary Arena")
	float ArenaAttackWindupDelay = 0.9f;

	/** Small random variation applied to wait/recovery pauses. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stationary Arena")
	float ArenaAttackDelayVariation = 0.35f;

	enum class EStationaryArenaState : uint8
	{
		Approach,
		Wait,
		Attack,
		Recovery
	};

	/** Current phase of the deliberately simple arena combat loop. */
	EStationaryArenaState ArenaState = EStationaryArenaState::Approach;

	/** Game time at which the current wait/recovery phase completes. */
	float ArenaStateEndTime = 0.0f;

	/** Small orchestration layer used only while this actor is the FTUE enemy. */
	bool bTutorialControlled = false;
	bool bTutorialAttackRequested = false;
	bool bTutorialDodgeWindowConsumed = false;
	bool bTutorialAttackFrozen = false;
	float TutorialPreviousAnimRateScale = 1.0f;
	FName TutorialDamageSourceBone = NAME_None;
	TWeakObjectPtr<ACombatCharacter> TutorialTarget;

	/** Max amount of HP the character will have on respawn */
	UPROPERTY(EditAnywhere, Category="Damage")
	float MaxHP = 3.0f;

public:

	/** Current amount of HP the character has */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Damage", meta = (ClampMin = 0, ClampMax = 100))
	float CurrentHP = 0.0f;

protected:

	/** Name of the pelvis bone, for damage ragdoll physics */
	UPROPERTY(EditAnywhere, Category="Damage")
	FName PelvisBoneName;

	/** Pointer to the life bar widget */
	UPROPERTY(EditAnywhere, Category="Damage")
	UCombatLifeBar* LifeBarWidget;

	/** If true, the character is currently playing an attack animation */
	bool bIsAttacking = false;

	/** Distance ahead of the character that melee attack sphere collision traces will extend */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float MeleeTraceDistance = 75.0f;

	/** Radius of the sphere trace for melee attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units = "cm"))
	float MeleeTraceRadius = 50.0f;

	/** Amount of damage a melee attack will deal */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MeleeDamage = 1.0f;

	/** Amount of knockback impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeKnockbackImpulse = 150.0f;

	/** Amount of upwards impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeLaunchImpulse = 350.0f;

	/** AnimMontage that will play for combo attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	UAnimMontage* ComboAttackMontage;

	/** Names of the AnimMontage sections that correspond to each stage of the combo attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	TArray<FName> ComboSectionNames;

	/** Target number of attacks in the combo attack string we're playing */
	int32 TargetComboCount = 0;

	/** Index of the current stage of the melee attack combo */
	int32 CurrentComboAttack = 0;

	/** AnimMontage that will play for charged attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	UAnimMontage* ChargedAttackMontage;

	/** Name of the AnimMontage section that corresponds to the charge loop */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	FName ChargeLoopSection;

	/** Name of the AnimMontage section that corresponds to the attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	FName ChargeAttackSection;

	/** Minimum number of charge animation loops that will be played by the AI */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged", meta = (ClampMin = 1, ClampMax = 20))
	int32 MinChargeLoops = 2;

	/** Maximum number of charge animation loops that will be played by the AI */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged", meta = (ClampMin = 1, ClampMax = 20))
	int32 MaxChargeLoops = 5;

	/** Target number of charge animation loops to play in this charged attack */
	int32 TargetChargeLoops = 0;

	/** Number of charge animation loop currently playing */
	int32 CurrentChargeLoop = 0;

	/** Time to wait before removing this character from the level after it dies */
	UPROPERTY(EditAnywhere, Category="Death")
	float DeathRemovalTime = 5.0f;

	/** Enemy death timer */
	FTimerHandle DeathTimer;

	/** Attack montage ended delegate */
	FOnMontageEnded OnAttackMontageEnded;

	/** Last recorded location we're being attacked from */
	FVector LastDangerLocation = FVector::ZeroVector;

	/** Last recorded game time we were attacked */
	float LastDangerTime = -1000.0f;

public:
	/** Attack completed internal delegate to notify StateTree tasks */
	FOnEnemyAttackCompleted OnAttackCompleted;

	/** Landed internal delegate to notify StateTree tasks. We use this instead of the built-in Landed delegate so we can bind to a Lambda in StateTree tasks */
	FOnEnemyLanded OnEnemyLanded;

	/** Enemy died delegate. Allows external subscribers to respond to enemy death */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnEnemyDied OnEnemyDied;

	/** Normal damage confirmation used by the FTUE state machine. */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnCombatEnemyDamaged OnDamageReceived;

	/** Normal attack notify reached the point at which the dodge should be taught. */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnTutorialDodgeWindow OnTutorialDodgeWindow;

public:

	/** Performs an AI-initiated combo attack. Number of hits will be decided by this character */
	void DoAIComboAttack();

	/** Performs an AI-initiated charged attack. Charge time will be decided by this character */
	void DoAIChargedAttack();

	/** Called from a delegate when the attack montage ends */
	void AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Cancels current attacks so blend-out notifies cannot deal late damage. */
	void CancelAttacks();

	/** Enters the deliberately long post-attack pause. */
	void EnterArenaRecovery();

	/** Suppresses autonomous decisions and protects the tutorial target from accidental death. */
	void SetTutorialControlled(bool bControlled);

	/** Approaches as needed, then starts the existing normal combo attack. */
	void StartTutorialDodgeAttack(ACombatCharacter* TargetPlayer);

	/** Resumes the paused montage and resolves its stored normal attack trace. */
	void ResolveTutorialDodgeAttack();

	/** Restores animation-only skeletal control after a nonlethal hit. */
	void RestoreAnimationAfterHit();

	/** Returns the last recorded location we were attacked from */
	const FVector& GetLastDangerLocation() const;

	/** Returns the last game time we were attacked */
	float GetLastDangerTime() const;

public:

	// ~begin ICombatAttacker interface

	/** Performs an attack's collision check */
	virtual void DoAttackTrace(FName DamageSourceBone) override;

	/** Performs a combo attack's check to continue the string */
	UFUNCTION(BlueprintCallable, Category="Attacker")
	virtual void CheckCombo() override;

	/** Performs a charged attack's check to loop the charge animation */
	UFUNCTION(BlueprintCallable, Category="Attacker")
	virtual void CheckChargedAttack() override;

	// ~end ICombatAttacker interface

	// ~begin ICombatDamageable interface

	/** Handles damage and knockback events */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Handles death events */
	virtual void HandleDeath() override;

	/** Handles healing events */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	/** Allows the enemy to react to incoming attacks */
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	// ~end ICombatDamageable interface

protected:

	/** Drives only the explicitly requested FTUE attack while normal AI is suppressed. */
	void TickTutorialControl();

	/** Pauses only this enemy's visual attack, leaving player input and the world responsive. */
	void SetTutorialAttackFrozen(bool bFrozen);

	/** Removes this character from the level after it dies */
	void RemoveFromLevel();

public:

	/** Overrides the default TakeDamage functionality */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Overrides landing to reset damage ragdoll physics */
	virtual void Landed(const FHitResult& Hit) override;

protected:

	/** Blueprint handler to play damage received effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void ReceivedDamage(float Damage, const FVector& ImpactPoint, const FVector& DamageDirection);

protected:

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** EndPlay cleanup */
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
};
