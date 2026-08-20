// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatAttacker.h"
#include "CombatDamageable.h"
#include "Animation/AnimInstance.h"
#include "CombatCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;
class UCombatLifeBar;
class UWidgetComponent;

DECLARE_LOG_CATEGORY_EXTERN(LogCombatCharacter, Log, All);

/** Broadcast when the player runs out of health. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatPlayerDied);

/** Existing player attack variants exposed for gameplay-flow confirmation. */
UENUM(BlueprintType)
enum class ECombatPlayerAttackType : uint8
{
	None,
	Light,
	Heavy
};

/** Broadcast only after the normal stationary dodge has actually started. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatDodgeStarted);

/**
 *  An enhanced Third Person Character with melee combat capabilities:
 *  - Combo attack string
 *  - Press and hold charged attack
 *  - Damage dealing and reaction
 *  - Death
 *  - Respawning
 */
UCLASS(abstract)
class ACombatCharacter : public ACharacter, public ICombatAttacker, public ICombatDamageable
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;

	/** Life bar widget component */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UWidgetComponent* LifeBar;
	
protected:

	/** Locks the character to its starting transform and uses a fixed side-view camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stationary Combat")
	bool bStationaryCombatMode = true;

	/** Distance between the stationary player and the side-view camera. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stationary Combat|Camera", meta = (ClampMin = 100, ClampMax = 5000, Units = "cm"))
	float SideViewCameraDistance = 1000.0f;

	/** Height above the player at which the side-view camera aims. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stationary Combat|Camera")
	FVector SideViewCameraTargetOffset = FVector(0);

	/** Rotation of the fixed camera boom. Yaw -90 looks across the X/Z combat plane. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stationary Combat|Camera")
	FRotator SideViewCameraRotation = FRotator(0.0f, -90.0f, 0.0f);

	/** If true, the original combat-template respawn behavior remains enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Stationary Combat|Game Over")
	bool bRespawnAfterDeath = false;

	/** World transform the player is locked to during stationary combat. */
	FTransform StationaryCombatTransform;

	/** True after the stationary transform has been captured during BeginPlay. */
	bool bHasStationaryCombatTransform = false;

	/** Jump Input Action. Used as backward dodge input in stationary combat. */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Combo Attack Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ComboAttackAction;

	/** Charged Attack Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ChargedAttackAction;

	/** Toggle Camera Side Input Action */
	UPROPERTY(EditAnywhere, Category ="Input")
	UInputAction* ToggleCameraAction;

	/** Existing mannequin dash montage reused as the stationary back-dodge action. */
	UPROPERTY(EditAnywhere, Category="Dodge")
	UAnimMontage* DodgeMontage;

	/** Maximum distance from the stationary anchor reached by a back dodge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dodge|Movement", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "cm"))
	float BackDodgeDistance = 220.0f;

	/** Total duration of the procedural out-and-back dodge movement. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dodge|Timing", meta = (ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float BackDodgeDuration = 0.55f;

	/** Minimum time between accepted dodge inputs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dodge|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float BackDodgeCooldown = 0.8f;

	/** Invulnerability window measured from the accepted dodge input. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dodge|Timing", meta = (ClampMin = "0.0", UIMin = "0.0", Units = "s"))
	float BackDodgeInvulnerabilityTime = 0.4f;

	/** Max amount of HP the character will have on respawn */
	UPROPERTY(EditAnywhere, Category="Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MaxHP = 5.0f;

	/** Current amount of HP the character has */
	UPROPERTY(VisibleAnywhere, Category="Damage")
	float CurrentHP = 0.0f;

	/** Life bar widget fill color */
	UPROPERTY(EditAnywhere, Category="Damage")
	FLinearColor LifeBarColor;

	/** Name of the pelvis bone, for damage ragdoll physics */
	UPROPERTY(EditAnywhere, Category="Damage")
	FName PelvisBoneName;

	/** Pointer to the life bar widget */
	UPROPERTY(EditAnywhere, Category="Damage")
	TObjectPtr<UCombatLifeBar> LifeBarWidget;

	/** Max amount of time that may elapse for a non-combo attack input to not be considered stale */
	UPROPERTY(EditAnywhere, Category="Melee Attack", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float AttackInputCacheTimeTolerance = 1.0f;

	/** Time at which an attack button was last pressed */
	float CachedAttackInputTime = -1000.0f;

	/** If true, the character is currently playing an attack animation */
	bool bIsAttacking = false;

	/** Attack type currently being resolved by the existing combat implementation. */
	ECombatPlayerAttackType ActiveAttackType = ECombatPlayerAttackType::None;

	/** Runtime input permissions set by the run/FTUE state machine. */
	bool bAllowLightAttackInput = true;
	bool bAllowHeavyAttackInput = true;
	bool bAllowDodgeInput = true;

	/** Distance ahead of the character that melee attack sphere collision traces will extend */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units="cm"))
	float MeleeTraceDistance = 75.0f;

	/** Radius of the sphere trace for melee attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float MeleeTraceRadius = 75.0f;

	/** Distance ahead of the character that enemies will be notified of incoming attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 500, Units="cm"))
	float DangerTraceDistance = 300.0f;

	/** Radius of the sphere trace to notify enemies of incoming attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Trace", meta = (ClampMin = 0, ClampMax = 200, Units = "cm"))
	float DangerTraceRadius = 100.0f;

	/** Amount of damage a melee attack will deal */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 100))
	float MeleeDamage = 1.0f;

	/** Amount of knockback impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeKnockbackImpulse = 250.0f;

	/** Amount of upwards impulse a melee attack will apply */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Damage", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm/s"))
	float MeleeLaunchImpulse = 300.0f;

	/** AnimMontage that will play for combo attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	UAnimMontage* ComboAttackMontage;

	/** Names of the AnimMontage sections that correspond to each stage of the combo attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo")
	TArray<FName> ComboSectionNames;

	/** Max amount of time that may elapse for a combo attack input to not be considered stale */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Combo", meta = (ClampMin = 0, ClampMax = 5, Units = "s"))
	float ComboInputCacheTimeTolerance = 0.45f;

	/** Index of the current stage of the melee attack combo */
	int32 ComboCount = 0;

	/** AnimMontage that will play for charged attacks */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	UAnimMontage* ChargedAttackMontage;

	/** Name of the AnimMontage section that corresponds to the charge loop */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	FName ChargeLoopSection;

	/** Name of the AnimMontage section that corresponds to the attack */
	UPROPERTY(EditAnywhere, Category="Melee Attack|Charged")
	FName ChargeAttackSection;

	/** Flag that determines if the player is currently holding the charged attack input */
	bool bIsChargingAttack = false;
	
	/** If true, the charged attack hold check has been tested at least once */
	bool bHasLoopedChargedAttack = false;

	/** If true, the character wants to release and resolve the charged attack. */
	bool bHasReleasedChargedAttack = false;

	/** Camera boom length while the character is dead */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float DeathCameraDistance = 400.0f;

	/** Camera boom length when the character respawns */
	UPROPERTY(EditAnywhere, Category="Camera", meta = (ClampMin = 0, ClampMax = 1000, Units = "cm"))
	float DefaultCameraDistance = 100.0f;

	/** Time to wait before respawning the character */
	UPROPERTY(EditAnywhere, Category="Respawn", meta = (ClampMin = 0, ClampMax = 10, Units = "s"))
	float RespawnTime = 3.0f;

	/** Attack montage ended delegate */
	FOnMontageEnded OnAttackMontageEnded;

	/** Dodge montage ended delegate */
	FOnMontageEnded OnDodgeMontageEnded;

	/** Character respawn timer */
	FTimerHandle RespawnTimer;

	/** Copy of the mesh's transform so we can reset it after ragdoll animations */
	FTransform MeshStartingTransform;

