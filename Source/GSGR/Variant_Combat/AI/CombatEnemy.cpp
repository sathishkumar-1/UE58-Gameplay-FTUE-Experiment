// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatEnemy.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CombatAIController.h"
#include "Components/WidgetComponent.h"
#include "Engine/DamageEvents.h"
#include "CombatLifeBar.h"
#include "TimerManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "CombatCharacter.h"

namespace
{
	// Kept outside the UObject layout so this recovery state remains safe for
	// Live Coding while the editor is open.
	TMap<const ACombatEnemy*, FTransform> EnemyMeshStartingTransforms;
}

ACombatEnemy::ACombatEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// bind the attack montage ended delegate
	OnAttackMontageEnded.BindUObject(this, &ACombatEnemy::AttackMontageEnded);

	// set the AI Controller class by default
	AIControllerClass = ACombatAIController::StaticClass();

	// use an AI Controller regardless of whether we're placed or spawned
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// ignore the controller's yaw rotation
	bUseControllerRotationYaw = false;

	// create the life bar
	LifeBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("LifeBar"));
	LifeBar->SetupAttachment(RootComponent);

	// set the collision capsule size
	GetCapsuleComponent()->SetCapsuleSize(35.0f, 90.0f);

	// set the character movement properties
	GetCharacterMovement()->bUseControllerDesiredRotation = true;

	// reset HP to maximum
	CurrentHP = MaxHP;
}

void ACombatEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bTutorialControlled)
	{
		TickTutorialControl();
		return;
	}

	if (!bUseStationaryArenaAI || !IsAlive())
	{
		return;
	}

	ACombatCharacter* Player = Cast<ACombatCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	if (!IsValid(Player) || !Player->IsAlive())
	{
		GetCharacterMovement()->StopMovementImmediately();
		return;
	}

	const float DeltaX = Player->GetActorLocation().X - GetActorLocation().X;
	const float DistanceToPlayer = FMath::Abs(DeltaX);
	const float FacingYaw = DeltaX >= 0.0f ? 0.0f : 180.0f;
	SetActorRotation(FRotator(0.0f, FacingYaw, 0.0f));

	const float CurrentTime = GetWorld()->GetTimeSeconds();

	switch (ArenaState)
	{
	case EStationaryArenaState::Approach:
		if (DistanceToPlayer > ArenaAttackRange)
		{
			AddMovementInput(FVector(DeltaX >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f));
			return;
		}

		GetCharacterMovement()->StopMovementImmediately();
		ArenaState = EStationaryArenaState::Wait;
		ArenaStateEndTime = CurrentTime + ArenaAttackWindupDelay
			+ FMath::FRandRange(0.0f, ArenaAttackDelayVariation);
		return;

	case EStationaryArenaState::Wait:
		GetCharacterMovement()->StopMovementImmediately();

		// If the player created meaningful space, visibly recommit to approaching
		// instead of attacking empty air with perfect responsiveness.
		if (DistanceToPlayer > ArenaAttackRange * 1.25f)
		{
			ArenaState = EStationaryArenaState::Approach;
			return;
		}

		if (CurrentTime >= ArenaStateEndTime)
		{
			DoAIComboAttack();
			if (bIsAttacking)
			{
				ArenaState = EStationaryArenaState::Attack;
			}
			else
			{
				EnterArenaRecovery();
			}
		}
		return;

	case EStationaryArenaState::Attack:
		GetCharacterMovement()->StopMovementImmediately();
		if (!bIsAttacking)
		{
			EnterArenaRecovery();
		}
		return;

	case EStationaryArenaState::Recovery:
		GetCharacterMovement()->StopMovementImmediately();
		if (CurrentTime >= ArenaStateEndTime)
		{
			bTutorialDodgeWindowConsumed = false;
			ArenaState = EStationaryArenaState::Approach;
		}
		return;
	}
}

