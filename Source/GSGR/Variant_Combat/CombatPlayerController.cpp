// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Combat/CombatPlayerController.h"
#include "Variant_Combat/CombatGameMode.h"
#include "Variant_Combat/UI/CombatRunWidget.h"
#include "Variant_Combat/UI/CombatMainMenuWidget.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
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
#include "InputCoreTypes.h"

void ACombatPlayerController::BeginPlay()
{
	Super::BeginPlay();
	EnsureRunFlowWidget();
}

void ACombatPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsLocalPlayerController())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (DemoShortcutsMappingContext) Subsystem->RemoveMappingContext(DemoShortcutsMappingContext);
			if (DemoReminderMappingContext) Subsystem->RemoveMappingContext(DemoReminderMappingContext);
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ACombatPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		if (DemoSkipAction) EnhancedInput->BindAction(DemoSkipAction, ETriggerEvent::Started, this, &ACombatPlayerController::HandleDemoSkipAction);
		if (DemoShowcaseAction) EnhancedInput->BindAction(DemoShowcaseAction, ETriggerEvent::Started, this, &ACombatPlayerController::HandleDemoShowcaseAction);
		if (DemoPostShowcaseAction) EnhancedInput->BindAction(DemoPostShowcaseAction, ETriggerEvent::Started, this, &ACombatPlayerController::HandleDemoPostShowcaseAction);
		if (DismissReminderAction) EnhancedInput->BindAction(DismissReminderAction, ETriggerEvent::Started, this, &ACombatPlayerController::HandleDismissReminderAction);
	}

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

void ACombatPlayerController::SetDemoShortcutsActive(bool bActive)
{
	bDemoShortcutsActive = bActive;
	if (!IsLocalPlayerController()) return;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DemoShortcutsMappingContext)
		{
			Subsystem->RemoveMappingContext(DemoShortcutsMappingContext);
			if (bActive && !bReminderInputActive) Subsystem->AddMappingContext(DemoShortcutsMappingContext, 10);
		}
	}
}

void ACombatPlayerController::SetReminderInputActive(bool bActive)
{
	bReminderInputActive = bActive;
	if (!IsLocalPlayerController()) return;
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DemoShortcutsMappingContext) Subsystem->RemoveMappingContext(DemoShortcutsMappingContext);
		if (DemoReminderMappingContext)
		{
			Subsystem->RemoveMappingContext(DemoReminderMappingContext);
			if (bActive) Subsystem->AddMappingContext(DemoReminderMappingContext, 20);
		}
		if (!bActive && bDemoShortcutsActive && DemoShortcutsMappingContext)
			Subsystem->AddMappingContext(DemoShortcutsMappingContext, 10);
	}
}

void ACombatPlayerController::HandleDemoSkipAction()
{
	if (ACombatGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatGameMode>()) Mode->HandleDemoSkip();
}

void ACombatPlayerController::HandleDemoShowcaseAction()
{
	if (ACombatGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatGameMode>()) Mode->JumpToShowcase();
}

void ACombatPlayerController::HandleDemoPostShowcaseAction()
{
	if (ACombatGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatGameMode>()) Mode->JumpToPostShowcase();
}

void ACombatPlayerController::HandleDismissReminderAction()
{
	if (ACombatGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatGameMode>()) Mode->RequestCloseEvadeReminder();
}

void ACombatPlayerController::SetRespawnTransform(const FTransform& NewRespawn)
{
	// save the new respawn transform
	RespawnTransform = NewRespawn;
}

void ACombatPlayerController::ShowStartupMenu()
{
	EnsureMainMenuWidget();
	if (!MainMenuWidget) return;
	MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
	SetMenuInputMode(true);
	if (UButton* PlayButton = MainMenuWidget->GetPlayButton())
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

void ACombatPlayerController::ShowShowcase()
{
	EnsureRunFlowWidget();
	RunFlowWidget->ShowShowcase();
	SetMenuInputMode(true);
	if (UButton* ReturnButton = RunFlowWidget->GetReturnButton()) ReturnButton->SetUserFocus(this);
}

void ACombatPlayerController::ShowEvadeReminder()
{
	EnsureRunFlowWidget();
	RunFlowWidget->ShowEvadeReminder();
	SetMenuInputMode(true);
}

void ACombatPlayerController::BeginHideEvadeReminder()
{
	if (RunFlowWidget) RunFlowWidget->BeginHideEvadeReminder();
}

void ACombatPlayerController::HideEvadeReminder()
{
	if (RunFlowWidget) RunFlowWidget->HideEvadeReminder();
	SetMenuInputMode(false);
}

void ACombatPlayerController::HandleReminderAnimationFinished()
{
	if (ACombatGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatGameMode>()) Mode->CloseEvadeReminder();
}

void ACombatPlayerController::HandleShowcaseReturnSelected()
{
	if (ACombatGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatGameMode>()) Mode->HandleShowcaseReturn();
}

void ACombatPlayerController::HandleReminderCloseSelected()
{
	if (ACombatGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatGameMode>()) Mode->RequestCloseEvadeReminder();
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

void ACombatPlayerController::LaunchShowcaseFromMenu()
{
	if (ACombatGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatGameMode>()) Mode->HandleMenuShowcaseSelected();
}

void ACombatPlayerController::LaunchPostShowcaseFromMenu()
{
	if (ACombatGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatGameMode>()) Mode->HandleMenuPostShowcaseSelected();
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

void ACombatPlayerController::EnsureMainMenuWidget()
{
	if (MainMenuWidget || !IsLocalPlayerController()) return;
	if (!MainMenuWidgetClass)
	{
		UE_LOG(LogGSGR, Error, TEXT("MainMenuWidgetClass is not set on the combat player controller."));
		return;
	}
	MainMenuWidget = CreateWidget<UCombatMainMenuWidget>(this, MainMenuWidgetClass);
	if (MainMenuWidget) MainMenuWidget->AddToPlayerScreen(200);
}

void ACombatPlayerController::SetMenuInputMode(bool bMenuActive)
{
	bShowMouseCursor = bMenuActive;
	if (bMenuActive && (MainMenuWidget || RunFlowWidget))
	{
		FInputModeGameAndUI InputMode;
		UUserWidget* FocusWidget = MainMenuWidget && MainMenuWidget->IsVisible()
			? Cast<UUserWidget>(MainMenuWidget) : Cast<UUserWidget>(RunFlowWidget);
		InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
	}
}
