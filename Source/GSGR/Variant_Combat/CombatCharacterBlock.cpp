#include "CombatCharacter.h"

void ACombatCharacter::StartBlocking()
{
	if (!bAllowBlockInput || !IsAlive() || IsBackDodgeActive() || bIsBlocking) return;
	// Guard takes priority over buffered attacks, including an in-progress charge.
	CancelAttacks();
	bIsBlocking = true;
}

void ACombatCharacter::StopBlocking()
{
	bIsBlocking = false;
}

void ACombatCharacter::SetBlockingAllowed(bool bAllowed)
{
	bAllowBlockInput = bAllowed;
	if (!bAllowed) StopBlocking();
}

bool ACombatCharacter::HasJustBlockedHit() const
{
	return bIsBlocking && GetWorld()->GetTimeSeconds() - LastBlockedHitTime < 0.2f;
}
