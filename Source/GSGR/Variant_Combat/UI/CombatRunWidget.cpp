// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatRunWidget.h"
#include "Variant_Combat/CombatPlayerController.h"
#include "Variant_Combat/CombatCharacter.h"
#include "Variant_Combat/CombatGameMode.h"
#include "Variant_Combat/AI/CombatEnemy.h"
#include "EngineUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "HAL/PlatformTime.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void ConfigureText(UTextBlock* Text, int32 Size, const FLinearColor& Color)
	{
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetColorAndOpacity(FSlateColor(Color));
		Text->SetJustification(ETextJustify::Center);
		Text->SetAutoWrapText(true);
	}

	void AddVerticalSpacer(UWidgetTree* WidgetTree, UVerticalBox* Box, float Height)
	{
		USpacer* Spacer = WidgetTree->ConstructWidget<USpacer>();
		Spacer->SetSize(FVector2D(1.0f, Height));
		Box->AddChildToVerticalBox(Spacer);
	}

	UButton* AddMenuButton(UWidgetTree* WidgetTree, UVerticalBox* Box, const FText& Label)
	{
		UButton* Button = WidgetTree->ConstructWidget<UButton>();
		UTextBlock* ButtonText = WidgetTree->ConstructWidget<UTextBlock>();
		ConfigureText(ButtonText, 24, FLinearColor::White);
		ButtonText->SetText(Label);
		Button->SetContent(ButtonText);
		UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Button);
		Slot->SetPadding(FMargin(28.0f, 8.0f));
		Slot->SetHorizontalAlignment(HAlign_Fill);
		return Button;
	}
}

UCombatRunWidget::UCombatRunWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> FlurryScreenshot(
		TEXT("/Game/Variant_Combat/UI/T_FlurryReminder.T_FlurryReminder"));
	if (FlurryScreenshot.Succeeded())
	{
		ReminderTexture = FlurryScreenshot.Object;
	}
}

void UCombatRunWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree->RootWidget)
	{
		BuildWidgetTree();
	}

	PlayButton->OnClicked.AddUniqueDynamic(this, &UCombatRunWidget::HandlePlayClicked);
	RestartButton->OnClicked.AddUniqueDynamic(this, &UCombatRunWidget::HandleRestartClicked);
	QuitButton->OnClicked.AddUniqueDynamic(this, &UCombatRunWidget::HandleQuitClicked);
	ReturnButton->OnClicked.AddUniqueDynamic(this, &UCombatRunWidget::HandleReturnClicked);
	ReminderCloseButton->OnClicked.AddUniqueDynamic(this, &UCombatRunWidget::HandleReminderCloseClicked);
	HideOverlay();
}