public:

	/** Broadcast once when the player dies. Endless enemy spawners use this to stop. */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnCombatPlayerDied OnPlayerDied;

	/** Lets the FTUE confirm that the real dodge implementation started. */
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnCombatDodgeStarted OnDodgeStarted;
	
	/** Constructor */
	ACombatCharacter();

	/** Keeps the player anchored even when an attack montage contains root motion. */
	virtual void Tick(float DeltaSeconds) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Called for combo attack input */
	void ComboAttackPressed();

	/** Called for combo attack input pressed */
	void ChargedAttackPressed();

	/** Called for combo attack input released */
	void ChargedAttackReleased();

	/** Called for toggle camera side input */
	void ToggleCamera();

	/** Called for backward dodge input. */
	void BackDodgePressed();

	/** BP hook to animate the camera side switch */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void BP_ToggleCamera();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles combo attack pressed from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoComboAttackStart();

	/** Handles combo attack released from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoComboAttackEnd();

	/** Handles charged attack pressed from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoChargedAttackStart();

	/** Handles charged attack released from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoChargedAttackEnd();

	/** Starts the stationary backward dodge. */
	void DoBackDodge();

	/** Restricts existing actions without creating tutorial-specific input paths. */
	void SetCombatInputPermissions(bool bAllowLightAttack, bool bAllowHeavyAttack, bool bAllowDodge, bool bCancelDisallowedAction = true);

	/** Returns the normal attack variant currently resolving damage. */
	ECombatPlayerAttackType GetActiveAttackType() const { return ActiveAttackType; }

	/** Existing Enhanced Input actions used to build binding-aware prompts. */
	const UInputAction* GetLightAttackAction() const { return ComboAttackAction; }
	const UInputAction* GetHeavyAttackAction() const { return ChargedAttackAction; }
	const UInputAction* GetDodgeAction() const { return JumpAction; }

