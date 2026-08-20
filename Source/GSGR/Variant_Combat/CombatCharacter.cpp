// Copyright Epic Games, Inc. All Rights Reserved.


#include "CombatCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "CombatLifeBar.h"
#include "Engine/DamageEvents.h"
#include "TimerManager.h"
#include "Engine/LocalPlayer.h"
#include "CombatPlayerController.h"
#include "AI/CombatEnemy.h"
#include "Animation/AnimInstance.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	struct FBackDodgeRuntimeState
	{
		bool bIsDodging = false;
		float StartTime = 0.0f;
		float NextAllowedTime = 0.0f;
		float InvulnerableUntil = 0.0f;
		FVector Direction = FVector::ZeroVector;
	};

	TMap<const ACombatCharacter*, FBackDodgeRuntimeState> BackDodgeStates;
}

ACombatCharacter::ACombatCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// bind the attack montage ended delegate
	OnAttackMontageEnded.BindUObject(this, &ACombatCharacter::AttackMontageEnded);
	OnDodgeMontageEnded.BindUObject(this, &ACombatCharacter::DodgeMontageEnded);

	// Reuse the mannequin dash that already drives the Platforming variant. The
	// combat dodge keeps its existing short out-and-back gameplay displacement.
	static ConstructorHelpers::FObjectFinder<UAnimMontage> DodgeMontageAsset(
		TEXT("/Game/Variant_Platforming/Anims/AM_Dash.AM_Dash"));
	if (DodgeMontageAsset.Succeeded())
	{
		DodgeMontage = DodgeMontageAsset.Object;
	}

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(35.0f, 90.0f);

	// Configure character movement
	GetCharacterMovement()->MaxWalkSpeed = 400.0f;

	// create the camera boom
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);

	CameraBoom->TargetArmLength = DefaultCameraDistance;
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->bEnableCameraRotationLag = true;

	// create the orbiting camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// create the life bar widget component
	LifeBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("LifeBar"));
	LifeBar->SetupAttachment(RootComponent);

	// set the player tag
	Tags.Add(FName("Player"));
}

void ACombatCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Keep the living player on its stationary anchor. The dodge is the only
	// stationary-combat action that intentionally moves the capsule; attack
	// montages play with their root translation discarded below.
	if (bStationaryCombatMode && bHasStationaryCombatTransform && IsAlive())
	{
		if (IsBackDodgeActive())
		{
			UpdateBackDodge();
			return;
		}

		GetCharacterMovement()->StopMovementImmediately();

		if (!GetActorTransform().Equals(StationaryCombatTransform, 0.01f))
		{
			SetActorTransform(StationaryCombatTransform, false, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void ACombatCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void ACombatCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void ACombatCharacter::ComboAttackPressed()
{
	// route the input
	DoComboAttackStart();
}

void ACombatCharacter::ChargedAttackPressed()
{
	// route the input
	DoChargedAttackStart();
}

void ACombatCharacter::ChargedAttackReleased()
{
	// route the input
	DoChargedAttackEnd();
}

void ACombatCharacter::ToggleCamera()
{
	if (bStationaryCombatMode)
	{
		return;
	}

	// call the BP hook
	BP_ToggleCamera();
}

void ACombatCharacter::BackDodgePressed()
{
	DoBackDodge();
}

void ACombatCharacter::DoMove(float Right, float Forward)
{
	if (bStationaryCombatMode)
	{
		return;
	}

	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void ACombatCharacter::DoLook(float Yaw, float Pitch)
{
	if (bStationaryCombatMode)
	{
		return;
	}

	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void ACombatCharacter::DoComboAttackStart()
{
	if (!bAllowLightAttackInput || !IsAlive() || IsBackDodgeActive())
	{
		return;
	}

	// are we already playing an attack animation?
	if (bIsAttacking)
	{
		// cache the input time so we can check it later
		CachedAttackInputTime = GetWorld()->GetTimeSeconds();

		return;
	}

	// perform a combo attack
	ComboAttack();
}

void ACombatCharacter::DoComboAttackEnd()
{
	// stub
}

void ACombatCharacter::DoChargedAttackStart()
{
	if (!bAllowHeavyAttackInput || !IsAlive() || IsBackDodgeActive())
	{
		return;
	}

	// raise the charging attack flag
	bIsChargingAttack = true;

	if (bIsAttacking)
	{
		// do not attack if the charge animation hasn't looped at least once
		if (!bHasLoopedChargedAttack)
		{
			bHasReleasedChargedAttack = false;
		}

		// cache the input time so we can check it later
		CachedAttackInputTime = GetWorld()->GetTimeSeconds();

		return;
	}

	ChargedAttack();
}

void ACombatCharacter::DoChargedAttackEnd()
{
	if (!bAllowHeavyAttackInput || !IsAlive() || IsBackDodgeActive())
	{
		bIsChargingAttack = false;
		return;
	}

	// lower the charging attack flag
	bIsChargingAttack = false;

	// have we done the charge loop at least once and haven't released the button yet?
	if (bHasLoopedChargedAttack && !bHasReleasedChargedAttack)
	{
		// release the charge and resolve the attack
		bHasReleasedChargedAttack = true;

		LoopOrResolveChargedAttack();
	}
}

void ACombatCharacter::DoBackDodge()
{
	if (!bAllowDodgeInput || !bStationaryCombatMode || !bHasStationaryCombatTransform || !IsAlive() || IsBackDodgeActive())
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	FBackDodgeRuntimeState& DodgeState = BackDodgeStates.FindOrAdd(this);
	if (CurrentTime < DodgeState.NextAllowedTime)
	{
		return;
	}

	DodgeState.bIsDodging = true;
	DodgeState.StartTime = CurrentTime;
	DodgeState.NextAllowedTime = CurrentTime + BackDodgeCooldown;
	DodgeState.InvulnerableUntil = CurrentTime + BackDodgeInvulnerabilityTime;
	DodgeState.Direction = -StationaryCombatTransform.GetRotation().GetForwardVector();
	DodgeState.Direction.Z = 0.0f;
	DodgeState.Direction.Normalize();

	CancelAttacks();
	RestoreAnimationAfterHit();

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		// Dodge displacement is the existing fixed out-and-back gameplay curve,
		// so discard this montage's platforming root motion to avoid double motion.
		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);

		if (DodgeMontage)
		{
			// MM_Dash is authored as a forward dash. Turn only the visual mesh away
			// from the opponent so the pose and the backward displacement agree,
			// without rotating the camera, capsule, or melee facing direction.
			FRotator DodgeMeshRotation = MeshStartingTransform.Rotator();
			DodgeMeshRotation.Yaw += 180.0f;
			GetMesh()->SetRelativeRotation(DodgeMeshRotation);

			const float PlaybackRate = FMath::Max(DodgeMontage->GetPlayLength() / BackDodgeDuration, 0.01f);
			const float MontageDuration = AnimInstance->Montage_Play(
				DodgeMontage, PlaybackRate, EMontagePlayReturnType::Duration, 0.0f, true);

			if (MontageDuration > 0.0f)
			{
				AnimInstance->Montage_SetEndDelegate(OnDodgeMontageEnded, DodgeMontage);
			}
			else
			{
				GetMesh()->SetRelativeTransform(MeshStartingTransform);
			}
		}
	}

	OnDodgeStarted.Broadcast();
}

void ACombatCharacter::SetCombatInputPermissions(bool bAllowLightAttack, bool bAllowHeavyAttack, bool bAllowDodge, bool bCancelDisallowedAction)
{
	bAllowLightAttackInput = bAllowLightAttack;
	bAllowHeavyAttackInput = bAllowHeavyAttack;
	bAllowDodgeInput = bAllowDodge;
	CachedAttackInputTime = -1000.0f;

	if (!bAllowHeavyAttackInput)
	{
		bIsChargingAttack = false;
		bHasReleasedChargedAttack = false;
	}

	if (!bCancelDisallowedAction)
	{
		return;
	}

	const bool bCurrentAttackDisallowed =
		(ActiveAttackType == ECombatPlayerAttackType::Light && !bAllowLightAttackInput)
		|| (ActiveAttackType == ECombatPlayerAttackType::Heavy && !bAllowHeavyAttackInput);
	if (bCurrentAttackDisallowed)
	{
		CancelAttacks();
	}

	if (IsBackDodgeActive() && !bAllowDodgeInput)
	{
		FinishBackDodge();
	}
}

void ACombatCharacter::ResetHP()
{
	// reset the current HP total
	CurrentHP = MaxHP;

	// update the life bar
	LifeBarWidget->SetLifePercentage(1.0f);
}

void ACombatCharacter::ComboAttack()
{
	if (!IsAlive() || IsBackDodgeActive() || !ComboAttackMontage)
	{
		return;
	}

	// raise the attacking flag
	bIsAttacking = true;
	ActiveAttackType = ECombatPlayerAttackType::Light;
	CachedAttackInputTime = -1000.0f;

	// reset the combo count
	ComboCount = 0;

	// notify enemies they are about to be attacked
	NotifyEnemiesOfIncomingAttack();

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

void ACombatCharacter::ChargedAttack()
{
	if (!IsAlive() || IsBackDodgeActive() || !ChargedAttackMontage)
	{
		return;
	}

	// raise the attacking flag
	bIsAttacking = true;
	ActiveAttackType = ECombatPlayerAttackType::Heavy;
	CachedAttackInputTime = -1000.0f;

	// reset the charge loop flag
	bHasLoopedChargedAttack = false;

	// reset the charge release flag
	bHasReleasedChargedAttack = false;

	// notify enemies they are about to be attacked
	NotifyEnemiesOfIncomingAttack();

	// play the charged attack montage
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

void ACombatCharacter::AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	// reset the attacking flag
	bIsAttacking = false;
	ActiveAttackType = ECombatPlayerAttackType::None;

	if (bStationaryCombatMode && bHasStationaryCombatTransform && !IsBackDodgeActive())
	{
		// Never promote montage displacement to the stationary anchor. Doing so
		// accumulated every attack lunge and eventually walked the player out of
		// the arena. This is also a safety reset for any external displacement.
		GetCharacterMovement()->StopMovementImmediately();
		SetActorTransform(StationaryCombatTransform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (!IsAlive() || IsBackDodgeActive())
	{
		return;
	}

	// check if we have a non-stale cached input
	const bool bHasBufferedAttack = GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= AttackInputCacheTimeTolerance;
	CachedAttackInputTime = -1000.0f;
	if (bHasBufferedAttack)
	{
		// are we holding the charged attack button?
		if (bIsChargingAttack)
		{
			// do a charged attack
			ChargedAttack();
		}
		else
		{
			// do a regular attack
			ComboAttack();
		}
	}
}

void ACombatCharacter::DoAttackTrace(FName DamageSourceBone)
{
	if (!IsAlive() || !bIsAttacking || IsBackDodgeActive())
	{
		return;
	}

	// sweep for objects in front of the character to be hit by the attack
	TArray<FHitResult> OutHits;

	// start at the provided socket location, sweep forward
	const FVector TraceStart = GetMesh()->GetSocketLocation(DamageSourceBone);
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * MeleeTraceDistance);

	// check for pawn and world dynamic collision object types
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

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

			// check if we've hit a damageable actor
			ICombatDamageable* Damageable = Cast<ICombatDamageable>(HitActor);

			if (Damageable)
			{
				// knock upwards and away from the impact normal
				const FVector Impulse = (CurrentHit.ImpactNormal * -MeleeKnockbackImpulse) + (FVector::UpVector * MeleeLaunchImpulse);

				// pass the damage event to the actor
				Damageable->ApplyDamage(MeleeDamage, this, CurrentHit.ImpactPoint, Impulse);

				// call the BP handler to play effects, etc.
				DealtDamage(MeleeDamage, CurrentHit.ImpactPoint);
			}
		}
	}
}

void ACombatCharacter::CheckCombo()
{
	// are we playing a non-charge attack animation?
	if (bIsAttacking && !bIsChargingAttack)
	{
		// is the last attack input not stale?
		if (GetWorld()->GetTimeSeconds() - CachedAttackInputTime <= ComboInputCacheTimeTolerance)
		{
			// consume the attack input so we don't accidentally trigger it twice
			CachedAttackInputTime = 0.0f;

			// increase the combo counter
			++ComboCount;

			// do we still have a combo section to play?
			if (ComboCount < ComboSectionNames.Num())
			{
				// notify enemies they are about to be attacked
				NotifyEnemiesOfIncomingAttack();

				// jump to the next combo section
				if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
				{
					AnimInstance->Montage_JumpToSection(ComboSectionNames[ComboCount], ComboAttackMontage);
				}
			}
		}
	}
}

void ACombatCharacter::CheckChargedAttack()
{
	// raise the looped charged attack flag
	bHasLoopedChargedAttack = true;

	// set the input release flag from the input. This will determine if we loop or resolve
	bHasReleasedChargedAttack = !bIsChargingAttack;

	// resolve the charge loop
	LoopOrResolveChargedAttack();
}

void ACombatCharacter::LoopOrResolveChargedAttack()
{
	// jump to either the loop or the attack section depending on whether we've released the charge
	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_JumpToSection(bHasReleasedChargedAttack ? ChargeAttackSection : ChargeLoopSection , ChargedAttackMontage);
	}
}

void ACombatCharacter::NotifyEnemiesOfIncomingAttack()
{
	// sweep for objects in front of the character to be hit by the attack
	TArray<FHitResult> OutHits;

	// start at the actor location, sweep forward
	const FVector TraceStart = GetActorLocation();
	const FVector TraceEnd = TraceStart + (GetActorForwardVector() * DangerTraceDistance);

	// check for pawn object types only
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	// use a sphere shape for the sweep
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(DangerTraceRadius);

	// ignore self
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	if (GetWorld()->SweepMultiByObjectType(OutHits, TraceStart, TraceEnd, FQuat::Identity, ObjectParams, CollisionShape, QueryParams))
	{
		// iterate over each object hit
		for (const FHitResult& CurrentHit : OutHits)
		{
			// check if we've hit a damageable actor
			ICombatDamageable* Damageable = Cast<ICombatDamageable>(CurrentHit.GetActor());

			if (Damageable)
			{
				// notify the enemy
				Damageable->NotifyDanger(GetActorLocation(), this);
			}
		}
	}
}

void ACombatCharacter::ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse)
{
	// pass the damage event to the actor
	FDamageEvent DamageEvent;
	const float ActualDamage = TakeDamage(Damage, DamageEvent, nullptr, DamageCauser);

	// only process knockback and effects if we received nonzero damage
	if (ActualDamage > 0.0f)
	{
		if (IsAlive())
		{
			if (IsBackDodgeActive())
			{
				FinishBackDodge();
			}
			CancelAttacks();
			RestoreAnimationAfterHit();
		}

		// apply the knockback impulse
		if (!bStationaryCombatMode)
		{
			GetCharacterMovement()->AddImpulse(DamageImpulse, true);
		}

		// is the character ragdolling?
		if (GetMesh()->IsSimulatingPhysics())
		{
			// apply an impulse to the ragdoll
			GetMesh()->AddImpulseAtLocation(DamageImpulse * GetMesh()->GetMass(), DamageLocation);
		}

		// pass control to BP to play effects, etc.
		ReceivedDamage(ActualDamage, DamageLocation, DamageImpulse.GetSafeNormal());
	}

}

void ACombatCharacter::HandleDeath()
{
	CurrentHP = 0.0f;
	if (IsBackDodgeActive())
	{
		FinishBackDodge();
	}
	BackDodgeStates.Remove(this);
	CancelAttacks();

	// dead pawns must not keep participating in melee sweeps
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// disable movement while we're dead
	GetCharacterMovement()->DisableMovement();

	// enable full ragdoll physics
	GetMesh()->SetSimulatePhysics(true);

	// hide the life bar
	LifeBar->SetHiddenInGame(true);

	// Keep the arena camera fixed. The original template camera pull-back remains
	// available when stationary mode is disabled.
	GetCameraBoom()->TargetArmLength = bStationaryCombatMode ? SideViewCameraDistance : DeathCameraDistance;

	// notify the endless arena loop that the run is over
	OnPlayerDied.Broadcast();

	// schedule respawning only when explicitly requested
	if (bRespawnAfterDeath)
	{
		GetWorld()->GetTimerManager().SetTimer(RespawnTimer, this, &ACombatCharacter::RespawnCharacter, RespawnTime, false);
	}
}

void ACombatCharacter::ApplyHealing(float Healing, AActor* Healer)
{
	// stub
}

void ACombatCharacter::NotifyDanger(const FVector& DangerLocation, AActor* DangerSource)
{
	// stub
}

void ACombatCharacter::RespawnCharacter()
{
	// destroy the character and let it be respawned by the Player Controller
	Destroy();
}

float ACombatCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	const ACombatEnemy* CombatEnemy = Cast<ACombatEnemy>(DamageCauser);
	// The FTUE uses the normal enemy montage and melee trace so the dodge lesson
	// exercises real combat timing. Tutorial ownership is nevertheless a hard
	// safety boundary: its staged strike must never reduce the player's HP.
	if (CombatEnemy && CombatEnemy->IsTutorialControlled())
	{
		return 0.0f;
	}

	// only process damage if the character is still alive
	if (CurrentHP <= 0.0f || Damage <= 0.0f || IsDodgeInvulnerable())
	{
		return 0.0f;
	}

	// reduce the current HP
	const float PreviousHP = CurrentHP;
	CurrentHP = FMath::Max(0.0f, CurrentHP - Damage);
	const float ActualDamage = PreviousHP - CurrentHP;

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

		// Nonlethal hits keep the mesh under animation control. The old partial
		// ragdoll only recovered in Landed(), which never fires for a planted pawn.
		RestoreAnimationAfterHit();
	}

	// return the received damage amount
	return ActualDamage;
}

void ACombatCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// is the character still alive?
	if (IsAlive())
	{
		RestoreAnimationAfterHit();
	}
}

void ACombatCharacter::CancelAttacks()
{
	bIsAttacking = false;
	ActiveAttackType = ECombatPlayerAttackType::None;
	bIsChargingAttack = false;
	bHasLoopedChargedAttack = false;
	bHasReleasedChargedAttack = false;
	CachedAttackInputTime = -1000.0f;

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		AnimInstance->Montage_Stop(0.1f, ComboAttackMontage);
		AnimInstance->Montage_Stop(0.1f, ChargedAttackMontage);
	}
}

void ACombatCharacter::RestoreAnimationAfterHit()
{
	if (!IsAlive())
	{
		return;
	}

	GetMesh()->SetAllBodiesSimulatePhysics(false);
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetPhysicsBlendWeight(0.0f);
	GetMesh()->SetRelativeTransform(MeshStartingTransform);
}

void ACombatCharacter::UpdateBackDodge()
{
	FBackDodgeRuntimeState* DodgeState = BackDodgeStates.Find(this);
	if (!DodgeState || !DodgeState->bIsDodging)
	{
		return;
	}

	const float NormalizedTime = FMath::Clamp((GetWorld()->GetTimeSeconds() - DodgeState->StartTime) / BackDodgeDuration, 0.0f, 1.0f);

	// Move back quickly, then settle smoothly onto the fixed combat anchor.
	const float OutwardFraction = 0.45f;
	const float DodgeAlpha = NormalizedTime < OutwardFraction
		? FMath::InterpEaseOut(0.0f, 1.0f, NormalizedTime / OutwardFraction, 2.0f)
		: FMath::InterpEaseIn(1.0f, 0.0f, (NormalizedTime - OutwardFraction) / (1.0f - OutwardFraction), 2.0f);

	const FVector TargetLocation = StationaryCombatTransform.GetLocation() + DodgeState->Direction * BackDodgeDistance * DodgeAlpha;
	FHitResult Hit;
	SetActorLocation(TargetLocation, true, &Hit, ETeleportType::TeleportPhysics);

	if (NormalizedTime >= 1.0f)
	{
		FinishBackDodge();
	}
}

