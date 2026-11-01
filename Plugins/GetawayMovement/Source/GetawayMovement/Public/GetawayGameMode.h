#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GetawayGameState.h"
#include "GetawayPiece.h"
#include "GetawayGameMode.generated.h"

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

	/* ---------------- Scoring ---------------- */

	/*
	 * The whole table, as of 2026-10-19: a point for getting there, a point for getting there
	 * first, and nothing at all in a round everybody cleared. Points come from the objective and
	 * nowhere else - a solo-goal bonus, a scale-back on the leader's bonuses and a payout for
	 * trap kills all used to sit here and were each taken out on purpose. See *Scoring* in
	 * CLAUDE.md before adding anything back.
	 */

	/** Points for reaching the objective at all. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Scoring", meta = (ClampMin = "0"))
	int32 GoalPoints = 1;

	/** Extra for being first home. Being the only one home still counts as first. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Scoring", meta = (ClampMin = "0"))
	int32 FirstGoalBonus = 1;

	/**
	 * Ultimate Chicken Horse's central rule, and the only thing stopping the level being a straight
	 * corridor: if everybody got there, it was too easy and nobody scores. Off only makes sense for
	 * testing. Kept deliberately when the rest of the table was simplified away.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Scoring")
	bool bNoPointsIfAllFinish = true;

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