void UCombatRunWidget::BuildWidgetTree()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RunFlowRoot"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	BackgroundBlur = WidgetTree->ConstructWidget<UBackgroundBlur>();
	BackgroundBlur->SetBlurStrength(20.0f);
	UOverlaySlot* BlurSlot = Root->AddChildToOverlay(BackgroundBlur);
	BlurSlot->SetHorizontalAlignment(HAlign_Fill);
	BlurSlot->SetVerticalAlignment(VAlign_Fill);

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
	PanelSize->SetWidthOverride(580.0f);
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelSize);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	MessagePanel = WidgetTree->ConstructWidget<UBorder>();
	MessagePanel->SetPadding(FMargin(38.0f, 30.0f));
	MessagePanel->SetBrushColor(FLinearColor(0.018f, 0.025f, 0.045f, 0.94f));
	PanelSize->SetContent(MessagePanel);

	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	MessagePanel->SetContent(Content);

	HeadingText = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureText(HeadingText, 46, FLinearColor(0.93f, 0.78f, 0.25f, 1.0f));
	Content->AddChildToVerticalBox(HeadingText)->SetHorizontalAlignment(HAlign_Fill);

	AddVerticalSpacer(WidgetTree, Content, 14.0f);

	MessageText = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureText(MessageText, 25, FLinearColor::White);
	Content->AddChildToVerticalBox(MessageText)->SetHorizontalAlignment(HAlign_Fill);

	ScoreText = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureText(ScoreText, 30, FLinearColor(0.7f, 0.9f, 1.0f, 1.0f));
	UVerticalBoxSlot* ScoreSlot = Content->AddChildToVerticalBox(ScoreText);
	ScoreSlot->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 8.0f));
	ScoreSlot->SetHorizontalAlignment(HAlign_Fill);

	PlayButton = AddMenuButton(WidgetTree, Content, NSLOCTEXT("CombatRunUI", "Play", "Play"));
	RestartButton = AddMenuButton(WidgetTree, Content, NSLOCTEXT("CombatRunUI", "Restart", "Restart"));
	QuitButton = AddMenuButton(WidgetTree, Content, NSLOCTEXT("CombatRunUI", "Quit", "Quit"));
	ReturnButton = AddMenuButton(WidgetTree, Content, NSLOCTEXT("CombatRunUI", "Return", "Return to Combat"));
	ReturnButton->SetVisibility(ESlateVisibility::Collapsed);

	USizeBox* ReminderSize = WidgetTree->ConstructWidget<USizeBox>();
	ReminderSize->SetWidthOverride(360.0f);
	UOverlaySlot* ReminderSlot = Root->AddChildToOverlay(ReminderSize);
	ReminderSlot->SetHorizontalAlignment(HAlign_Left);
	ReminderSlot->SetVerticalAlignment(VAlign_Center);
	ReminderSlot->SetPadding(FMargin(16.0f, 0.0f));
	ReminderPanel = WidgetTree->ConstructWidget<UBorder>();
	ReminderPanel->SetPadding(FMargin(20.0f));
	ReminderPanel->SetBrushColor(FLinearColor(0.04f, 0.08f, 0.16f, 0.96f));
	ReminderSize->SetContent(ReminderPanel);
	UVerticalBox* ReminderContent = WidgetTree->ConstructWidget<UVerticalBox>();
	ReminderPanel->SetContent(ReminderContent);
	ReminderCloseButton = AddMenuButton(WidgetTree, ReminderContent, FText::FromString(TEXT("X")));
	if (UVerticalBoxSlot* CloseSlot = Cast<UVerticalBoxSlot>(ReminderCloseButton->Slot))
	{
		CloseSlot->SetPadding(FMargin(220.0f, 0.0f, 0.0f, 6.0f));
	}
	UTextBlock* ReminderHeading = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureText(ReminderHeading, 34, FLinearColor(0.93f, 0.78f, 0.25f));
	ReminderHeading->SetText(NSLOCTEXT("FullFlow", "ReminderHeading", "EVADE"));
	ReminderContent->AddChildToVerticalBox(ReminderHeading);
	if (ReminderTexture)
	{
		USizeBox* ScreenshotSize = WidgetTree->ConstructWidget<USizeBox>();
		ScreenshotSize->SetWidthOverride(320.0f);
		ScreenshotSize->SetHeightOverride(190.0f);
		UBorder* ScreenshotFrame = WidgetTree->ConstructWidget<UBorder>();
		ScreenshotFrame->SetPadding(FMargin(2.0f));
		ScreenshotFrame->SetBrushColor(FLinearColor(0.93f, 0.78f, 0.25f));
		ScreenshotSize->SetContent(ScreenshotFrame);
		UImage* Screenshot = WidgetTree->ConstructWidget<UImage>();
		Screenshot->SetBrushFromTexture(ReminderTexture);
		FSlateBrush ScreenshotBrush = Screenshot->GetBrush();
		ScreenshotBrush.SetUVRegion(FBox2f(FVector2f(0.24f, 0.44f), FVector2f(0.535f, 0.74f)));
		Screenshot->SetBrush(ScreenshotBrush);
		ScreenshotFrame->SetContent(Screenshot);
		UVerticalBoxSlot* ScreenshotSlot = ReminderContent->AddChildToVerticalBox(ScreenshotSize);
		ScreenshotSlot->SetPadding(FMargin(0.0f, 8.0f, 0.0f, 12.0f));
		ScreenshotSlot->SetHorizontalAlignment(HAlign_Center);
	}
	UTextBlock* ReminderBody = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureText(ReminderBody, 22, FLinearColor::White);
	ReminderBody->SetText(NSLOCTEXT("FullFlow", "ReminderBody", "Flurry incoming!\nHold F / Left Shoulder through the hits.\nRelease to counter."));
	ReminderContent->AddChildToVerticalBox(ReminderBody);
	ReminderPanel->SetVisibility(ESlateVisibility::Collapsed);

	CombatCueText = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureText(CombatCueText, 26, FLinearColor::White);
	UOverlaySlot* CueSlot = Root->AddChildToOverlay(CombatCueText);
	CueSlot->SetHorizontalAlignment(HAlign_Fill);
	CueSlot->SetVerticalAlignment(VAlign_Top);
	CueSlot->SetPadding(FMargin(20.0f, 60.0f));
	EvadeStatusText = WidgetTree->ConstructWidget<UTextBlock>();
	ConfigureText(EvadeStatusText, 22, FLinearColor::White);
	UOverlaySlot* EvadeSlot = Root->AddChildToOverlay(EvadeStatusText);
	EvadeSlot->SetHorizontalAlignment(HAlign_Fill);
	EvadeSlot->SetVerticalAlignment(VAlign_Bottom);
	EvadeSlot->SetPadding(FMargin(20.0f, 32.0f));
}

void UCombatRunWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (bReminderVisible)
	{
		const double Elapsed = FPlatformTime::Seconds() - ReminderAnimationStart;
		const float Alpha = FMath::Clamp(static_cast<float>(Elapsed / 0.3), 0.0f, 1.0f);
		ReminderPanel->SetRenderTranslation(FVector2D(bReminderClosing ? -380.0f * Alpha : -380.0f * (1.0f - Alpha), 0.0f));
		if (bReminderClosing && Alpha >= 1.0f)
		{
			if (ACombatPlayerController* Owner = Cast<ACombatPlayerController>(GetOwningPlayer()))
			{
				Owner->HandleReminderAnimationFinished();
			}
		}
	}
	const ACombatGameMode* Mode = GetWorld()->GetAuthGameMode<ACombatGameMode>();
	const ACombatCharacter* Player = Cast<ACombatCharacter>(GetOwningPlayerPawn());
	const ACombatPlayerController* Controller = Cast<ACombatPlayerController>(GetOwningPlayer());
	const bool bShowCombat = Mode && Mode->IsRunActive() && Player && Player->IsAlive();
	CombatCueText->SetVisibility(bShowCombat ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	EvadeStatusText->SetVisibility(bShowCombat ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bShowCombat) return;

	const FText Binding = Controller ? Controller->GetBindingDisplayText(Player->GetEvadeAction()) : FText::GetEmpty();
	EvadeStatusText->SetText(Player->IsEvading()
		? (Player->HasJustEvadedHit() ? NSLOCTEXT("Evade", "Hit", "EVADED!") : NSLOCTEXT("Evade", "Hold", "EVADING - release to attack"))
		: FText::Format(NSLOCTEXT("Evade", "Hint", "Hold {0} to evade"), Binding));
	EvadeStatusText->SetColorAndOpacity(Player->IsEvading() ? FLinearColor(0.2f, 0.85f, 1.0f) : FLinearColor::White);

	const ACombatEnemy* NearestEnemy = nullptr;
	double NearestDistance = TNumericLimits<double>::Max();
	for (TActorIterator<ACombatEnemy> It(GetWorld()); It; ++It)
	{
		if (!It->IsAlive() || !It->IsFlurryEnemy()) continue;
		const double Distance = FVector::DistSquared(It->GetActorLocation(), Player->GetActorLocation());
		if (Distance < NearestDistance) { NearestEnemy = *It; NearestDistance = Distance; }
	}
	CombatCueText->SetText(NearestEnemy ? NearestEnemy->GetCombatCue() : FText::GetEmpty());
	CombatCueText->SetColorAndOpacity(NearestEnemy && NearestEnemy->GetFlurryState() == ECombatFlurryState::Exhausted
		? FLinearColor(0.3f, 1.0f, 0.35f) : FLinearColor(1.0f, 0.65f, 0.3f));
}

void UCombatRunWidget::ShowStartupMenu()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BackgroundBlur->SetVisibility(ESlateVisibility::Visible);
	MessagePanel->SetVisibility(ESlateVisibility::Visible);
	HeadingText->SetText(NSLOCTEXT("CombatRunUI", "GameTitle", "GSGR"));
	MessageText->SetText(FText::GetEmpty());
	ScoreText->SetVisibility(ESlateVisibility::Collapsed);
	PlayButton->SetVisibility(ESlateVisibility::Visible);
	RestartButton->SetVisibility(ESlateVisibility::Collapsed);
	QuitButton->SetVisibility(ESlateVisibility::Collapsed);
	ReturnButton->SetVisibility(ESlateVisibility::Collapsed);
	SetMenuInteractionEnabled(true);
}

void UCombatRunWidget::ShowTutorialMessage(const FText& Heading, const FText& Message)
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BackgroundBlur->SetVisibility(ESlateVisibility::Collapsed);
	MessagePanel->SetVisibility(ESlateVisibility::HitTestInvisible);
	HeadingText->SetText(Heading);
	MessageText->SetText(Message);
	ScoreText->SetVisibility(ESlateVisibility::Collapsed);
	PlayButton->SetVisibility(ESlateVisibility::Collapsed);
	RestartButton->SetVisibility(ESlateVisibility::Collapsed);
	QuitButton->SetVisibility(ESlateVisibility::Collapsed);
	ReturnButton->SetVisibility(ESlateVisibility::Collapsed);
	SetMenuInteractionEnabled(false);
}

void UCombatRunWidget::ShowTutorialComplete()
{
	ShowTutorialMessage(
		NSLOCTEXT("CombatRunUI", "TutorialCompleteHeading", "Tutorial Complete"),
		NSLOCTEXT("CombatRunUI", "TutorialCompleteMessage", "Survive as long as you can."));
}