void ACombatEnemy::DoAIComboAttack()
{
	// ignore if we're already playing an attack animation
	if (!IsAlive() || bIsAttacking || !ComboAttackMontage || ComboSectionNames.Num() < 2)
	{
		return;
	}

	// raise the attacking flag
	bIsAttacking = true;

	// Stationary arena opponents deliberately use one readable light strike.
	// The existing multi-hit behavior remains available to non-arena AI.
	TargetComboCount = bUseStationaryArenaAI ? 1 : FMath::RandRange(1, ComboSectionNames.Num() - 1);

	// reset the attack counter
	CurrentComboAttack = 0;

	// play the attack montage
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(ComboAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

		// subscribe to montage completed and interrupted events
		if (MontageLength > 0.0f)
		{
			// set the end delegate for the montage
			AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ComboAttackMontage);
		}
		else
		{
			bIsAttacking = false;
		}
	}
}

void ACombatEnemy::DoAIChargedAttack()
{
	// ignore if we're already playing an attack animation
	if (!IsAlive() || bIsAttacking || !ChargedAttackMontage)
	{
		return;
	}

	// raise the attacking flag
	bIsAttacking = true;

	// choose how many loops are we going to charge for
	TargetChargeLoops = FMath::RandRange(MinChargeLoops, MaxChargeLoops);

	// reset the charge loop counter
	CurrentChargeLoop = 0;

	// play the attack montage
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		const float MontageLength = AnimInstance->Montage_Play(ChargedAttackMontage, 1.0f, EMontagePlayReturnType::MontageLength, 0.0f, true);

		// subscribe to montage completed and interrupted events
		if (MontageLength > 0.0f)
		{
			// set the end delegate for the montage
			AnimInstance->Montage_SetEndDelegate(OnAttackMontageEnded, ChargedAttackMontage);
		}
		else
		{
			bIsAttacking = false;
		}
	}
}

void ACombatEnemy::AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// reset the attacking flag
	bIsAttacking = false;

	// call the attack completed delegate so the StateTree can continue execution
	OnAttackCompleted.ExecuteIfBound();

	if (bUseStationaryArenaAI && IsAlive() && ArenaState != EStationaryArenaState::Recovery)
	{
		EnterArenaRecovery();
	}
}

const FVector& ACombatEnemy::GetLastDangerLocation() const
{
	return LastDangerLocation;
}

float ACombatEnemy::GetLastDangerTime() const
{
	return LastDangerTime;
}

void ACombatEnemy::DoAttackTrace(FName DamageSourceBone)
{
	if (!IsAlive() || !bIsAttacking)
	{
		return;
	}

	if (bTutorialControlled && bTutorialAttackRequested && !bTutorialDodgeWindowConsumed)
	{
		TutorialDamageSourceBone = DamageSourceBone;
		bTutorialDodgeWindowConsumed = true;
		SetTutorialAttackFrozen(true);
		OnTutorialDodgeWindow.Broadcast(this);
		return;
	}

	// Tutorial enemies may trace only the one strike explicitly requested by the
	// FTUE. This also suppresses blend-out notifies while ownership is acquired or
	// released, when no tutorial attack is pending.
	if (bTutorialDodgeWindowConsumed && !bTutorialAttackRequested)
	{
		return;
	}
	const bool bResolvingTutorialDodgeAttack = bTutorialControlled
		&& bTutorialAttackRequested && bTutorialDodgeWindowConsumed;

	// sweep for objects in front of the character to be hit by the attack
	TArray<FHitResult> OutHits;

	// start at the provided socket location, sweep forward
	const FVector TraceStart = GetMesh()->GetSocketLocation(DamageSourceBone);
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * MeleeTraceDistance);

	// enemies only affect Pawn collision objects; they don't knock back boxes
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	// use a sphere shape for the sweep
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(MeleeTraceRadius);

	// ignore self
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		TSet<AActor*> DamagedActors;

		// iterate over each object hit
		for (const FHitResult& CurrentHit : OutHits)
		{
			AActor* HitActor = CurrentHit.GetActor();
			if (!IsValid(HitActor) || DamagedActors.Contains(HitActor))
			{
				continue;
			}
			DamagedActors.Add(HitActor);

			/** does the actor have the player tag? */
			if (HitActor->ActorHasTag(FName("Player")))
			{
				// check if the actor is damageable
				ICombatDamageable* Damageable = Cast<ICombatDamageable>(HitActor);

				if (Damageable)
				{
					// The FTUE has already accepted the player's dodge at this point. Keep
					// the real sweep and montage timing, but never damage its staged target.
					if (bResolvingTutorialDodgeAttack && HitActor == TutorialTarget.Get())
					{
						continue;
					}

					// knock upwards and away from the impact normal
					const FVector Impulse = (CurrentHit.ImpactNormal * -MeleeKnockbackImpulse) + (FVector::UpVector * MeleeLaunchImpulse);

					// pass the damage event to the actor
					Damageable->ApplyDamage(MeleeDamage, this, CurrentHit.ImpactPoint, Impulse);

				}
			}
		}
	}
}

