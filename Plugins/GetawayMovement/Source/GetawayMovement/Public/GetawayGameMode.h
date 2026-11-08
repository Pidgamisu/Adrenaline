#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GetawayGameState.h"
#include "GetawayPiece.h"
#include "GetawayGameMode.generated.h"

class AGetawayMenuCamera;
class AGetawayPlacementPawn;
class AGetawayPlayerState;

/**
 * Runs the round: everybody places a piece, then everybody races, then the round is scored.
 *
 * Placement possesses AGetawayPlacementPawn (a fly camera); the run possesses AGetawayCharacter at
 * the PlayerStart. One build clock (PlacementTime) covers everybody; spending your own hand starts
 * your own run early, Enter does the same by hand, and Backspace drops you out of a run. Pieces
 * placed during a round stay put for the rest of the session.
 *
 * This is the server-side authority and nothing else: it owns every transition, but what the
 * players can *see* of the round lives on AGetawayGameState (phase and clocks, shared) and
 * AGetawayPlayerState (your hand, your run, your score). That split is the whole reason a client
 * can draw a build clock at all - a client has no game mode. See *The round loop* in CLAUDE.md.
 *
 * Still missing: deaths. The trap-kill bonus is wired (RegisterTrapKill) but nothing calls it,
 * because nothing can die yet.
 */
