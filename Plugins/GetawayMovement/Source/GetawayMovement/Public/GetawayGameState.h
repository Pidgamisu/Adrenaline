#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GetawayGameState.generated.h"

class AGetawayPlayerState;

UENUM(BlueprintType)
enum class EGetawayPhase : uint8
{
	/** Flying around dropping a piece into the level. */
	Placement,
	/** Running the level as AGetawayCharacter. */
	Run,
	/**
	 * The front end: nobody is playing, the course turns slowly behind the menu panel and the
	 * player holds an AGetawayMenuCamera.
	 *
	 * Deliberately last in the enum rather than first, where it reads better. The values are
	 * replicated and are also saved into Blueprint defaults and into the map, so Placement has to
	 * stay 0 and Run has to stay 1 or anything already stored shifts underneath us.
	 */
	Menu,
};

/**
 * What everybody can see about the round: which phase it is, when the clocks run out, and how the
 * round ended.
 *
 * This exists because a client has no game mode at all - UGameplayStatics::GetGameMode returns null
 * off the server - so before this, a client's build clock simply did not draw. The game mode is
 * still the authority and still drives every transition; it writes here and the clients read.
 *
 * Clocks are stored as absolute *server* times rather than as remaining seconds, so they do not
 * have to be replicated every tick. AGameStateBase::GetServerWorldTimeSeconds() is the clock they
 * are measured against and it works on both sides, so a client counts down on its own between
 * updates instead of stepping whenever a packet lands.
 */
UCLASS()
class GETAWAYMOVEMENT_API AGetawayGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AGetawayGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/* ---------------- Phase ---------------- */

	UFUNCTION(BlueprintPure, Category = "Getaway")
	EGetawayPhase GetPhase() const { return Phase; }

	/** Rounds played, counting from 1. Bumped as each placement phase opens. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 GetRoundNumber() const { return RoundNumber; }

	/** True between the round ending and the next placement phase opening - the banner window. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsRoundOver() const { return bRoundOver; }

	/* ---------------- The build clock ---------------- */

	/** True while a build clock is actually running. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsPlacementTimed() const;

	/** Seconds left to build. 0 when there is no clock. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetPlacementTimeLeft() const;

	/* ---------------- The run clock ---------------- */

	/** Seconds since the run started, frozen once the round is over. 0 while placing. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetRunTime() const;

	/** True while the run has a deadline - an overall limit, or the grace after the first finish. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsRunTimed() const;

	/** Seconds until the run is called. 0 when there is no deadline. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetRunTimeLeft() const;

	/** True once someone has finished and the others are running out their grace. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsGraceRunning() const { return bGraceRunning; }

	/** Best run of the session by anybody, or negative if nobody has finished yet. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetBestTime() const { return BestTime; }

	/* ---------------- Standings ---------------- */

	/** Every player, best score first, then by name so the order is stable between frames. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	TArray<AGetawayPlayerState*> GetStandings() const;

	/** The highest score anyone holds. 0 with nobody playing. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 GetLeadingScore() const;

	/* ---------------- Written by AGetawayGameMode, on the server ---------------- */

	void SetPhase(EGetawayPhase InPhase);
	void BeginPlacementClock(float Seconds);
	void BeginRunClock();
	void SetRunDeadline(float Seconds, bool bIsGrace);
	void ClearRunDeadline();
	void SetRoundOver(bool bOver);
	void BumpRound() { ++RoundNumber; }
	void ReportFinishTime(float Seconds);

private:
	UPROPERTY(Replicated)
	EGetawayPhase Phase = EGetawayPhase::Placement;

	UPROPERTY(Replicated)
	int32 RoundNumber = 0;

	UPROPERTY(Replicated)
	bool bRoundOver = false;

	/** Server time the build clock runs out. Negative means placement is untimed. */
	UPROPERTY(Replicated)
	float PlacementEndServerTime = -1.f;

	UPROPERTY(Replicated)
	float RunStartServerTime = 0.f;

	/** Server time the run gets called whatever happens. Negative means it runs until everyone is done. */
	UPROPERTY(Replicated)
	float RunEndServerTime = -1.f;

	UPROPERTY(Replicated)
	bool bGraceRunning = false;

	/** Frozen run time once the round is over, so the banner does not keep counting. */
	UPROPERTY(Replicated)
	float RoundEndRunTime = 0.f;

	UPROPERTY(Replicated)
	float BestTime = -1.f;

	/** Server time, read the same way on both sides. Falls back to local time before the first update. */
	float Now() const;
};