void ACombatCharacter::FinishBackDodge()
{
	if (FBackDodgeRuntimeState* DodgeState = BackDodgeStates.Find(this))
	{
		DodgeState->bIsDodging = false;
	}

	if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
	{
		if (DodgeMontage && AnimInstance->Montage_IsPlaying(DodgeMontage))
		{
			AnimInstance->Montage_Stop(0.08f, DodgeMontage);
		}

		AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
	}

	GetMesh()->SetRelativeTransform(MeshStartingTransform);
	SetActorTransform(StationaryCombatTransform, false, nullptr, ETeleportType::TeleportPhysics);
}

void ACombatCharacter::DodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage == DodgeMontage && IsBackDodgeActive())
	{
		FinishBackDodge();
	}
}

bool ACombatCharacter::IsDodgeInvulnerable() const
{
	const FBackDodgeRuntimeState* DodgeState = BackDodgeStates.Find(this);
	return DodgeState && DodgeState->bIsDodging && GetWorld() && GetWorld()->GetTimeSeconds() <= DodgeState->InvulnerableUntil;
}

bool ACombatCharacter::IsBackDodgeActive() const
{
	const FBackDodgeRuntimeState* DodgeState = BackDodgeStates.Find(this);
	return DodgeState && DodgeState->bIsDodging;
}

