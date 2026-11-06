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
 * The round's effect: one thing that is true for everybody this round, settled before anyone
 * builds.
 *
 * There is exactly one at a time and they do not stack - the value is overwritten every placement
 * phase and set back to None on a plain round, so nothing can carry over. It lives on
 * AGetawayGameState because both players get the same one and both have to be able to read it;
 * what an effect actually *does* is AGetawayCharacter::ApplyRoundEffect, on the machine that owns
 * the character.
 *
 * Add to the end only. The values are replicated and are saved into the game mode Blueprint's
 * effect pool, so renumbering shifts what is already stored underneath us.
 */
UENUM(BlueprintType)
enum class EGetawayEffect : uint8
{
	/** A plain round: nothing granted, every value at its tuned default. */
	None,
	/** A jump in the air, which the tuned default does not give you. */
	DoubleJump,
	/** You start the run carrying one charge of C4, and can hold only the one. */
	Loaded,
	/** Everybody floats: higher jumps, slower falls, and a C4 launch that throws you further. */
	LowGravity,
	/** One grappling hook: left click to pull yourself to whatever you are looking at. */
	Grapple,
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

	/* ---------------- The round's effect ---------------- */

	/** What everybody has this round. None on a plain round. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	EGetawayEffect GetActiveEffect() const { return ActiveEffect; }

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool HasActiveEffect() const { return ActiveEffect != EGetawayEffect::None; }

	/** Short name for the HUD, in capitals: "DOUBLE JUMP". Empty for None. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	static FString GetEffectName(EGetawayEffect Effect);

	/** The one-line explanation that sits under the name. Empty for None. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	static FString GetEffectBlurb(EGetawayEffect Effect);

	/* ---------------- The build clock ---------------- */

	/** True while a build clock is actually running. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsPlacementTimed() const;

	/** Seconds left to build. 0 when there is no clock. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetPlacementTimeLeft() const;

	/**
	 * Seconds since the build phase opened, whether or not it is timed. 0 outside placement.
	 *
	 * Counted up from the start rather than down to the end because what reads it is the effect
	 * banner, which wants the first two seconds of the phase - and that has to work with
	 * PlacementTime 0, where there is no end to count down to at all.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetPlacementElapsed() const;

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
	void SetActiveEffect(EGetawayEffect InEffect);
	void ReportFinishTime(float Seconds);

private:
	UPROPERTY(Replicated)
	EGetawayPhase Phase = EGetawayPhase::Placement;

	UPROPERTY(Replicated)
	int32 RoundNumber = 0;

	UPROPERTY(Replicated)
	bool bRoundOver = false;

	/**
	 * This round's effect. Replicated rather than worked out locally from the round number, so a
	 * client never has to know the rule and the game mode stays the only thing that decides.
	 */
	UPROPERTY(Replicated)
	EGetawayEffect ActiveEffect = EGetawayEffect::None;

	/** Server time the build clock runs out. Negative means placement is untimed. */
	UPROPERTY(Replicated)
	float PlacementEndServerTime = -1.f;

	/** Server time the build phase opened. Set every round, even when placement is untimed. */
	UPROPERTY(Replicated)
	float PlacementStartServerTime = -1.f;

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
