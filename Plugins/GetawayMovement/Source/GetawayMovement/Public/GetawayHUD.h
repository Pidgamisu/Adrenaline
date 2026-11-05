#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GetawayHUD.generated.h"

class AGetawayGameState;
class AGetawayPlacementPawn;
class AGetawayPlayerState;

/** One line of feedback under the crosshair. Local to this machine; nothing about it replicates. */
struct FGetawayMessage
{
	FString Text;
	FLinearColor Colour = FLinearColor::White;
	/** World seconds at which it goes away. */
	double Until = 0.0;
	/** Its call site's key, so the same line replaces itself instead of stacking. -1 never does. */
	int32 Key = -1;
};

/**
 * Crosshair dot, speedometer in the lab's units (u/s, 1 u = 2.5 cm), movement state and C4 count,
 * plus the round: clocks, your hand, and the scoreboard.
 *
 * Everything about the round is read from AGetawayGameState and AGetawayPlayerState, never from
 * the game mode. A client has no game mode, which is why the build clock used to draw for the host
 * and nobody else.
 */
UCLASS()
class GETAWAYMOVEMENT_API AGetawayHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	/**
	 * Say something to this player: picking up C4, trying to throw one you have not got, dying.
	 *
	 * These used to be `GEngine->AddOnScreenDebugMessage`, whose whole body is
	 * `#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)`. The call compiles in a Shipping build and then
	 * silently does nothing, so the game would simply have stopped telling people what happened -
	 * which is a poor thing to discover after release. Checked in UnrealEngine.cpp, 2026-11-02.
	 *
	 * A line with the same `Key` replaces its own rather than stacking; -1 always adds a new one.
	 * The call sites kept the keys they passed to the debug messages.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void ShowMessage(const FString& Text, float Seconds = 1.5f,
		FLinearColor Colour = FLinearColor::White, int32 Key = -1);

	/** How many lines fit under the crosshair before the oldest is dropped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Messages", meta = (ClampMin = "1"))
	int32 MaxMessages = 4;

	/** Seconds a line spends fading out, so one leaving does not just blink off. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Messages", meta = (ClampMin = "0.05"))
	float MessageFadeTime = 0.5f;

	/* ---------------- The round's effect ---------------- */

	/**
	 * Seconds the effect sits big in the middle of the screen as the run starts, before it moves up
	 * out of the way. It is the one thing you have to read before you set off, so it is in front of
	 * your face rather than off in a corner - and then it gets out of your face.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Effects", meta = (ClampMin = "0"))
	float EffectBannerTime = 2.f;

	/** Seconds it takes to travel up to its corner. 0 snaps it there. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|Effects", meta = (ClampMin = "0"))
	float EffectMoveTime = 0.35f;

private:
	/** Drawn before either early return below, so the lines show during placement too. */
	void DrawMessages(float CX, float CY);

	TArray<FGetawayMessage> Messages;

	void DrawPlacementHUD(const AGetawayPlacementPawn* Placing, const AGetawayGameState* GS, float CX);
	void DrawRunClock(const AGetawayGameState* GS, float CX);
	/**
	 * The round's effect: big in the middle of the screen, then up to the top left corner.
	 *
	 * `Elapsed` is how long the thing being announced has been going - the build phase, in practice.
	 * Negative means it has already arrived, which is how the run draws the resting line without
	 * playing the arrival a second time.
	 */
	void DrawEffectBanner(const AGetawayGameState* GS, float CX, float CY, float Elapsed, float RestX);
	void DrawScoreboard(const AGetawayGameState* GS, const AGetawayPlayerState* Mine);
	void DrawRoundBanner(const AGetawayGameState* GS, const AGetawayPlayerState* Mine, float CX);
	/**
	 * You are home and somebody else is not. DrawRoundBanner only ever fires once the whole round
	 * is over, which solo is the same moment you cross the line and with company is not - so the
	 * first player home used to get no word at all that they had made it.
	 *
	 * Big while you still have your body, one line once the camera has lifted you out of it.
	 */
	void DrawFinishedBanner(const AGetawayGameState* GS, const AGetawayPlayerState* Mine, float CX);

	/** Text centred on CX, since four different things here want it. */
	void Centred(const FString& Text, class UFont* Font, float Scale, float CX, float Y, const FLinearColor& Color);
	/** Text ending at Right, for the scoreboard column. */
	void RightAligned(const FString& Text, class UFont* Font, float Scale, float Right, float Y, const FLinearColor& Color);
};
