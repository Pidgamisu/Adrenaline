#include "GetawayPlayerController.h"

#include "GetawayGameState.h"
#include "GetawayMenuSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Misc/ConfigCacheIni.h"
#include "Engine/World.h"
#include "TimerManager.h"
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

	ApplyChosenName();
}

void AGetawayPlayerController::ApplyChosenName()
{
	// `?Name=` on the URL does not work, which is worth knowing because this project has believed
	// it did since the Tools/ launchers were written. The client overwrites it: its login URL is
	// rebuilt with the online subsystem's nickname, so a host launched with `?Name=HostNathan`
	// still logs in as `?Name=RPidgey-9201E4034500...`. Watched happening on 2026-11-02 with two
	// instances over the loopback.
	//
	// ServerChangeName is the supported way round it - the engine's own rename path, which runs
	// locally on a listen-server host and as an RPC from a client.
	if (!IsLocalController()) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// Nothing to name in the front end: there is no round and no scoreboard.
	if (const AGetawayGameState* GS = World->GetGameState<AGetawayGameState>())
	{
		if (GS->GetPhase() == EGetawayPhase::Menu) return;
	}

	// What was asked for on the way in, or failing that whatever `?Name=` the URL carried - which
	// is how `Tools/Host-Adrenaline.bat` asks for a name, and is still readable here even though
	// the login overwrote it.
	FString Chosen;
	if (const UGetawayMenuSubsystem* State = MenuState())
	{
		Chosen = State->PlayerName;
	}
	if (Chosen.IsEmpty())
	{
		// The game mode's own copy of the options first: it is what the map was actually opened
		// with. Only the server has a game mode, hence the URL as a second try.
		if (const AGameModeBase* GM = World->GetAuthGameMode())
		{
			Chosen = UGameplayStatics::ParseOption(GM->OptionsString, TEXT("Name"));
		}
	}
	if (Chosen.IsEmpty())
	{
		Chosen = UGameplayStatics::ParseOption(World->URL.ToString(), TEXT("Name"));
	}
	if (Chosen.IsEmpty()) return;

	// Reject the engine's own default, which is not a name anybody chose. Every URL picks up
	// `?Name=Player` on its own: `FURL::LoadURLConfig` copies the whole `[DefaultPlayer]` ini
	// section in as options, and `BaseGame.ini` sets `Name=Player` there. Without this, an unnamed
	// player is renamed to "Player" and two of them are indistinguishable - worse than the
	// engine's own fallback, which at least appends a player id. Spotted in a log on 2026-11-02.
	//
	// Read from the ini rather than hardcoded, since that is where the value actually lives.
	FString IniDefaultName;
	GConfig->GetString(TEXT("DefaultPlayer"), TEXT("Name"), IniDefaultName, GGameIni);
	if (!IniDefaultName.IsEmpty() && Chosen.Equals(IniDefaultName, ESearchCase::IgnoreCase)) return;

	UE_LOG(LogTemp, Log, TEXT("Getaway: asking to be called '%s'"), *Chosen);
	ServerChangeName(Chosen);
}

void AGetawayPlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
	RememberTyping();
	// The timer dies with the world, but the flag is read by TogglePauseMenu, so clear it too.
	bConnecting = false;
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
	// The game mode calls this twice for the first player of a menu session - once from
	// HandleStartingNewPlayer at login and once from StartMenu in BeginPlay - so everything below
	// would otherwise be done twice, including taking the failure message off the subsystem and
	// stealing focus back to the name box.
	if (IsMainMenuOpen()) return;

	OpenMenu(EGetawayMenuMode::Main);
	if (!Menu) return;

	UGetawayMenuSubsystem* State = MenuState();
	if (!State) return;

	// Put back what was typed before the last travel, so leaving a game and coming back does not
	// mean typing your name in again.
	Menu->SetTypedName(State->PlayerName);
	Menu->SetTypedAddress(State->LastAddress);

	// Tell the host what their friends have to type. Nothing else in the game says this any more
	// now that hosting goes through the menu rather than through Tools/Host-Adrenaline.bat.
	// All of them, because a machine with a mesh VPN running has one address for a friend in the
	// same house and a different one for a friend in another city, and this code cannot tell which
	// is wanted. See *Reading out the right address* in docs/multiplayer.md.
	const TArray<FGetawayLocalAddress> Mine = UGetawayMenuSubsystem::FindLocalAddresses();
	Menu->SetLocalAddresses(Mine);

	FString Listed;
	for (const FGetawayLocalAddress& One : Mine)
	{
		if (!Listed.IsEmpty()) Listed += TEXT(", ");
		Listed += One.Address;
		if (!One.Note.IsEmpty()) Listed += FString::Printf(TEXT(" [%s]"), *One.Note);
	}
	UE_LOG(LogTemp, Log, TEXT("Getaway: the menu is up; this machine is reachable on %s"),
		Listed.IsEmpty() ? TEXT("(no address found)") : *Listed);

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
	// Escape while waiting on a host stops waiting. Worth having as well as the Cancel button,
	// because Escape is what anybody reaches for when something appears to have hung.
	if (bConnecting)
	{
		CancelJoin(TEXT("Stopped waiting."));
		return;
	}

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
		// Both guarded, so an empty box never erases something already known. That matters on the
		// way out: EndPlay calls this during the travel, and without the guard a name set through
		// HostGame or the console was wiped by the blank box a moment later.
		const FString Name = Menu->GetTypedName();
		if (!Name.IsEmpty())
		{
			State->PlayerName = Name;
		}
		const FString Address = Menu->GetTypedAddress();
		if (!Address.IsEmpty())
		{
			State->LastAddress = Address;
		}
	}
}

void AGetawayPlayerController::RememberName(const FString& PlayerName)
{
	if (PlayerName.IsEmpty()) return;

	if (UGetawayMenuSubsystem* State = MenuState())
	{
		State->PlayerName = PlayerName;
	}
}

void AGetawayPlayerController::HostGame(const FString& PlayerName)
{
	RememberTyping();
	// The argument, not just whatever is in the boxes: this is also reachable from Blueprint and
	// from the console, where there is no widget to read.
	RememberName(PlayerName);

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
	RememberName(PlayerName);
	if (UGetawayMenuSubsystem* State = MenuState())
	{
		// So a failed attempt comes back to the menu with the address still in the box.
		State->LastAddress = Address;
	}

	FString URL = Address;
	if (!PlayerName.IsEmpty())
	{
		URL += FString::Printf(TEXT("?Name=%s"), *PlayerName);
	}

	UE_LOG(LogTemp, Log, TEXT("Getaway: joining %s"), *URL);

	// Start watching before travelling, not after: a connection that is refused outright can fail
	// inside this frame.
	bConnecting = true;
	ConnectingTo = Address;
	ConnectStartedAt = FPlatformTime::Seconds();
	if (Menu)
	{
		Menu->SetConnecting(true);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ConnectTimer, this,
			&AGetawayPlayerController::TickConnecting, 0.25f, /*bLoop=*/true);
	}

	// Absolute, so none of the menu session's own URL comes along - checked in URL.cpp: only
	// TRAVEL_Relative and TRAVEL_Partial inherit a base URL's options.
	ClientTravel(URL, TRAVEL_Absolute);
}

void AGetawayPlayerController::TickConnecting()
{
	if (!bConnecting) return;

	// An engine-reported failure beats the clock: say what actually went wrong rather than
	// "no answer", and say it as soon as it is known.
	if (UGetawayMenuSubsystem* State = MenuState())
	{
		if (!State->PeekFailure().IsEmpty())
		{
			CancelJoin(State->TakeFailure());
			return;
		}
	}

	if (FPlatformTime::Seconds() - ConnectStartedAt >= JoinTimeout)
	{
		CancelJoin(FString::Printf(
			TEXT("No answer from %s. Check the address, that they are in a game, and that they let the game through Windows Firewall. On this machine, use 127.0.0.1."),
			*ConnectingTo));
	}
}

void AGetawayPlayerController::CancelJoin(const FString& Why)
{
	if (!bConnecting) return;

	bConnecting = false;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ConnectTimer);
	}

	// Drop the half-open connection, or it keeps its socket and its own timeout running behind the
	// menu and can land us in a game minutes after we gave up on it.
	if (GEngine && World)
	{
		GEngine->CancelPending(World);
	}

	UE_LOG(LogTemp, Log, TEXT("Getaway: gave up joining %s - %s"), *ConnectingTo, *Why);

	if (Menu)
	{
		Menu->SetConnecting(false);
		Menu->SetStatus(Why);
	}
	ConnectingTo.Empty();
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