void UCombatRunWidget::ShowGameOver(float FinalSurvivalTime)
{
	const int32 TotalHundredths = FMath::Max(0, FMath::RoundToInt(FinalSurvivalTime * 100.0f));
	const int32 Minutes = TotalHundredths / 6000;
	const int32 Seconds = (TotalHundredths / 100) % 60;
	const int32 Hundredths = TotalHundredths % 100;

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BackgroundBlur->SetVisibility(ESlateVisibility::Visible);
	MessagePanel->SetVisibility(ESlateVisibility::Visible);
	HeadingText->SetText(NSLOCTEXT("CombatRunUI", "GameOver", "Game Over"));
	MessageText->SetText(NSLOCTEXT("CombatRunUI", "SurvivalTime", "Survival Time"));
	ScoreText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d.%02d"), Minutes, Seconds, Hundredths)));
	ScoreText->SetVisibility(ESlateVisibility::Visible);
	PlayButton->SetVisibility(ESlateVisibility::Collapsed);
	RestartButton->SetVisibility(ESlateVisibility::Visible);
	QuitButton->SetVisibility(ESlateVisibility::Visible);
	ReturnButton->SetVisibility(ESlateVisibility::Collapsed);
	SetMenuInteractionEnabled(true);
}

void UCombatRunWidget::HideOverlay()
{
	BackgroundBlur->SetVisibility(ESlateVisibility::Collapsed);
	MessagePanel->SetVisibility(ESlateVisibility::Collapsed);
	SetMenuInteractionEnabled(false);
}

void UCombatRunWidget::ShowShowcase()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	BackgroundBlur->SetVisibility(ESlateVisibility::Visible);
	MessagePanel->SetVisibility(ESlateVisibility::Visible);
	HeadingText->SetText(NSLOCTEXT("FullFlow", "ShowcaseHeading", "Control Recap Showcase"));
	MessageText->SetText(NSLOCTEXT("FullFlow", "ShowcaseBody",
		"Light Attack: Left Mouse\nHeavy Attack: Right Mouse\nDodge: Space\nEvade: Hold F\n\nPlaceholder showcase - return for a live flurry encounter."));
	ScoreText->SetVisibility(ESlateVisibility::Collapsed);
	PlayButton->SetVisibility(ESlateVisibility::Collapsed);
	RestartButton->SetVisibility(ESlateVisibility::Collapsed);
	QuitButton->SetVisibility(ESlateVisibility::Collapsed);
	ReturnButton->SetVisibility(ESlateVisibility::Visible);
	ReturnButton->SetIsEnabled(true);
}

void UCombatRunWidget::ShowEvadeReminder()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	bReminderVisible = true;
	bReminderClosing = false;
	ReminderAnimationStart = FPlatformTime::Seconds();
	ReminderPanel->SetRenderTranslation(FVector2D(-380.0f, 0.0f));
	ReminderPanel->SetVisibility(ESlateVisibility::Visible);
	ReminderCloseButton->SetIsEnabled(true);
}

void UCombatRunWidget::BeginHideEvadeReminder()
{
	if (!bReminderVisible || bReminderClosing) return;
	bReminderClosing = true;
	ReminderAnimationStart = FPlatformTime::Seconds();
	ReminderCloseButton->SetIsEnabled(false);
}

void UCombatRunWidget::HideEvadeReminder()
{
	bReminderVisible = false;
	bReminderClosing = false;
	ReminderPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UCombatRunWidget::SetMenuInteractionEnabled(bool bEnabled)
{
	PlayButton->SetIsEnabled(bEnabled);
	RestartButton->SetIsEnabled(bEnabled);
	QuitButton->SetIsEnabled(bEnabled);
}

void UCombatRunWidget::HandlePlayClicked()
{
	if (ACombatPlayerController* Controller = Cast<ACombatPlayerController>(GetOwningPlayer()))
	{
		Controller->HandlePlaySelected();
	}
}

void UCombatRunWidget::HandleRestartClicked()
{
	if (ACombatPlayerController* Controller = Cast<ACombatPlayerController>(GetOwningPlayer()))
	{
		Controller->HandleRestartSelected();
	}
}

void UCombatRunWidget::HandleQuitClicked()
{
	if (ACombatPlayerController* Controller = Cast<ACombatPlayerController>(GetOwningPlayer()))
	{
		Controller->HandleQuitSelected();
	}
}

void UCombatRunWidget::HandleReturnClicked()
{
	if (ACombatPlayerController* Controller = Cast<ACombatPlayerController>(GetOwningPlayer()))
	{
		Controller->HandleShowcaseReturnSelected();
	}
}

void UCombatRunWidget::HandleReminderCloseClicked()
{
	if (ACombatPlayerController* Controller = Cast<ACombatPlayerController>(GetOwningPlayer()))
	{
		Controller->HandleReminderCloseSelected();
	}
}