void ACombatCharacter::BeginPlay()
{
	Super::BeginPlay();
	BackDodgeStates.Remove(this);

	// get the life bar from the widget component
	LifeBarWidget = Cast<UCombatLifeBar>(LifeBar->GetUserWidgetObject());
	check(LifeBarWidget);

	// initialize the camera and movement for the selected combat style
	if (bStationaryCombatMode)
	{
		GetCameraBoom()->bUsePawnControlRotation = false;
		GetCameraBoom()->bEnableCameraLag = false;
		GetCameraBoom()->bEnableCameraRotationLag = false;
		GetCameraBoom()->bDoCollisionTest = false;
		GetCameraBoom()->SetRelativeRotation(SideViewCameraRotation);
		GetCameraBoom()->TargetOffset = SideViewCameraTargetOffset;
		GetCameraBoom()->TargetArmLength = SideViewCameraDistance;

		GetFollowCamera()->bUsePawnControlRotation = false;
		GetFollowCamera()->SetFieldOfView(65.0f);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->MaxWalkSpeed = 0.0f;
		GetCharacterMovement()->bOrientRotationToMovement = false;
		GetCharacterMovement()->bUseControllerDesiredRotation = false;
		GetCharacterMovement()->bConstrainToPlane = true;
		GetCharacterMovement()->bSnapToPlaneAtStart = true;
		GetCharacterMovement()->SetPlaneConstraintNormal(FVector::YAxisVector);
		GetCharacterMovement()->SetPlaneConstraintOrigin(GetActorLocation());

		bUseControllerRotationYaw = false;
		StationaryCombatTransform = GetActorTransform();
		bHasStationaryCombatTransform = true;
	}
	else
	{
		GetCameraBoom()->TargetArmLength = DefaultCameraDistance;
	}

	// save the relative transform for the mesh so we can reset the ragdoll later
	MeshStartingTransform = GetMesh()->GetRelativeTransform();

	// Stationary combat must not accumulate the translation authored into attack
	// montages. Ignore the extracted root motion so the pose still plays while
	// the capsule remains at its arena anchor.
	if (bStationaryCombatMode)
	{
		if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
		{
			AnimInstance->SetRootMotionMode(ERootMotionMode::IgnoreRootMotion);
		}
	}

	// set the life bar color
	LifeBarWidget->SetBarColor(LifeBarColor);

	// reset HP to maximum
	ResetHP();
	RestoreAnimationAfterHit();
}

void ACombatCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	BackDodgeStates.Remove(this);

	// clear the respawn timer
	GetWorld()->GetTimerManager().ClearTimer(RespawnTimer);
}

void ACombatCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Move);

		// Stationary backward dodge (Space / gamepad face button left in IMC_Combat)
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACombatCharacter::BackDodgePressed);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Look);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ACombatCharacter::Look);

		// Combo Attack
		EnhancedInputComponent->BindAction(ComboAttackAction, ETriggerEvent::Started, this, &ACombatCharacter::ComboAttackPressed);

		// Charged Attack
		EnhancedInputComponent->BindAction(ChargedAttackAction, ETriggerEvent::Started, this, &ACombatCharacter::ChargedAttackPressed);
		EnhancedInputComponent->BindAction(ChargedAttackAction, ETriggerEvent::Completed, this, &ACombatCharacter::ChargedAttackReleased);

		// Camera Side Toggle
		EnhancedInputComponent->BindAction(ToggleCameraAction, ETriggerEvent::Triggered, this, &ACombatCharacter::ToggleCamera);
	}
}

void ACombatCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// update the respawn transform on the Player Controller
	if (ACombatPlayerController* PC = Cast<ACombatPlayerController>(GetController()))
	{
		PC->SetRespawnTransform(GetActorTransform());
	}
}