UCLASS()
class GETAWAYMOVEMENT_API AGetawayGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGetawayGameMode();

	/** Everything that can be dealt. Filled with a default set in the constructor; edit in a Blueprint child. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Round")
	TArray<FGetawayPieceDef> Catalog;

	/** How many pieces the party box deals each player, each placement phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Round", meta = (ClampMin = "1"))
	int32 HandSize = 3;

	/**
	 * Every hand holds at least one ordinary piece and at least one special one, rather than being
	 * drawn from the whole catalog at random. At `HandSize` 2 - which is what BP_GetawayGameMode
	 * plays - that is exactly one of each: something to build with in slot 1, something that
	 * changes how the course is moved through in slot 2, in that order every round.
	 *
	 * Which is which is `bSpecial` on the piece. Off deals from the whole catalog as it did before,
	 * which could hand you two walls or two traps and nothing to join them with.
	 *
	 * Any slots past the first two are free, dealt from whatever is left either way.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Round")
	bool bOneOfEach = true;

	/**
	 * Seconds everyone gets to build before the run starts. Spending your own hand starts your own
	 * run early. 0 turns the clock off and waits for Enter, which is how placement was first tested.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Round", meta = (ClampMin = "0"))
	float PlacementTime = 30.f;

	/** Beat between your last piece landing and your run starting, so you see it drop in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Round", meta = (ClampMin = "0"))
	float LastPieceDelay = 1.f;

	/** Off drops you straight into the run, which is how the movement test course was played. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Round")
	bool bStartInPlacement = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Round")
	TSubclassOf<AGetawayPlacementPawn> PlacementPawnClass;

	/* ---------------- The round's effect, which is OFF ---------------- */

	/**
	 * **Off since 2026-11-08, and that is the decision rather than a default anybody should flip
	 * back casually.** Read *Effects are gone* in `CLAUDE.md` and the note at the top of
	 * `docs/effects.md` first.
	 *
	 * On: every other round, both players are given the same effect before anybody builds - a
	 * double jump, a charge of C4, low gravity, a grapple hook. One at a time, the same for
	 * everybody, gone at the end of the round. All of that machinery still works exactly as
	 * documented; nothing was deleted, it simply is not dealt.
	 *
	 * Two reasons it went. **Abilities belong to pieces**, because since *Speed comes from the
	 * pieces* everything else you can do comes from what somebody chose to place, and an effect
	 * dealt by the game mode was the last ability the level did not grant. And **low gravity was
	 * actively bad** - it took away the calibration the whole game is built on, which is a piece of
	 * feedback worth more than the feature was.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Effects")
	bool bRoundEffects = false;

	/**
	 * An effect lands on every Nth round, so 2 leaves round 1 plain, gives round 2 an effect, round
	 * 3 plain again. 1 means every round has one.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Effects", meta = (ClampMin = "1"))
	int32 EffectEveryNthRound = 2;

	/** What can be drawn. Filled with all three in the constructor; drop one here to retire it. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Effects")
	TArray<EGetawayEffect> EffectPool;

	/** Never the same effect two effect rounds running, while the pool has something else to offer. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Effects")
	bool bNoRepeatEffect = true;

	/* ---------------- The front end ---------------- */

	/**
	 * Open in the menu rather than in a round: ADRENALINE over the course turning slowly, with
	 * Host, Join and Quit. This is what a packaged build does when you double-click it, and what
	 * pressing Play in the editor does too.
	 *
	 * **Untick it while you are working on something that is not the menu** and Play goes straight
	 * into placement as it did before the front end existed.
	 *
	 * It only has any effect on a game that is nobody's client and nobody's server - see
	 * ShouldShowMenu. The two launchers in `Tools/` and the two-player test in docs/multiplayer.md
	 * both start a listen server, so neither of them ever sees the menu whatever this says.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Front end")
	bool bStartInMenu = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Front end")
	TSubclassOf<AGetawayMenuCamera> MenuPawnClass;

	/**
	 * Should this session be the front end? True when `bStartInMenu` is on, nothing is connected
	 * either way (`NM_Standalone`), and the URL this world was opened with carries no `Play`
	 * option.
	 *
	 * That last part is how the Host button gets back out of the menu: it travels to this same map
	 * with `?listen?Play=1`, and both halves of that independently say "this is a game".
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway|Front end")
	bool ShouldShowMenu() const;

	/** Put every player in the menu: the orbiting camera, and the panel over the top of it. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Front end")
	void StartMenu();

	/** Give one player the menu camera and the panel. Used for a late arrival too. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Front end")
	void StartMenuFor(APlayerController* PC);

	/* ---------------- The run clock ---------------- */

	/**
	 * Seconds a run may last before it is called, however many players are still out there. 0 lets
	 * it run until everyone is done - fine alone, but with no deaths in the game a player who never
	 * finishes would hold the round open forever, so leave it on.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Run", meta = (ClampMin = "0"))
	float RunTimeLimit = 120.f;

	/**
	 * Once the first player is home, everyone else gets this long. Ultimate Chicken Horse's
	 * hurry-up: it stops a finished round waiting on someone picking their way across. 0 is off.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Run", meta = (ClampMin = "0"))
	float FinishGraceTime = 10.f;

	/** Seconds the round-over banner stays up before the next placement phase. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Round", meta = (ClampMin = "0"))
	float FinishLingerTime = 2.5f;

	/*
	 * Reaching the objective lifts you into the fly camera to watch the rest of the field, after a
	 * beat spent looking at where you landed.
	 *
	 * Off, you keep your character for the rest of the run. That was the behaviour until
	 * 2026-11-04 and it was reported as a bug: the course has nothing left to offer a player who
	 * is already home except an edge to fall off, and falling off after finishing used to say
	 * "Killed" and then leave you hanging there, because nothing respawns a player who is not
	 * running.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Run")
	bool bWatchAfterFinish = true;

	/**
	 * How long a finisher keeps their body before the camera lifts. Long enough to read YOU MADE IT
	 * from where you stopped; short enough that you are watching the race again quickly.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Run", meta = (ClampMin = "0"))
	float FinishWatchDelay = 2.f;

	/* ---------------- Scoring ---------------- */

	/*
	 * The whole table, as of 2026-11-08: **a point for being home first, and a second point if
	 * nobody else got home at all.** Nothing else pays - not finishing second, not finishing at
	 * all, not a trap kill.
	 *
	 * **This game is about speed, not about everyone arriving.** That is what separates it from
	 * Ultimate Chicken Horse, whose central rule - nobody scores if everybody finishes - is now
	 * `bNoPointsIfAllFinish` and is **off**. There, the game asks you to build something you alone
	 * can clear; here it asks you to be *fastest*, and then pays you again for having built
	 * something the others could not clear at all. See *Scoring is about speed* in CLAUDE.md
	 * before adding anything back.
	 */

	/**
	 * Points for reaching the objective at all, **0** - finishing is not worth anything by itself,
	 * because only being first is. Set it to 1 to pay everybody who gets home.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Scoring", meta = (ClampMin = "0"))
	int32 GoalPoints = 0;

	/** The point for being first home. With `GoalPoints` 0 this is the whole of a normal round. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Scoring", meta = (ClampMin = "0"))
	int32 FirstGoalBonus = 1;

	/**
	 * The second point, paid to the first player home when **nobody else got home at all**.
	 *
	 * "At all" and "inside the window" are the same thing, which is the neat part: the first finish
	 * starts the `FinishGraceTime` hurry-up, so the round is already over that many seconds later.
	 * Anybody who was going to arrive has arrived by the time this is counted.
	 *
	 * This is the reward for building something cruel - the job `bNoPointsIfAllFinish` used to do
	 * in reverse, by paying nobody rather than paying you. It is the same knob an earlier table
	 * called `SoloGoalBonus`, which was removed on 2026-10-19 and is back with a different value
	 * and a different reason.
	 *
	 * Alone, you collect it every time you finish, because you were trivially the only one home.
	 * That is deliberate and harmless; single-player scoring decides nothing.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Scoring", meta = (ClampMin = "0"))
	int32 UncontestedBonus = 1;

	/**
	 * Ultimate Chicken Horse's central rule: if everybody got there, it was too easy and nobody
	 * scores. **Off since 2026-11-08**, because this game pays for speed - the fastest player is
	 * still the fastest player in a round everybody cleared, and taking their point away for the
	 * others' success is the opposite of what the scoring is for. `UncontestedBonus` is what
	 * rewards a hard level now.
	 *
	 * On, it zeroes the round for *everyone*, the first player home included.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Scoring")
	bool bNoPointsIfAllFinish = false;

	/* ---------------- Phases ---------------- */

	UFUNCTION(BlueprintPure, Category = "Getaway")
	EGetawayPhase GetPhase() const;

	/** Open a new round: deal every player a hand, rebuild the course, everyone flies. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void StartPlacement();

	/** Give one player the placement camera. Does not deal, reset the clock or rebuild the course. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void StartPlacementFor(APlayerController* PC);

	/** Everyone still building swaps to a character at the PlayerStart and runs. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void StartRun();

	/** One player stops building and runs. Called for everybody at once when the round sets off. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void StartRunFor(APlayerController* PC);

	/**
	 * One player says they have finished building - their hand is spent, or they pressed Enter with
	 * pieces to spare. It does *not* start their run: the placement phase is shared, so everybody
	 * sets off together once the last player is done. Only the build clock overrides that.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void MarkDonePlacing(APlayerController* PC);

	/** Start the run if nobody is still placing. Harmless to call at any time. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void CheckEveryoneReady();

	/** How many players have not finished building yet. 0 once the round is ready to set off. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 CountStillPlacing() const;

	/** One player gives up on their run and goes back to the fly camera. No goal points for them. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void GiveUpRunFor(APlayerController* PC);

	/**
	 * A trap killed somebody. Credits whoever placed the thing and then, after `DeathLingerTime`,
	 * either puts the victim back at the start or ends their run, depending on `bRespawnOnDeath`.
	 * Server only; `AGetawayCharacter::Die` routes here.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void PlayerDied(APlayerController* PC, AActor* KilledBy);

	/**
	 * Put one player back at the PlayerStart with a fresh character, their run still live. Server
	 * only; `PlayerDied` calls it after the linger. Does nothing outside a run, or for a player
	 * who is not running.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void RespawnRunner(APlayerController* PC);

	/**
	 * Dying puts you back at the start and your run carries on, as many times as you like until
	 * the run clock runs out.
	 *
	 * Turn it off for the old behaviour, which is what Ultimate Chicken Horse itself does: one
	 * death ends your run for the round and you watch the rest from the fly camera, scoring
	 * nothing. With it on, the thing that stops a brutal level dragging is `RunTimeLimit` and the
	 * `FinishGraceTime` hurry-up, not the death itself.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Round")
	bool bRespawnOnDeath = true;

	/** Beat between being killed and being moved, so you get a look at what got you. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Round", meta = (ClampMin = "0"))
	float DeathLingerTime = 1.2f;

	/* ---------------- Pieces ---------------- */

	/**
	 * Spawn a piece into the level, remembering who placed it. Returns the actor, or null if the
	 * definition is unusable.
	 *
	 * `SizeOverride` is for a piece that does not come out at its catalog size: spikes are trimmed
	 * back to the part of the surface that holds them up. Zero means use the catalog.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	AActor* PlacePiece(const FGetawayPieceDef& Def, const FVector& Centre, const FRotator& Rotation,
		AGetawayPlayerState* Placer, const FVector& SizeOverride = FVector::ZeroVector);

	/** Spend a slot of one player's hand, and start their run if that emptied it. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void TakeFromHand(APlayerController* PC, int32 Slot);

	/** Who placed a given piece, or null for level geometry. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	AGetawayPlayerState* GetPiecePlacer(const AActor* Piece) const;

	/* ---------------- Results ---------------- */

	/** Called by AGetawayFinish when a runner reaches it. Server only; de-duplicated per player. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void ReachedFinish(class AGetawayCharacter* Runner);

	/**
	 * Credit a kill to whoever placed the piece that did it. Nothing calls this yet; it is here so
	 * the bonus is one line away the day deaths land.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void RegisterTrapKill(class AGetawayCharacter* Victim, const AActor* Piece);

protected:
	virtual void BeginPlay() override;
	virtual void InitGameState() override;

	/** A player joining mid-placement needs a placement camera and a hand of their own. */
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	virtual void Logout(AController* Exiting) override;

