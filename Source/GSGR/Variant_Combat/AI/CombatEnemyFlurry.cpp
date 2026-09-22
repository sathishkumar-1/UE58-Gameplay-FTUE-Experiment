#include "CombatEnemy.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GSGR.h"

void ACombatEnemy::BeginFlurryWindup()
{
	if (!ComboAttackMontage || ComboSectionNames.IsEmpty() || !GetMesh()->GetAnimInstance())
	{
		EnterArenaRecovery();
		return;
	}
	FlurryState = ECombatFlurryState::Windup;
	FlurryStateEndTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, FlurryWindupDuration);
	NormalAttacksSinceFlurry = 0;
	GetCharacterMovement()->StopMovementImmediately();
	UE_LOG(LogGSGR, Display, TEXT("Flurry enemy: windup"));
}

void ACombatEnemy::PlayFlurryMontage()
{
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	if (!Anim || !ComboAttackMontage || ComboSectionNames.IsEmpty())
	{
		FinishFlurry();
		return;
	}
	// Damage still comes exclusively from the montage's normal fist trace notifies.
	bIsAttacking = true;
	CurrentComboAttack = 0;
	if (Anim->Montage_Play(ComboAttackMontage, FMath::Clamp(FlurryPlayRate, 1.0f, 4.0f)) <= 0.0f)
	{
		FinishFlurry();
		return;
	}
	Anim->Montage_JumpToSection(ComboSectionNames[0], ComboAttackMontage);
	Anim->Montage_SetEndDelegate(OnAttackMontageEnded, ComboAttackMontage);
}

void ACombatEnemy::TickFlurry()
{
	GetCharacterMovement()->StopMovementImmediately();
	const float Now = GetWorld()->GetTimeSeconds();
	switch (FlurryState)
	{
	case ECombatFlurryState::Windup:
		if (Now >= FlurryStateEndTime)
		{
			FlurryState = ECombatFlurryState::Flurry;
			FlurryStateEndTime = Now + FMath::Clamp(FlurryDuration, 1.0f, 2.0f);
			PlayFlurryMontage();
			UE_LOG(LogGSGR, Display, TEXT("Flurry enemy: burst"));
		}
		break;
	case ECombatFlurryState::Flurry:
		if (Now >= FlurryStateEndTime)
		{
			FinishFlurry();
		}
		else if (!bIsAttacking)
		{
			PlayFlurryMontage();
		}
		break;
	case ECombatFlurryState::Exhausted:
		if (Now >= FlurryStateEndTime)
		{
			FlurryState = ECombatFlurryState::Recovering;
			FlurryStateEndTime = Now + FMath::Max(0.1f, FlurryRecoveryDuration);
			if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
			{
				if (ExhaustedMontage) Anim->Montage_Stop(0.15f, ExhaustedMontage);
			}
			UE_LOG(LogGSGR, Display, TEXT("Flurry enemy: recovering"));
		}
		break;
	case ECombatFlurryState::Recovering:
		if (Now >= FlurryStateEndTime)
		{
			ResetFlurry();
			ArenaState = EStationaryArenaState::Approach;
		}
		break;
	default:
		break;
	}
}

void ACombatEnemy::FinishFlurry()
{
	// Switch state before stopping: synchronous montage callbacks must not start normal recovery.
	FlurryState = ECombatFlurryState::Exhausted;
	FlurryStateEndTime = GetWorld()->GetTimeSeconds() + FMath::Clamp(ExhaustedDuration, 0.5f, 1.0f);
	CancelAttacks();
	if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
	{
		if (ExhaustedAnimation && ComboAttackMontage && !ComboAttackMontage->SlotAnimTracks.IsEmpty())
		{
			// Reuse the mannequin's heavy recoil as the prototype tired/slumped motion.
			ExhaustedMontage = Anim->PlaySlotAnimationAsDynamicMontage(ExhaustedAnimation,
				ComboAttackMontage->SlotAnimTracks[0].SlotName, 0.08f, 0.15f,
				ExhaustedAnimation->GetPlayLength() / FMath::Clamp(ExhaustedDuration, 0.5f, 1.0f));
		}
	}
	UE_LOG(LogGSGR, Display, TEXT("Flurry enemy: exhausted (one-hit counter window)"));
}

void ACombatEnemy::ResetFlurry()
{
	FlurryState = ECombatFlurryState::None;
	FlurryStateEndTime = 0.0f;
	if (ExhaustedMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance()) Anim->Montage_Stop(0.1f, ExhaustedMontage);
		ExhaustedMontage = nullptr;
	}
}

FText ACombatEnemy::GetCombatCue() const
{
	if (!bFlurryEnemy || !IsAlive() || bTutorialControlled) return FText::GetEmpty();
	switch (FlurryState)
	{
	case ECombatFlurryState::Windup: return NSLOCTEXT("Flurry", "Windup", "FLURRY INCOMING - HOLD EVADE");
	case ECombatFlurryState::Flurry: return NSLOCTEXT("Flurry", "Burst", "KEEP EVADING");
	case ECombatFlurryState::Exhausted: return NSLOCTEXT("Flurry", "Counter", "EXHAUSTED - RELEASE EVADE AND PUNCH!");
	case ECombatFlurryState::Recovering: return NSLOCTEXT("Flurry", "Recover", "RECOVERING");
	default: return NSLOCTEXT("Flurry", "Ready", "RED ENEMY - WATCH FOR THE FLURRY");
	}
}
