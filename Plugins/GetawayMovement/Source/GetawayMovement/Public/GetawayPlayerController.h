#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GetawayMenuWidget.h"
#include "GetawayPlayerController.generated.h"

class UGetawayMenuSubsystem;

/**
 * The front end lives here: the menu panel, Escape, and the four things a player can do about the
 * session they are in - host one, join one, leave one, quit.
 *
 * It is the only place in the plugin that travels. The widget just calls these.
 *
 * See *The front end* in docs/multiplayer.md for how a menu session is told from a game session,
 * and why there is no menu map.
 */
UCLASS()
class GETAWAYMOVEMENT_API AGetawayPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGetawayPlayerController();

	/**
	 * The map the game is played in, and also the map the menu runs in - they are the same map.
	 * Hosting travels here with `?listen?Play=1`, and leaving travels here with nothing, which is
	 * what puts you back in the menu.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Front end")
	FString GameMap = TEXT("/Game/Maps/TestCourse");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Front end")
	TSubclassOf<UGetawayMenuWidget> MenuWidgetClass;

	/* ---------------- The panel ---------------- */

	/** Put the front end up: ADRENALINE, Host, Join, Quit. Called by the game mode. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Front end")
	void ShowMainMenu();

	/** Put the in-game panel up: Resume, Leave to menu, Quit. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Front end")
	void ShowPauseMenu();

	/** Take whichever panel is up down and give the game the mouse back. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Front end")
	void HideMenu();

	UFUNCTION(BlueprintPure, Category = "Getaway|Front end")
	bool IsMenuOpen() const { return Menu != nullptr; }

	/** True while the panel up is the front end rather than the in-game one. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Front end")
	bool IsMainMenuOpen() const;

	/** What Escape does: open the in-game panel, or close it again. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Front end")
	void TogglePauseMenu();

	/* ---------------- The four buttons ---------------- */

	/**
	 * Start a game others can join: travel to GameMap as a listen server, with `Play=1` so the
	 * round opens in placement instead of coming straight back to the menu.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Front end")
	void HostGame(const FString& PlayerName);

	/**
	 * Connect to a host. Address is what was typed - an IP, optionally with `:port`.
	 *
	 * Also an `Exec`, so it can be driven from the console as `JoinGame 10.10.240.77 Nathan`.
	 * That is a better escape hatch than the engine's own `open <ip>`, because this path has the
	 * give-up clock on it and will tell you when nothing answered; `open` just hangs.
	 */
	UFUNCTION(Exec, BlueprintCallable, Category = "Getaway|Front end")
	void JoinGame(const FString& Address, const FString& PlayerName);

	/**
	 * Stop waiting for a host and go back to the menu as it was, with `Why` on the status line.
	 * Called by the Cancel button, by Escape, and by the join clock running out.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Front end")
	void CancelJoin(const FString& Why);

	/**
	 * Seconds to wait for a host to answer before giving up and saying so.
	 *
	 * The engine will not do this for us. `UEngine::HandleNetworkFailure` ends in
	 * `CallHandleDisconnectForFailure`, which only acts through `GetOnlineSession()` - and this
	 * project deliberately has no online subsystem yet ("Local first, Steam later"), so there is
	 * nothing there to take us back to a stable map. Left to the engine, a join that never
	 * connects sits on "Connecting..." for ever. Found by playing, 2026-11-02.
	 *
	 * 10 s is long enough for a LAN and short enough not to feel hung. A real timeout from the
	 * engine, if one arrives first, is reported instead of this.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Front end", meta = (ClampMin = "1"))
	float JoinTimeout = 10.f;

	/** True between pressing Join and either arriving, failing or cancelling. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Front end")
	bool IsConnecting() const { return bConnecting; }

	/**
	 * Back to the front end: absolute travel to GameMap with no options at all, which is what
	 * makes the game mode open in the menu phase rather than in placement.
	 *
	 * On a listen server host this drops the other players, because the net driver goes down with
	 * the world. Their own failure handler puts them back at their menu with a message. Fine for
	 * a LAN game among friends; see *What the front end does not do* in docs/multiplayer.md.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Front end")
	void LeaveToMenu();

	UFUNCTION(BlueprintCallable, Category = "Getaway|Front end")
	void QuitToDesktop();

protected:
	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Build the widget for this mode, add it to the viewport, and hand it the mouse. */
	void OpenMenu(EGetawayMenuMode InMode);

	/** Cursor on and input to the panel while it is up; cursor off and input to the game when not. */
	void ApplyInputMode(bool bMenuUp);

	UGetawayMenuSubsystem* MenuState() const;

	/** Remember the boxes so leaving a game and coming back does not mean typing them again. */
	void RememberTyping();

	/** Keep a name across the travel, so ApplyChosenName can ask for it on the other side. */
	void RememberName(const FString& PlayerName);

	/**
	 * Ask to be called by the name typed into the menu, once we are in a game.
	 *
	 * Needed because `?Name=` on the travel URL is silently discarded - see the comment on the
	 * implementation. Nothing in this project had noticed, because the `.bat` launchers that
	 * relied on it had never been run.
	 */
	void ApplyChosenName();

	/**
	 * Watches a connection attempt a few times a second: reports an engine failure the moment one
	 * is recorded, and gives up once `JoinTimeout` has passed.
	 *
	 * A repeating check rather than a single timer, because the engine's own failure never travels
	 * us anywhere (see `JoinTimeout`) - so if we only waited for the deadline, a host that
	 * actively refused us would still show "Connecting..." until the clock ran out.
	 */
	void TickConnecting();

	UPROPERTY(Transient)
	TObjectPtr<UGetawayMenuWidget> Menu;

	bool bConnecting = false;
	/** What was typed, kept so the give-up message can name it. */
	FString ConnectingTo;
	double ConnectStartedAt = 0.0;
	FTimerHandle ConnectTimer;
};
