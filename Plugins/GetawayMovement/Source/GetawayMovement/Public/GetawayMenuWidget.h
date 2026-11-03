#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GetawayMenuWidget.generated.h"

class AGetawayPlayerController;
class UButton;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;

UENUM(BlueprintType)
enum class EGetawayMenuMode : uint8
{
	/** The front end: Host, Join and an address box, Quit. */
	Main,
	/** Escape in a game: Resume, Leave to menu, Quit. */
	Pause,
};

/**
 * The menu panel: ADRENALINE over the course turning behind it, and the four things you can do.
 *
 * **It is built entirely in C++ and there is no Widget Blueprint**, which is deliberate: this
 * project keeps its logic in code and gets no benefit from a `.uasset` nobody is going to open.
 * `UUserWidget::Initialize` creates a `WidgetTree` even for a native class, and calls
 * `NativeOnInitialized` as long as the widget has a player context - which `CreateWidget(PC, ...)`
 * gives it - so `NativeOnInitialized` is where the tree gets built. Checked in UserWidget.cpp,
 * 2026-11-02.
 *
 * It does not decide anything. Every button calls straight through to
 * `AGetawayPlayerController`, which owns the travelling.
 *
 * The colours follow docs/look.md: pink edges round dark fills, with the panel see-through enough
 * to watch the level turn behind it. The one deliberate departure is the status line, which is
 * allowed amber, because a failed join has to read as a problem rather than as architecture.
 */
UCLASS()
class GETAWAYMOVEMENT_API UGetawayMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Which set of buttons to build. Set before the widget is added to the viewport. */
	UPROPERTY(BlueprintReadWrite, Category = "Getaway|Menu")
	EGetawayMenuMode Mode = EGetawayMenuMode::Main;

	/* ---------------- Look ---------------- */

	/** The palette's edge pink, straight out of adrenaline_look.py. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Menu|Look")
	FLinearColor EdgePink = FLinearColor(1.f, 0.35f, 0.68f);

	/** What a button is filled with when you are not touching it. Nearly black, barely pink. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Menu|Look")
	FLinearColor ButtonFill = FLinearColor(0.055f, 0.008f, 0.03f, 0.92f);

	/** How much of the pink a button takes on under the pointer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Menu|Look", meta = (ClampMin = "0", ClampMax = "1"))
	float HoverStrength = 0.3f;

	/** The full-screen wash behind the panel. Its alpha is how much of the level you still see. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Menu|Look")
	FLinearColor ScrimColour = FLinearColor(0.f, 0.f, 0.f, 0.55f);

	/** The pause panel sits over a live game, so it washes the picture out further. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Menu|Look", meta = (ClampMin = "0", ClampMax = "1"))
	float PauseScrimAlpha = 0.78f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Menu|Look", meta = (ClampMin = "12"))
	float TitleSize = 96.f;

	/** The same word, smaller, over a game in progress. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Menu|Look", meta = (ClampMin = "12"))
	float PauseTitleSize = 42.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Menu|Look", meta = (ClampMin = "8"))
	float ButtonTextSize = 22.f;

	/** How wide the column of buttons and boxes is, in pixels. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Menu|Look", meta = (ClampMin = "160"))
	float PanelWidth = 440.f;

	/* ---------------- What it says ---------------- */

	/** Put a line under the buttons: why a join failed, or what is missing. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Menu")
	void SetStatus(const FString& Text, bool bIsProblem = true);

	/**
	 * Waiting on a host to answer: Host and Join go dead and a Cancel appears in their place, so
	 * the panel is never a dead end while a connection is being attempted.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Menu")
	void SetConnecting(bool bConnecting);

	/**
	 * Show this machine's own address above the buttons, for the host to read out. Empty hides
	 * the line.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Menu")
	void SetLocalAddress(const FString& Address);

	/** What is typed in the two boxes, trimmed. Empty when the box has not been built. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Menu")
	FString GetTypedAddress() const;

	UFUNCTION(BlueprintPure, Category = "Getaway|Menu")
	FString GetTypedName() const;

	/** Fill the boxes in from what was typed last time, before the widget goes up. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Menu")
	void SetTypedAddress(const FString& Address);

	UFUNCTION(BlueprintCallable, Category = "Getaway|Menu")
	void SetTypedName(const FString& Name);

	/** The box the player should be typing in, so the menu opens ready to type. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Menu")
	void FocusFirstBox();

protected:
	/**
	 * Where the tree gets built, and it has to be here rather than in NativeOnInitialized or
	 * NativeConstruct. `UUserWidget::RebuildWidget` reads `WidgetTree->RootWidget` and wraps
	 * whatever it finds, so by NativeConstruct it is already too late; and doing it at
	 * initialisation would mean `Mode` had to be set before `CreateWidget` returns, which it
	 * cannot be. Building here lets the caller set `Mode` and then add the widget to the viewport.
	 */
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	AGetawayPlayerController* Owner() const;

	/** The whole tree, built once. Mode decides which set of rows goes in. */
	void BuildPanel();

	/**
	 * One row of the column: a pink-framed button with a word in it.
	 *
	 * The frame is a UBorder filled pink with 2 px of padding and the button on top of it, rather
	 * than a border texture - it keeps the edge-band look of docs/look.md without needing an asset.
	 */
	UButton* AddButton(UVerticalBox* Column, const FString& Label);

	UEditableTextBox* AddTextBox(UVerticalBox* Column, const FString& Hint);

	UFUNCTION() void OnHostClicked();
	UFUNCTION() void OnJoinClicked();
	UFUNCTION() void OnQuitClicked();
	UFUNCTION() void OnResumeClicked();
	UFUNCTION() void OnLeaveClicked();
	UFUNCTION() void OnCancelClicked();
	UFUNCTION() void OnAddressCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> AddressBox;
	UPROPERTY(Transient) TObjectPtr<UEditableTextBox> NameBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AddressHint;

	/** Kept so SetConnecting can grey them out and show the Cancel. Only built in Main mode. */
	UPROPERTY(Transient) TObjectPtr<UButton> HostButton;
	UPROPERTY(Transient) TObjectPtr<UButton> JoinButton;
	/** The Cancel button's pink frame, so hiding it collapses the row rather than leaving a gap. */
	UPROPERTY(Transient) TObjectPtr<UWidget> CancelRow;
};
