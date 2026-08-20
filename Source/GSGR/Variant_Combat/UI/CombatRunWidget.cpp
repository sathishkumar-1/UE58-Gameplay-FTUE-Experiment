// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatRunWidget.h"
#include "Variant_Combat/CombatPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/BackgroundBlur.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

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
	SetMenuInteractionEnabled(true);
}

void UCombatRunWidget::HideOverlay()
{
	BackgroundBlur->SetVisibility(ESlateVisibility::Collapsed);
	MessagePanel->SetVisibility(ESlateVisibility::Collapsed);
	SetMenuInteractionEnabled(false);
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
