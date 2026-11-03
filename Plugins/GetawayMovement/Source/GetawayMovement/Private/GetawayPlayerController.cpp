#include "GetawayPlayerController.h"

#include "GetawayMenuSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

AGetawayPlayerController::AGetawayPlayerController()
{
	MenuWidgetClass = UGetawayMenuWidget::StaticClass();
}

void AGetawayPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (!InputComponent) return;

	// A plain key binding, not an Enhanced Input action, and that is on purpose. Both pawns call
	// Subsystem->ClearAllMappings() when they are possessed (see AGetawayPlacementPawn and
	// AGetawayCharacter), so a mapping context registered here would be wiped the moment the
	// player got a pawn. The controller's own input component sits below the pawn's on the input
	// stack and survives every possession.
	//
	// Note for testing: in the editor's Play-in-Editor, Escape never reaches this. The editor
	// binds Escape to Stop (DebuggerCommands.cpp) and UGameViewportClient::InputKey consults the
	// editor's bindings *before* the player controller. Use Play > Standalone Game, or a packaged
	// build, to test it. Nothing inside the game can change that.
	FInputKeyBinding& Binding = InputComponent->BindKey(
		EKeys::Escape, IE_Pressed, this, &AGetawayPlayerController::TogglePauseMenu);
	Binding.bConsumeInput = true;
}

void AGetawayPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Hand the game the mouse and the keyboard back, unless a panel is already up.
	//
	// This is not belt and braces, it is the fix for a real bug found by playing on 2026-11-02:
	// after pressing Host you arrived in placement unable to look or press anything.
	// FInputModeUIOnly calls SetIgnoreInput(true) and SetMouseCaptureMode(NoCapture) on the
	// **UGameViewportClient** (checked in PlayerController.cpp), and the viewport client belongs
	// to the game instance, not to the world - so it sails straight through a level change with
	// "ignore all input" still set, and nothing on the other side puts it back.
	//
	// It has to tolerate running in either order: in a menu session the game mode may already have
	// called ShowMainMenu from HandleStartingNewPlayer before this BeginPlay, or not yet. If a
	// panel is up, its own input mode is the right one and this must leave it alone. In practice
	// ShowMainMenu does run first, so that guard is load-bearing rather than theoretical - both
	// orders were watched with a log on the viewport's IgnoreInput() flag, which is also how the
	// bug was pinned down.
	if (!IsMenuOpen())
	{
		ApplyInputMode(false);
	}

	// The menu itself is put up by AGetawayGameMode, which knows whether this is a menu session.
	// Asking the game state here would be a race: the phase is replicated and this runs before the
	// first update lands - the same ordering trap as AGetawayPlacementPawn::EnsureGhostMaterials.
	//
	// A failure that happened on the way here, though, is purely local, and is worth saying even
	// when we came back into a game rather than into the menu.
	if (const UGetawayMenuSubsystem* State = MenuState())
	{
		if (!State->PeekFailure().IsEmpty())
		{
			UE_LOG(LogTemp, Log, TEXT("Getaway: arrived with a connection failure to report: %s"),
				*State->PeekFailure());
		}
	}
}

void AGetawayPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	RememberTyping();
	if (Menu)
	{
		Menu->RemoveFromParent();
		Menu = nullptr;
	}
	Super::EndPlay(Reason);
}

UGetawayMenuSubsystem* AGetawayPlayerController::MenuState() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UGetawayMenuSubsystem>() : nullptr;
}

bool AGetawayPlayerController::IsMainMenuOpen() const
{
	return Menu && Menu->Mode == EGetawayMenuMode::Main;
}

void AGetawayPlayerController::ShowMainMenu()
{
	OpenMenu(EGetawayMenuMode::Main);
	if (!Menu) return;

	UGetawayMenuSubsystem* State = MenuState();
	if (!State) return;

	// Put back what was typed before the last travel, so leaving a game and coming back does not
	// mean typing your name in again.
	Menu->SetTypedName(State->PlayerName);
	Menu->SetTypedAddress(State->LastAddress);

	const FString Failure = State->TakeFailure();
	if (!Failure.IsEmpty())
	{
		Menu->SetStatus(Failure);
	}
	else
	{
		Menu->FocusFirstBox();
	}
}

void AGetawayPlayerController::ShowPauseMenu()
{
	OpenMenu(EGetawayMenuMode::Pause);
}