void ACombatEnemy::CheckCombo()
{
	// increase the combo counter
	++CurrentComboAttack;

	// do we still have attacks to play in this string?
	if (CurrentComboAttack < TargetComboCount)
	{
		// jump to the next attack section
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->Montage_JumpToSection(ComboSectionNames[CurrentComboAttack], ComboAttackMontage);
		}
	}
}

void ACombatEnemy::CheckChargedAttack()
{
	// increase the charge loop counter
	++CurrentChargeLoop;

	// jump to either the loop or attack section of the montage depending on whether we hit the loop target
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_JumpToSection(CurrentChargeLoop >= TargetChargeLoops ? ChargeAttackSection : ChargeLoopSection, ChargedAttackMontage);
	}
}

void ACombatEnemy::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	
	// pass the damage event to the actor
	FDamageEvent DamageEvent;
	const float ActualDamage = TakeDamage(Damage, DamageEvent, nullptr, DamageCauser);

	// only process knockback and effects if we received nonzero damage
	if (ActualDamage > 0.0f)
	{
		if (IsAlive())
		{
			CancelAttacks();
			RestoreAnimationAfterHit();

			// Character movement provides knockback without leaving the mesh in a
			// persistent partial-ragdoll pose.
			GetCharacterMovement()->AddImpulse(DamageImpulse, true);
		}

		// is the character ragdolling?
		if (!IsAlive() && GetMesh()->IsSimulatingPhysics())
		{
			// apply an impulse to the ragdoll
			GetMesh()->AddImpulseAtLocation(DamageImpulse * GetMesh()->GetMass(), DamageLocation);
		}

		// pass control to BP to play effects, etc.
		ReceivedDamage(ActualDamage, DamageLocation, DamageImpulse.GetSafeNormal());
		OnDamageReceived.Broadcast(this, DamageCauser);
	}
}

void ACombatEnemy::HandleDeath()
{
	CurrentHP = 0.0f;
	CancelAttacks();

	// hide the life bar
	LifeBar->SetHiddenInGame(true);

	// disable the collision capsule to avoid being hit again while dead
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// disable character movement
	GetCharacterMovement()->DisableMovement();

	// enable full ragdoll physics
	GetMesh()->SetSimulatePhysics(true);

	// call the died delegate to notify any subscribers
	OnEnemyDied.Broadcast();

	// set up the death timer
	GetWorld()->GetTimerManager().SetTimer(DeathTimer, this, &ACombatEnemy::RemoveFromLevel, DeathRemovalTime);
}

void ACombatEnemy::ApplyHealing(float Healing, AActor* Healer)
{
	// stub
}

void ACombatEnemy::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// ensure we're being attacked by the player
	if (DangerSource && DangerSource->ActorHasTag(FName("Player")))
	{
		// save the danger location and game time
		LastDangerLocation = DangerLocation;
		LastDangerTime = GetWorld()->GetTimeSeconds();
	}
}

