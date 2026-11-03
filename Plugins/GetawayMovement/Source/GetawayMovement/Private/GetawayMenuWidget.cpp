#include "GetawayMenuWidget.h"

#include "GetawayPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateColorBrush.h"
#include "Styling/CoreStyle.h"

namespace
{
	/** Amber, for a line that has to read as a problem. The one colour the palette does not own. */
	const FLinearColor Problem(0.94f, 0.70f, 0.24f);

	FString Trimmed(const FText& Text)
	{
		FString S = Text.ToString();
		S.TrimStartAndEndInline();
		return S;
	}
}

TSharedRef<SWidget> UGetawayMenuWidget::RebuildWidget()
{
	BuildPanel();
	return Super::RebuildWidget();
}

AGetawayPlayerController* UGetawayMenuWidget::Owner() const
{
	return Cast<AGetawayPlayerController>(GetOwningPlayer());
}

void UGetawayMenuWidget::BuildPanel()
{
	if (!WidgetTree || WidgetTree->RootWidget) return;

	const bool bPause = (Mode == EGetawayMenuMode::Pause);

	UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// The wash. Its alpha is the whole trick: the level keeps turning behind the panel instead of
	// the menu being a page you go to.
	{
		UImage* Scrim = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Scrim"));
		FLinearColor Wash = ScrimColour;
		if (bPause)
		{
			Wash.A = PauseScrimAlpha;
		}
		Scrim->SetColorAndOpacity(Wash);
		if (UOverlaySlot* Placed = Cast<UOverlaySlot>(Root->AddChild(Scrim)))
		{
			Placed->SetHorizontalAlignment(HAlign_Fill);
			Placed->SetVerticalAlignment(VAlign_Fill);
		}
	}

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Stack"));
	if (UOverlaySlot* Placed = Cast<UOverlaySlot>(Root->AddChild(Stack)))
	{
		Placed->SetHorizontalAlignment(HAlign_Center);
		Placed->SetVerticalAlignment(VAlign_Center);
	}

	// ADRENALINE. The letters are spaced out by hand rather than with a tracking setting, because
	// FSlateFontInfo has LetterSpacing only in 1/1000 em and a word this size reads better split.
	{
		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Title"));
		Title->SetText(FText::FromString(TEXT("A D R E N A L I N E")));
		Title->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), bPause ? PauseTitleSize : TitleSize));
		Title->SetColorAndOpacity(FSlateColor(EdgePink));
		if (UVerticalBoxSlot* Placed = Cast<UVerticalBoxSlot>(Stack->AddChild(Title)))
		{
			Placed->SetHorizontalAlignment(HAlign_Center);
			Placed->SetPadding(FMargin(0.f, 0.f, 0.f, bPause ? 26.f : 48.f));
		}
	}

	// The column of buttons and boxes, held to a fixed width so a long hint does not stretch it.
	USizeBox* Sized = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("PanelWidth"));
	Sized->SetWidthOverride(PanelWidth);
	if (UVerticalBoxSlot* Placed = Cast<UVerticalBoxSlot>(Stack->AddChild(Sized)))
	{
		Placed->SetHorizontalAlignment(HAlign_Center);
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Column"));
	Sized->AddChild(Column);

	if (bPause)
	{
		AddButton(Column, TEXT("RESUME"))->OnClicked.AddDynamic(this, &UGetawayMenuWidget::OnResumeClicked);
		AddButton(Column, TEXT("LEAVE TO MENU"))->OnClicked.AddDynamic(this, &UGetawayMenuWidget::OnLeaveClicked);
		AddButton(Column, TEXT("QUIT"))->OnClicked.AddDynamic(this, &UGetawayMenuWidget::OnQuitClicked);
	}
	else
	{
		// Name first, because it applies to both of the two things under it. Without one the
		// scoreboard calls you "Player 0" - the same ?Name= the two launchers in Tools/ ask for.
		NameBox = AddTextBox(Column, TEXT("Your name"));
		AddButton(Column, TEXT("HOST"))->OnClicked.AddDynamic(this, &UGetawayMenuWidget::OnHostClicked);

		AddressBox = AddTextBox(Column, TEXT("Host's IP address"));
		AddressBox->OnTextCommitted.AddDynamic(this, &UGetawayMenuWidget::OnAddressCommitted);
		AddButton(Column, TEXT("JOIN"))->OnClicked.AddDynamic(this, &UGetawayMenuWidget::OnJoinClicked);

		AddButton(Column, TEXT("QUIT"))->OnClicked.AddDynamic(this, &UGetawayMenuWidget::OnQuitClicked);
	}

	// The status line. Always built, so SetStatus works whenever it is called.
	{
		StatusText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Status"));
		StatusText->SetText(FText::GetEmpty());
		StatusText->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 16.f));
		StatusText->SetColorAndOpacity(FSlateColor(Problem));
		StatusText->SetAutoWrapText(true);
		if (UVerticalBoxSlot* Placed = Cast<UVerticalBoxSlot>(Stack->AddChild(StatusText)))
		{
			Placed->SetHorizontalAlignment(HAlign_Center);
			Placed->SetPadding(FMargin(0.f, 22.f, 0.f, 0.f));
		}
	}
}

