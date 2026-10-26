#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GetawayHUD.generated.h"

class AGetawayGameState;
class AGetawayPlacementPawn;
class AGetawayPlayerState;

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

private:
	void DrawPlacementHUD(const AGetawayPlacementPawn* Placing, const AGetawayGameState* GS, float CX);
	void DrawRunClock(const AGetawayGameState* GS, float CX);
	void DrawScoreboard(const AGetawayGameState* GS, const AGetawayPlayerState* Mine);
	void DrawRoundBanner(const AGetawayGameState* GS, const AGetawayPlayerState* Mine, float CX);

	/** Text centred on CX, since four different things here want it. */
	void Centred(const FString& Text, class UFont* Font, float Scale, float CX, float Y, const FLinearColor& Color);
	/** Text ending at Right, for the scoreboard column. */
	void RightAligned(const FString& Text, class UFont* Font, float Scale, float Right, float Y, const FLinearColor& Color);
};