void ACombatEnemy::RemoveFromLevel()
{
	// destroy this actor
	Destroy();
}

float ACombatEnemy::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// only process damage if the character is still alive
	if (CurrentHP <= 0.0f || Damage <= 0.0f)
	{
		return 0.0f;
	}

	// reduce the current HP
	const float PreviousHP = CurrentHP;
	CurrentHP = FMath::Max(bTutorialControlled ? 1.0f : 0.0f, CurrentHP - Damage);
	// Tutorial protection must not hide a valid normal hit once HP reaches one.
	const float ActualDamage = bTutorialControlled
		? FMath::Min(Damage, PreviousHP)
		: PreviousHP - CurrentHP;

	// have we run out of HP?
	if (CurrentHP <= 0.0f)
	{
		// die
		HandleDeath();
	}
	else
	{
		// update the life bar
		LifeBarWidget->SetLifePercentage(CurrentHP / MaxHP);

		RestoreAnimationAfterHit();
	}

	// return the received damage amount
	return ActualDamage;
}

void ACombatEnemy::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// is the character still alive?
	if (IsAlive())
	{
		RestoreAnimationAfterHit();
	}

	// call the landed Delegate for StateTree
	OnEnemyLanded.ExecuteIfBound();
}

void ACombatEnemy::BeginPlay()
{
	// reset HP to maximum
	CurrentHP = MaxHP;

	// we top the HP before BeginPlay so StateTree picks it up at the right value
	Super::BeginPlay();
	EnemyMeshStartingTransforms.Add(this, GetMesh()->GetRelativeTransform());

	if (bUseStationaryArenaAI)
	{
		ArenaState = EStationaryArenaState::Approach;
		ArenaStateEndTime = 0.0f;
		GetCharacterMovement()->bUseControllerDesiredRotation = false;
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetCharacterMovement()->bConstrainToPlane = true;
		GetCharacterMovement()->bSnapToPlaneAtStart = true;
		GetCharacterMovement()->SetPlaneConstraintNormal(FVector::YAxisVector);
		GetCharacterMovement()->SetPlaneConstraintOrigin(GetActorLocation());

		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		}
	}

	RestoreAnimationAfterHit();

	// get the life bar widget from the widget comp
	LifeBarWidget = Cast<UCombatLifeBar>(LifeBar->GetUserWidgetObject());
	check(LifeBarWidget);

	// fill the life bar
	LifeBarWidget->SetLifePercentage(1.0f);
}

void ACombatEnemy::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	EnemyMeshStartingTransforms.Remove(this);

	// clear the death timer
	GetWorld()->GetTimerManager().ClearTimer(DeathTimer);
}

void ACombatEnemy::CancelAttacks()
{
	bIsAttacking = false;
	CurrentComboAttack = 0;
	CurrentChargeLoop = 0;

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.1f, ComboAttackMontage);
		AnimInstance->Montage_Stop(0.1f, ChargedAttackMontage);
	}

	if (bUseStationaryArenaAI && IsAlive() && ArenaState != EStationaryArenaState::Recovery)
	{
		EnterArenaRecovery();
	}
}

void ACombatEnemy::SetTutorialControlled(bool bControlled)
{
	if (bTutorialControlled == bControlled)
	{
		return;
	}

	if (!bControlled)
	{
		// Keep tutorial ownership until the attack montage has been stopped. Montage
		// blend-out can emit trace notifies, and they must remain tutorial-suppressed.
		bTutorialDodgeWindowConsumed = true;
		SetTutorialAttackFrozen(false);
		CancelAttacks();
	}

	bTutorialControlled = bControlled;
	bTutorialAttackRequested = false;
	bTutorialDodgeWindowConsumed = !bControlled;
	TutorialDamageSourceBone = NAME_None;
	TutorialTarget.Reset();
	GetCharacterMovement()->StopMovementImmediately();

	if (bControlled)
	{
		bTutorialDodgeWindowConsumed = true;
		CancelAttacks();
		bTutorialDodgeWindowConsumed = false;
		return;
	}

	if (bUseStationaryArenaAI && IsAlive())
	{
		EnterArenaRecovery();
	}
}