UButton* UGetawayMenuWidget::AddButton(UVerticalBox* Column, const FString& Label)
{
	// Pink border, 2 px thick, with the button filling the rest of it. Everything in this game
	// wears its edge; see *How the edge band works* in docs/look.md.
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Frame->SetBrushColor(EdgePink);
	Frame->SetPadding(FMargin(2.f));
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Fill);

	if (UVerticalBoxSlot* Placed = Cast<UVerticalBoxSlot>(Column->AddChild(Frame)))
	{
		Placed->SetPadding(FMargin(0.f, 5.f));
		Placed->SetHorizontalAlignment(HAlign_Fill);
	}

	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());

	FLinearColor Hover = ButtonFill + (EdgePink * HoverStrength);
	Hover.A = 0.95f;
	FLinearColor Press = ButtonFill + (EdgePink * FMath::Min(1.f, HoverStrength * 2.f));
	Press.A = 1.f;

	FButtonStyle Style = Button->GetStyle();
	Style.SetNormal(FSlateColorBrush(ButtonFill))
		.SetHovered(FSlateColorBrush(Hover))
		.SetPressed(FSlateColorBrush(Press))
		.SetDisabled(FSlateColorBrush(FLinearColor(0.04f, 0.04f, 0.04f, 0.9f)))
		.SetNormalPadding(FMargin(14.f, 11.f))
		.SetPressedPadding(FMargin(14.f, 12.f, 14.f, 10.f));
	Button->SetStyle(Style);

	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Text->SetText(FText::FromString(Label));
	Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Bold"), ButtonTextSize));
	Text->SetColorAndOpacity(FSlateColor(EdgePink));
	Button->SetContent(Text);

	Frame->SetContent(Button);
	return Button;
}

UEditableTextBox* UGetawayMenuWidget::AddTextBox(UVerticalBox* Column, const FString& Hint)
{
	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	// Dimmer than a button's frame: a box you type in should not shout louder than the thing it
	// feeds.
	Frame->SetBrushColor(EdgePink * 0.55f);
	Frame->SetPadding(FMargin(2.f));
	Frame->SetHorizontalAlignment(HAlign_Fill);
	Frame->SetVerticalAlignment(VAlign_Fill);

	if (UVerticalBoxSlot* Placed = Cast<UVerticalBoxSlot>(Column->AddChild(Frame)))
	{
		Placed->SetPadding(FMargin(0.f, 5.f));
		Placed->SetHorizontalAlignment(HAlign_Fill);
	}

	UEditableTextBox* Box = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass());
	Box->SetHintText(FText::FromString(Hint));

	// Start from UMG's own style rather than a default-constructed one, so the caret, the
	// selection highlight and the scrollbar keep working, and only repaint what shows.
	FEditableTextBoxStyle Style = Box->GetWidgetStyle();
	Style.SetBackgroundImageNormal(FSlateColorBrush(ButtonFill))
		.SetBackgroundImageHovered(FSlateColorBrush(ButtonFill))
		.SetBackgroundImageFocused(FSlateColorBrush(FLinearColor(0.10f, 0.015f, 0.055f, 0.96f)))
		.SetBackgroundImageReadOnly(FSlateColorBrush(ButtonFill))
		.SetForegroundColor(FSlateColor(FLinearColor::White))
		.SetFocusedForegroundColor(FSlateColor(FLinearColor::White))
		.SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 20.f))
		.SetPadding(FMargin(12.f, 9.f));
	Box->SetWidgetStyle(Style);

	Frame->SetContent(Box);
	return Box;
}

void UGetawayMenuWidget::SetStatus(const FString& Text, bool bIsProblem)
{
	if (!StatusText) return;
	StatusText->SetText(FText::FromString(Text));
	StatusText->SetColorAndOpacity(FSlateColor(bIsProblem ? Problem : EdgePink));
}

FString UGetawayMenuWidget::GetTypedAddress() const
{
	return AddressBox ? Trimmed(AddressBox->GetText()) : FString();
}

FString UGetawayMenuWidget::GetTypedName() const
{
	return NameBox ? Trimmed(NameBox->GetText()) : FString();
}

void UGetawayMenuWidget::SetTypedAddress(const FString& Address)
{
	if (AddressBox)
	{
		AddressBox->SetText(FText::FromString(Address));
	}
}

void UGetawayMenuWidget::SetTypedName(const FString& Name)
{
	if (NameBox)
	{
		NameBox->SetText(FText::FromString(Name));
	}
}

void UGetawayMenuWidget::FocusFirstBox()
{
	// The name box, which is the first thing anybody arriving at this menu has to fill in.
	if (NameBox)
	{
		NameBox->SetKeyboardFocus();
	}
}

void UGetawayMenuWidget::OnHostClicked()
{
	if (AGetawayPlayerController* PC = Owner())
	{
		SetStatus(TEXT("Starting a game..."), false);
		PC->HostGame(GetTypedName());
	}
}

void UGetawayMenuWidget::OnJoinClicked()
{
	AGetawayPlayerController* PC = Owner();
	if (!PC) return;

	const FString Address = GetTypedAddress();
	if (Address.IsEmpty())
	{
		// Said here rather than in the controller, because it is about the box and not about the
		// network: there is nothing to try yet.
		SetStatus(TEXT("Type the host's IP address first."));
		if (AddressBox)
		{
			AddressBox->SetKeyboardFocus();
		}
		return;
	}

	SetStatus(FString::Printf(TEXT("Connecting to %s..."), *Address), false);
	PC->JoinGame(Address, GetTypedName());
}

void UGetawayMenuWidget::OnQuitClicked()
{
	if (AGetawayPlayerController* PC = Owner())
	{
		PC->QuitToDesktop();
	}
}

void UGetawayMenuWidget::OnResumeClicked()
{
	if (AGetawayPlayerController* PC = Owner())
	{
		PC->HideMenu();
	}
}

void UGetawayMenuWidget::OnLeaveClicked()
{
	if (AGetawayPlayerController* PC = Owner())
	{
		PC->LeaveToMenu();
	}
}

void UGetawayMenuWidget::OnAddressCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
	// Enter in the address box joins, which is what anybody typing an IP expects. Tabbing or
	// clicking away is not a commit to act on.
	if (CommitMethod == ETextCommit::OnEnter)
	{
		OnJoinClicked();
	}
}