protected:

	/** Resets the character's current HP to maximum */
	void ResetHP();

	/** Performs a combo attack */
	void ComboAttack();

	/** Performs a charged attack */
	void ChargedAttack();

	/** Called from a delegate when the attack montage ends */
	void AttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Finishes the dodge when its visual montage completes or is interrupted. */
	void DodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	/** Cancels attack montages and clears all buffered attack state. */
	void CancelAttacks();

	/** Restores the skeletal mesh to animation-only control after a nonlethal hit. */
	void RestoreAnimationAfterHit();

	/** Advances or completes the procedural backward dodge. */
	void UpdateBackDodge();
	void FinishBackDodge();

	/** Returns whether damage should be ignored during the current dodge. */
	bool IsDodgeInvulnerable() const;
	bool IsBackDodgeActive() const;

	
public:

	// ~begin CombatAttacker interface

	/** Performs the collision check for an attack */
	virtual void DoAttackTrace(FName DamageSourceBone) override;

	/** Performs the combo string check */
	virtual void CheckCombo() override;

	/** Performs the charged attack hold check */
	virtual void CheckChargedAttack() override;

	/** Loops or resolves the charged attack animation */
	void LoopOrResolveChargedAttack();

	// ~end CombatAttacker interface

	// ~begin CombatDamageable interface

	/** Notifies nearby enemies that an attack is coming so they can react */
	void NotifyEnemiesOfIncomingAttack();

	/** Handles damage and knockback events */
	virtual void ApplyDamage(float Damage, AActor* DamageCauser, const FVector& DamageLocation, const FVector& DamageImpulse) override;

	/** Handles death events */
	virtual void HandleDeath() override;

	/** Handles healing events */
	virtual void ApplyHealing(float Healing, AActor* Healer) override;

	/** Allows reaction to incoming attacks */
	virtual void NotifyDanger(const FVector& DangerLocation, AActor* DangerSource) override;

	// ~end CombatDamageable interface

	/** Called from the respawn timer to destroy and re-create the character */
	void RespawnCharacter();

public:

	/** Overrides the default TakeDamage functionality */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	/** Overrides landing to reset damage ragdoll physics */
	virtual void Landed(const FHitResult& Hit) override;

protected:

	/** Blueprint handler to play damage dealt effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void DealtDamage(float Damage, const FVector& ImpactPoint);

	/** Blueprint handler to play damage received effects */
	UFUNCTION(BlueprintImplementableEvent, Category="Combat")
	void ReceivedDamage(float Damage, const FVector& ImpactPoint, const FVector& DamageDirection);

protected:

	/** Initialization */
	virtual void BeginPlay() override;

	/** Cleanup */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Handles input bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	/** Handles possessed initialization */
	virtual void NotifyControllerChanged() override;

public:

	/** Returns whether the player can still participate in combat. */
	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsAlive() const { return CurrentHP > 0.0f; }

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};