void ACombatEnemy::StartTutorialDodgeAttack(ACombatCharacter* TargetPlayer)
{
	if (!bTutorialControlled || !IsAlive() || !IsValid(TargetPlayer))
	{
		return;
	}

	CancelAttacks();
	TutorialTarget = TargetPlayer;
	bTutorialAttackRequested = true;
	bTutorialDodgeWindowConsumed = false;
	TutorialDamageSourceBone = NAME_None;
}

void ACombatEnemy::ResolveTutorialDodgeAttack()
{
	if (!bTutorialControlled || !bTutorialAttackRequested || !bTutorialDodgeWindowConsumed)
	{
		return;
	}

	SetTutorialAttackFrozen(false);

	if (TutorialDamageSourceBone != NAME_None)
	{
		DoAttackTrace(TutorialDamageSourceBone);
		TutorialDamageSourceBone = NAME_None;
	}

	// Keep the consumed state as a guard against any additional trace notifies
	// that the rest of this montage may emit after it resumes.
	bTutorialAttackRequested = false;
}

void ACombatEnemy::TickTutorialControl()
{
	GetCharacterMovement()->StopMovementImmediately();
	if (!IsAlive() || bTutorialAttackFrozen || bIsAttacking)
	{
		return;
	}

	ACombatCharacter* Player = TutorialTarget.Get();
	if (!IsValid(Player))
	{
		Player = Cast<ACombatCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	}

	if (!IsValid(Player) || !Player->IsAlive())
	{
		return;
	}

	const float DeltaX = Player->GetActorLocation().X - GetActorLocation().X;
	const float DistanceToPlayer = FMath::Abs(DeltaX);
	SetActorRotation(FRotator(0.0f, DeltaX >= 0.0f ? 0.0f : 180.0f, 0.0f));

	if (DistanceToPlayer > ArenaAttackRange)
	{
		AddMovementInput(FVector(DeltaX >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f));
		return;
	}

	// During the Light and Heavy lessons, approach only so the stationary player
	// can reach the target. Commit to an attack only when the Dodge lesson asks
	// for the normal telegraphed strike.
	if (bTutorialAttackRequested)
	{
		DoAIComboAttack();
		if (bIsAttacking)
		{
			ArenaState = EStationaryArenaState::Attack;
		}
	}
}

void ACombatEnemy::SetTutorialAttackFrozen(bool bFrozen)
{
	if (bTutorialAttackFrozen == bFrozen)
	{
		return;
	}

	bTutorialAttackFrozen = bFrozen;
	GetCharacterMovement()->StopMovementImmediately();
	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		if (bFrozen)
		{
			TutorialPreviousAnimRateScale = MeshComponent->GlobalAnimRateScale;
			MeshComponent->GlobalAnimRateScale = 0.0f;
		}
		else
		{
			MeshComponent->GlobalAnimRateScale = FMath::Max(TutorialPreviousAnimRateScale, 0.01f);
		}
	}
}

void ACombatEnemy::EnterArenaRecovery()
{
	ArenaState = EStationaryArenaState::Recovery;
	ArenaStateEndTime = GetWorld()->GetTimeSeconds() + ArenaAttackCooldown
		+ FMath::FRandRange(0.0f, ArenaAttackDelayVariation);
}

void ACombatEnemy::RestoreAnimationAfterHit()
{
	if (!IsAlive())
	{
		return;
	}

	GetMesh()->SetAllBodiesSimulatePhysics(false);
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetPhysicsBlendWeight(0.0f);

	if (const FTransform* StartingTransform = EnemyMeshStartingTransforms.Find(this))
	{
		GetMesh()->SetRelativeTransform(*StartingTransform);
	}
}
