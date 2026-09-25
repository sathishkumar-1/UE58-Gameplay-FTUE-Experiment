#include "CombatCharacter.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"

void ACombatCharacter::StartEvading()
{
	if (!bAllowEvadeInput || !IsAlive() || IsBackDodgeActive() || bIsEvading) return;
	// Evade takes priority over buffered attacks, including an in-progress charge.
	CancelAttacks();
	RestoreAnimationAfterHit();
	bIsEvading = true;
	bFinishCurrentEvadeAnimation = false;
	UpdateEvadeAnimation();
}

void ACombatCharacter::StopEvading()
{
	bIsEvading = false;
	bFinishCurrentEvadeAnimation = false;
	LastEvadedHitTime = -1000.0f;
	if (EvadeMontage)
	{
		if (UAnimInstance* Anim = GetMesh()->GetAnimInstance())
		{
			// Stop only our montage; never interrupt the follow-up counter punch.
			Anim->Montage_Stop(EvadeBlendTime, EvadeMontage);
		}
		EvadeMontage = nullptr;
	}
}

void ACombatCharacter::FinishCurrentEvadeAnimation()
{
	// Keep the current clip playing, but do not let held input start another one.
	bAllowEvadeInput = false;
	if (bIsEvading) bFinishCurrentEvadeAnimation = true;
}

void ACombatCharacter::UpdateEvadeAnimation()
{
	if (!bIsEvading) return;
	if (!IsAlive() || (!bAllowEvadeInput && !bFinishCurrentEvadeAnimation)) { StopEvading(); return; }
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	if (!Anim)
	{
		if (bFinishCurrentEvadeAnimation) StopEvading();
		return;
	}
	if (bFinishCurrentEvadeAnimation)
	{
		if (!EvadeMontage || !Anim->Montage_IsPlaying(EvadeMontage)) StopEvading();
		return;
	}

	const float PlayRate = FMath::Max(0.1f, EvadePlayRate);
	const float BlendTime = FMath::Clamp(EvadeBlendTime, 0.0f, 0.3f);
	if (EvadeMontage && Anim->Montage_IsPlaying(EvadeMontage)
		&& Anim->Montage_GetPosition(EvadeMontage) < EvadeMontage->GetPlayLength() - BlendTime * PlayRate)
	{
		return;
	}

	TArray<int32, TInlineAllocator<3>> Candidates;
	for (int32 Index = 0; Index < EvadeAnimations.Num(); ++Index)
	{
		if (EvadeAnimations[Index] && EvadeAnimations[Index]->GetPlayLength() > 0.0f)
		{
			Candidates.Add(Index);
		}
	}
	if (Candidates.Num() > 1) Candidates.Remove(LastEvadeAnimationIndex);
	if (Candidates.IsEmpty()) return;

	const int32 Index = Candidates[FMath::RandHelper(Candidates.Num())];
	const FName Slot = ComboAttackMontage && !ComboAttackMontage->SlotAnimTracks.IsEmpty()
		? ComboAttackMontage->SlotAnimTracks[0].SlotName : FName(TEXT("DefaultSlot"));
	EvadeMontage = Anim->PlaySlotAnimationAsDynamicMontage(
		EvadeAnimations[Index], Slot, BlendTime, BlendTime, PlayRate);
	if (EvadeMontage)
	{
		LastEvadeAnimationIndex = Index;
		UE_LOG(LogCombatCharacter, Verbose, TEXT("Evade animation: %s"), *EvadeAnimations[Index]->GetName());
	}
}

void ACombatCharacter::SetEvadingAllowed(bool bAllowed)
{
	bAllowEvadeInput = bAllowed;
	if (!bAllowed) StopEvading();
}

bool ACombatCharacter::HasJustEvadedHit() const
{
	return bIsEvading && GetWorld()->GetTimeSeconds() - LastEvadedHitTime < 0.2f;
}
