#include "CombatEnemy.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GSGR.h"

namespace
{
	// Draw the fist back quickly, then let the player read the loaded punch pose.
	constexpr float FlurryWindupDrawFraction = 0.35f;

	float GetWindupPoseTime(const UAnimMontage* Montage, FName ChargeSection)
	{
		const int32 SectionIndex = Montage->GetSectionIndex(ChargeSection);
		float ChargeStart = 0.0f;
		float ChargeEnd = 0.0f;
		if (SectionIndex != INDEX_NONE)
		{
			Montage->GetSectionStartAndEndTime(SectionIndex, ChargeStart, ChargeEnd);
		}
		// The charged montage's Charge section begins with the fist drawn back.
		// A combo-only enemy can use the opening preparation of its first punch.
		return FMath::Clamp(ChargeStart > 0.0f ? ChargeStart : 0.12f,
			0.0f, Montage->GetPlayLength() * 0.5f);
	}
}

void ACombatEnemy::BeginFlurryWindup()
{
	if (!ComboAttackMontage || ComboSectionNames.IsEmpty() || !GetMesh()->GetAnimInstance())
	{
		EnterArenaRecovery();
		return;
	}
	FlurryState = ECombatFlurryState::Windup;
	const float BaseWindupDuration = FMath::Max(0.1f, FlurryWindupDuration);
	ActiveFlurryWindupDuration = BaseWindupDuration
		+ (bTutorialControlled ? TutorialFlurryPromptDelay : 0.0f);
	FlurryStateEndTime = GetWorld()->GetTimeSeconds() + ActiveFlurryWindupDuration;
	NormalAttacksSinceFlurry = 0;
	GetCharacterMovement()->StopMovementImmediately();
	CancelAttacks();
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	UAnimMontage* WindupMontage = ChargedAttackMontage ? ChargedAttackMontage : ComboAttackMontage;
	if (Anim->Montage_Play(WindupMontage) <= 0.0f)
	{
		ResetFlurry();
		EnterArenaRecovery();
		return;
	}
	// Sample only the preparation frames in TickFlurry. Pausing prevents the
	// attack/charge notifies and root motion from advancing during anticipation.
	Anim->Montage_Pause(WindupMontage);
	const float Duration = ActiveFlurryWindupDuration;
	UE_LOG(LogGSGR, Display, TEXT("Flurry enemy: windup (%.3f s: draw %.3f, hold %.3f)"),
		Duration, BaseWindupDuration * FlurryWindupDrawFraction,
		Duration - BaseWindupDuration * FlurryWindupDrawFraction);
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
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			UAnimMontage* WindupMontage = ChargedAttackMontage ? ChargedAttackMontage : ComboAttackMontage;
			if (WindupMontage)
			{
				const float Elapsed = Now - (FlurryStateEndTime - ActiveFlurryWindupDuration);
				const float DrawAlpha = FMath::SmoothStep(0.0f,
					FMath::Max(0.1f, FlurryWindupDuration) * FlurryWindupDrawFraction, Elapsed);
				Anim->Montage_SetPosition(WindupMontage, GetWindupPoseTime(WindupMontage, ChargeLoopSection) * DrawAlpha);
			}
		}
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
	if (FlurryState == ECombatFlurryState::Windup)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			UAnimMontage* WindupMontage = ChargedAttackMontage ? ChargedAttackMontage : ComboAttackMontage;
			if (WindupMontage) Anim->Montage_Stop(0.1f, WindupMontage);
		}
	}
	FlurryState = ECombatFlurryState::None;
	FlurryStateEndTime = 0.0f;
	ActiveFlurryWindupDuration = 0.0f;
	TutorialFlurryPromptDelay = 0.0f;
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