private:
	AGetawayGameState* State() const;
	static AGetawayPlayerState* StateOf(const APlayerController* PC);

	/** Deal one player a fresh hand from the catalog, without repeats while it is big enough. */
	void DealHandFor(AGetawayPlayerState* PS);

	/** This round's effect, or None on a plain round. Server only; the game state carries it. */
	EGetawayEffect PickEffectForRound(int32 Round);

	/** The last effect actually dealt, so bNoRepeatEffect has something to compare against. */
	EGetawayEffect LastEffect = EGetawayEffect::None;

	/** Phase, run clock and objectives, done once however many players trigger the switch. */
	void BeginRun();

	/** Put a deadline on the run, on the game state so clients can see it and on a timer here. */
	void ApplyRunDeadline(float Seconds, bool bGrace);

	/** Has everyone who started a run finished it one way or another? Ends the round if so. */
	void CheckRoundOver();

	/** Called when nobody is left running, or the run clock runs out. Scores, then lingers. */
	void EndRound(bool bOutOfTime);

	/** The Ultimate Chicken Horse table, applied to this round's results. */
	void ScoreRound();

	AGetawayPlacementPawn* FindPlacementPawn(const APlayerController* PC) const;
	void RememberPlacementPawn(APlayerController* PC, AGetawayPlacementPawn* Pawn);
	void ForgetPlacementPawn(const APlayerController* PC);

	/** Fires StartRun when the build clock runs out. */
	FTimerHandle PlacementTimer;
	/** Calls the run when RunTimeLimit, or the grace after the first finish, runs out. */
	FTimerHandle RunTimer;
	/** Holds the round-over banner before the next placement phase. */
	FTimerHandle LingerTimer;

	/** How many players are already home this round, so the next one knows their place. */
	int32 FinishersThisRound = 0;

	/**
	 * Who placed what, as two parallel lists rather than a map so the pairing is plain. Only the
	 * trap-kill bonus reads it, and only on the server.
	 */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> PlacedPieces;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGetawayPlayerState>> PlacedBy;

	/** One placement camera per player, again as two parallel lists. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<APlayerController>> PlacementOwners;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGetawayPlacementPawn>> PlacementPawns;
};
