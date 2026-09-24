// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatMainMenuWidget.h"
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

void UCombatMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree->RootWidget) BuildDefaultLayout();
	if (!PlayButton) PlayButton = Cast<UButton>(WidgetTree->FindWidget(TEXT("PlayButton")));
	if (PlayButton) PlayButton->OnClicked.AddUniqueDynamic(this, &UCombatMainMenuWidget::HandlePlayClicked);
}

void UCombatMainMenuWidget::BuildDefaultLayout()
{
	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MainMenuRoot"));
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	WidgetTree->RootWidget = Root;

	UBackgroundBlur* Blur = WidgetTree->ConstructWidget<UBackgroundBlur>();
	Blur->SetBlurStrength(20.0f);
	Root->AddChildToOverlay(Blur);

	USizeBox* PanelSize = WidgetTree->ConstructWidget<USizeBox>();
	PanelSize->SetWidthOverride(580.0f);
	UOverlaySlot* PanelSlot = Root->AddChildToOverlay(PanelSize);
	PanelSlot->SetHorizontalAlignment(HAlign_Center);
	PanelSlot->SetVerticalAlignment(VAlign_Center);

	UBorder* Panel = WidgetTree->ConstructWidget<UBorder>();
	Panel->SetPadding(FMargin(38.0f, 30.0f));
	Panel->SetBrushColor(FLinearColor(0.018f, 0.025f, 0.045f, 0.94f));
	PanelSize->SetContent(Panel);
	UVerticalBox* Content = WidgetTree->ConstructWidget<UVerticalBox>();
	Panel->SetContent(Content);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
	Title->SetText(NSLOCTEXT("CombatMainMenu", "Title", "GSGR"));
	FSlateFontInfo TitleFont = Title->GetFont();
	TitleFont.Size = 46;
	Title->SetFont(TitleFont);
	Title->SetColorAndOpacity(FSlateColor(FLinearColor(0.93f, 0.78f, 0.25f)));
	Title->SetJustification(ETextJustify::Center);
	Content->AddChildToVerticalBox(Title);

	USpacer* Gap = WidgetTree->ConstructWidget<USpacer>();
	Gap->SetSize(FVector2D(1.0f, 14.0f));
	Content->AddChildToVerticalBox(Gap);

	PlayButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("PlayButton"));
	UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>();
	Label->SetText(NSLOCTEXT("CombatMainMenu", "Play", "Play"));
	FSlateFontInfo LabelFont = Label->GetFont();
	LabelFont.Size = 24;
	Label->SetFont(LabelFont);
	Label->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	Label->SetJustification(ETextJustify::Center);
	PlayButton->SetContent(Label);
	UVerticalBoxSlot* ButtonSlot = Content->AddChildToVerticalBox(PlayButton);
	ButtonSlot->SetPadding(FMargin(28.0f, 8.0f));
	ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
}

void UCombatMainMenuWidget::HandlePlayClicked()
{
	if (ACombatPlayerController* Controller = Cast<ACombatPlayerController>(GetOwningPlayer()))
		Controller->HandlePlaySelected();
}
