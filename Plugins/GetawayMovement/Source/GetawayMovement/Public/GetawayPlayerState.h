#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GetawayPiece.h"
#include "GetawayPlayerState.generated.h"

/** Where one player is in the round. Everyone starts each round Building. */
UENUM(BlueprintType)
enum class EGetawayRunState : uint8
{
	/** Still in the fly camera with pieces to place, or waiting out the build clock. */
	Building,
	/** On the course. */
	Running,
	/** Reached the objective. FinishTime and FinishPlace say when. */
	Finished,
	/** Out of time, or gave up and went back to the fly camera. No goal points. */
	Forfeit,
	/** Killed by a trap. Behaves exactly like Forfeit - it is separate so the scoreboard can say so. */
	Dead,
};

/**
 * One player's own round: the hand they drafted, how their run went, and what it was worth.
 *
 * Before this the hand was a single list on the game mode, so both players drafted from the same
 * three pieces and one of them spending a slot shifted the indices under the other - which did
 * happen during the spike's testing. The hand is dealt per player and lives here, replicated to its
 * owner only; AGetawayPlacementPawn::ServerPlacePiece looks the slot up in the *asking* player's
 * hand, so a client can still only place what it was actually dealt.
 *
 * Score is replicated to everybody, because the scoreboard is the point of the game.
 */
UCLASS()
class GETAWAYMOVEMENT_API AGetawayPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AGetawayPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void CopyProperties(APlayerState* NewPlayerState) override;

	/* ---------------- The hand ---------------- */

	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 GetHandCount() const { return Hand.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Getaway")
	bool GetHandPiece(int32 Slot, FGetawayPieceDef& OutDef) const;

	/** Server only. */
	void SetHand(const TArray<FGetawayPieceDef>& InHand);

	/** Server only. Returns true if that spent the last piece. */
	bool TakeFromHand(int32 Slot);

	/* ---------------- The run ---------------- */

	UFUNCTION(BlueprintPure, Category = "Getaway")
	EGetawayRunState GetRunState() const { return RunState; }

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsBuilding() const { return RunState == EGetawayRunState::Building; }

	/**
	 * Finished building this round: hand spent, or Enter pressed to say so with pieces to spare.
	 * The placement phase is shared, so this is what everybody else is waiting on. Being done does
	 * not start your run - the round starts everyone together once the last player is done, or when
	 * the build clock runs out. Cleared by BeginRound.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsDonePlacing() const { return bDonePlacing; }

	/** Still building and still able to place: what the round is actually waiting for. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsStillPlacing() const { return IsBuilding() && !bDonePlacing; }

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsRunning() const { return RunState == EGetawayRunState::Running; }

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool HasFinished() const { return RunState == EGetawayRunState::Finished; }

	/** This round's time, or negative if they did not finish it. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetFinishTime() const { return FinishTime; }

	/** 1 for first home this round, 0 for nobody. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 GetFinishPlace() const { return FinishPlace; }

	/** Their own best of the session, or negative. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetBestTime() const { return BestTime; }

	/* ---------------- Score ---------------- */

	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 GetPoints() const { return Points; }

	/** What the last scored round was worth to them, for the banner. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 GetLastRoundPoints() const { return LastRoundPoints; }

	/** Kills this round by a piece this player placed. Nothing sets it yet - deaths do not exist. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 GetTrapKills() const { return TrapKills; }

	/* ---------------- Written by AGetawayGameMode, on the server ---------------- */

	/** Clear everything that is about one round, keeping the running score. */
	void BeginRound();
	void SetRunState(EGetawayRunState InState);
	void SetDonePlacing(bool bValue);
	void MarkFinished(float Seconds, int32 Place);
	void AddTrapKill() { ++TrapKills; }
	void AwardPoints(int32 Delta);
	void SetLastRoundPoints(int32 Value) { LastRoundPoints = Value; }

	/** The beat between spending your last piece and your run starting. Server-side only. */
	FTimerHandle HandSpentTimer;

private:
	/** Dealt per player. Owner only: nobody else needs to know what you are holding. */
	UPROPERTY(Replicated)
	TArray<FGetawayPieceDef> Hand;

	UPROPERTY(Replicated)
	EGetawayRunState RunState = EGetawayRunState::Building;

	UPROPERTY(Replicated)
	bool bDonePlacing = false;

	UPROPERTY(Replicated)
	float FinishTime = -1.f;

	UPROPERTY(Replicated)
	int32 FinishPlace = 0;

	UPROPERTY(Replicated)
	float BestTime = -1.f;

	UPROPERTY(Replicated)
	int32 Points = 0;

	UPROPERTY(Replicated)
	int32 LastRoundPoints = 0;

	UPROPERTY(Replicated)
	int32 TrapKills = 0;
};
