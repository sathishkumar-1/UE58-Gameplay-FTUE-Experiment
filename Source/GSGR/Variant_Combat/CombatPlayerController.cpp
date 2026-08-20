// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Combat/CombatPlayerController.h"
#include "Variant_Combat/CombatGameMode.h"
#include "Variant_Combat/UI/CombatRunWidget.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerStart.h"
#include "CombatCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "GSGR.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "InputKeyEventArgs.h"

void ACombatPlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureRunFlowWidget();
}

void ACombatPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// add the input mapping context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}

	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogGSGR, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void ACombatPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	// subscribe to the pawn's OnDestroyed delegate
	InPawn->OnDestroyed.AddDynamic(this, &ACombatPlayerController::OnPawnDestroyed);
}

bool ACombatPlayerController::InputKey(const FInputKeyEventArgs& Params)
{
	const bool bHandledByGameplay = Super::InputKey(Params);
	if (!bAwaitingWelcomeInput)
	{
		return bHandledByGameplay;
	}

	const bool bIsDeliberateButton = Params.Key.IsValid()
		&& Params.Key.IsDigital()
		&& !Params.Key.IsMouseButton()
		&& !Params.Key.IsTouch()
		&& !Params.Key.IsGesture();

	if (!bWelcomeKeyPressed && Params.Event == IE_Pressed && bIsDeliberateButton)
	{
		bWelcomeKeyPressed = true;
		WelcomeContinuationKey = Params.Key;
		return true;
	}

	if (bWelcomeKeyPressed && Params.Event == IE_Released && Params.Key == WelcomeContinuationKey)
	{
		bAwaitingWelcomeInput = false;
		bWelcomeKeyPressed = false;
		WelcomeContinuationKey = FKey();

		if (ACombatGameMode* CombatGameMode = GetWorld()->GetAuthGameMode<ACombatGameMode>())
		{
			CombatGameMode->HandleWelcomeContinue();
		}
		return true;
	}

	return bHandledByGameplay;
}

void ACombatPlayerController::SetRespawnTransform(const FTransform& NewRespawn)
{
	// save the new respawn transform
	RespawnTransform = NewRespawn;
}

void ACombatPlayerController::ShowStartupMenu()
{
	EnsureRunFlowWidget();
	RunFlowWidget->ShowStartupMenu();
	SetMenuInputMode(true);
	if (UButton* PlayButton = RunFlowWidget->GetPlayButton())
	{
		PlayButton->SetUserFocus(this);
	}
}

void ACombatPlayerController::ShowTutorialMessage(const FText& Heading, const FText& Message)
{
	EnsureRunFlowWidget();
	RunFlowWidget->ShowTutorialMessage(Heading, Message);
	SetMenuInputMode(false);
}

void ACombatPlayerController::ShowTutorialComplete()
{
	EnsureRunFlowWidget();
	RunFlowWidget->ShowTutorialComplete();
	SetMenuInputMode(false);
}

void ACombatPlayerController::ShowGameOver(float FinalSurvivalTime)
{
	EnsureRunFlowWidget();
	RunFlowWidget->ShowGameOver(FinalSurvivalTime);
	SetMenuInputMode(true);
}

void ACombatPlayerController::HideRunFlowUI()
{
	EnsureRunFlowWidget();
	RunFlowWidget->HideOverlay();
	SetMenuInputMode(false);
}

void ACombatPlayerController::SetAwaitingWelcomeInput(bool bAwaiting)
{
	bAwaitingWelcomeInput = bAwaiting;
	bWelcomeKeyPressed = false;
	WelcomeContinuationKey = FKey();
}

FText ACombatPlayerController::GetBindingDisplayText(const UInputAction* Action) const
{
	if (!Action)
	{
		return NSLOCTEXT("CombatRunUI", "Unbound", "Unbound");
	}

	FKey KeyboardOrMouseKey;
	FKey GamepadKey;
	if (const UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		for (const FKey& Key : Subsystem->QueryKeysMappedToAction(Action))
		{
			if (!Key.IsValid() || Key.IsAnalog())
			{
				continue;
			}

			if (Key.IsGamepadKey())
			{
				if (!GamepadKey.IsValid())
				{
					GamepadKey = Key;
				}
			}
			else if (!KeyboardOrMouseKey.IsValid())
			{
				KeyboardOrMouseKey = Key;
			}
		}
	}

	if (KeyboardOrMouseKey.IsValid() && GamepadKey.IsValid())
	{
		return FText::Format(
			NSLOCTEXT("CombatRunUI", "DualBinding", "{0} / {1}"),
			KeyboardOrMouseKey.GetDisplayName(), GamepadKey.GetDisplayName());
	}
	if (KeyboardOrMouseKey.IsValid())
	{
		return KeyboardOrMouseKey.GetDisplayName();
	}
	if (GamepadKey.IsValid())
	{
		return GamepadKey.GetDisplayName();
	}
	return FText::FromName(Action->GetFName());
}

void ACombatPlayerController::HandlePlaySelected()
{
	if (ACombatGameMode* CombatGameMode = GetWorld()->GetAuthGameMode<ACombatGameMode>())
	{
		CombatGameMode->HandlePlaySelected();
	}
}

void ACombatPlayerController::HandleRestartSelected()
{
	if (ACombatGameMode* CombatGameMode = GetWorld()->GetAuthGameMode<ACombatGameMode>())
	{
		CombatGameMode->HandleRestartSelected();
	}
}

void ACombatPlayerController::HandleQuitSelected()
{
	if (ACombatGameMode* CombatGameMode = GetWorld()->GetAuthGameMode<ACombatGameMode>())
	{
		CombatGameMode->HandleQuitSelected();
	}
}

void ACombatPlayerController::OnPawnDestroyed(AActor* DestroyedActor)
{
	// spawn a new character at the respawn transform
	if (ACombatCharacter* RespawnedCharacter = GetWorld()->SpawnActor<ACombatCharacter>(CharacterClass, RespawnTransform))
	{
		// possess the character
		Possess(RespawnedCharacter);
	}
}

bool ACombatPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void ACombatPlayerController::EnsureRunFlowWidget()
{
	if (RunFlowWidget || !IsLocalPlayerController())
	{
		return;
	}

	RunFlowWidget = CreateWidget<UCombatRunWidget>(this, UCombatRunWidget::StaticClass());
	if (RunFlowWidget)
	{
		RunFlowWidget->AddToPlayerScreen(100);
	}
}

void ACombatPlayerController::SetMenuInputMode(bool bMenuActive)
{
	bShowMouseCursor = bMenuActive;
	if (bMenuActive && RunFlowWidget)
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetWidgetToFocus(RunFlowWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}