void AGetawayPlayerController::OpenMenu(EGetawayMenuMode InMode)
{
	if (Menu)
	{
		if (Menu->Mode == InMode) return;
		HideMenu();
	}

	UClass* Class = MenuWidgetClass ? MenuWidgetClass.Get() : UGetawayMenuWidget::StaticClass();
	UGetawayMenuWidget* New = CreateWidget<UGetawayMenuWidget>(this, Class);
	if (!New) return;

	// Both of these have to be set before AddToViewport. The mode because the two modes build
	// different trees and the tree is built in RebuildWidget, which is what AddToViewport
	// triggers; and focusable because a UUserWidget takes that flag at construction and ignores it
	// after (it is documented so on UUserWidget::bIsFocusable). Without it, FInputModeUIOnly logs
	// "Attempting to focus Non-Focusable widget" and the panel never takes the keyboard.
	New->Mode = InMode;
	New->SetIsFocusable(true);

	Menu = New;
	Menu->AddToViewport(100);
	ApplyInputMode(true);
}

void AGetawayPlayerController::HideMenu()
{
	if (!Menu) return;

	RememberTyping();
	Menu->RemoveFromParent();
	Menu = nullptr;
	ApplyInputMode(false);
}

void AGetawayPlayerController::TogglePauseMenu()
{
	// Escape while the front end is up does nothing. There is no game underneath to go back to,
	// and Quit is a click away.
	if (IsMainMenuOpen()) return;

	if (Menu)
	{
		HideMenu();
	}
	else
	{
		ShowPauseMenu();
	}
}

void AGetawayPlayerController::ApplyInputMode(bool bMenuUp)
{
	if (bMenuUp)
	{
		SetShowMouseCursor(true);
		// UI only, not GameAndUI: the pawn should stop taking input while a panel is up, or you
		// carry on running about behind it with no way to see where you are going.
		FInputModeUIOnly Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		if (Menu)
		{
			Mode.SetWidgetToFocus(Menu->TakeWidget());
		}
		SetInputMode(Mode);
	}
	else
	{
		SetShowMouseCursor(false);
		SetInputMode(FInputModeGameOnly());
	}
}

void AGetawayPlayerController::RememberTyping()
{
	if (!Menu || Menu->Mode != EGetawayMenuMode::Main) return;

	if (UGetawayMenuSubsystem* State = MenuState())
	{
		State->PlayerName = Menu->GetTypedName();
		const FString Address = Menu->GetTypedAddress();
		if (!Address.IsEmpty())
		{
			State->LastAddress = Address;
		}
	}
}

void AGetawayPlayerController::HostGame(const FString& PlayerName)
{
	RememberTyping();

	// ?listen makes this a listen server somebody can join; ?Play=1 is what tells the game mode
	// this is a game and not another visit to the menu. See *The front end* in
	// docs/multiplayer.md for the whole table of how a session starts.
	FString Options = TEXT("listen?Play=1");
	if (!PlayerName.IsEmpty())
	{
		Options += FString::Printf(TEXT("?Name=%s"), *PlayerName);
	}

	UE_LOG(LogTemp, Log, TEXT("Getaway: hosting - travelling to %s?%s"), *GameMap, *Options);
	UGameplayStatics::OpenLevel(this, FName(*GameMap), /*bAbsolute=*/true, Options);
}

void AGetawayPlayerController::JoinGame(const FString& Address, const FString& PlayerName)
{
	RememberTyping();

	FString URL = Address;
	if (!PlayerName.IsEmpty())
	{
		URL += FString::Printf(TEXT("?Name=%s"), *PlayerName);
	}

	UE_LOG(LogTemp, Log, TEXT("Getaway: joining %s"), *URL);
	// Absolute, so none of the menu session's own URL comes along - checked in URL.cpp: only
	// TRAVEL_Relative and TRAVEL_Partial inherit a base URL's options.
	ClientTravel(URL, TRAVEL_Absolute);
}

void AGetawayPlayerController::LeaveToMenu()
{
	HideMenu();

	UE_LOG(LogTemp, Log, TEXT("Getaway: leaving to the menu - travelling to %s"), *GameMap);
	// No options at all, which is exactly what makes the game mode open in the menu phase.
	ClientTravel(GameMap, TRAVEL_Absolute);
}

void AGetawayPlayerController::QuitToDesktop()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
}
