#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
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
 * Crosshair dot, speedometer in **cm/s** - the editor's own units, so the number on screen is the
 * number you type into the Details panel - with the lab's u/s on the small line under it (1 u =
 * 2.5 cm), movement state and C4 count, plus the round: clocks, your hand, and the scoreboard.
 *
 * Everything about the round is read from AGetawayGameState and AGetawayPlayerState, never from
 * the game mode. A client has no game mode, which is why the build clock used to draw for the host
 * and nobody else.
 *
 * **The shape of it (2026-11-06).** Three things happen to every line of text on its way out, and
 * no call site knows about any of them:
 *
 * - **Size.** Text asks for a size in px and gets `Face`, a real Roboto face cached at that size,
 *   the same `FCoreStyle::GetDefaultFontStyle` the menu uses. It used to ask `GEngine`'s small and
 *   large fonts for a *multiple* of themselves, and both of those are the same 10 px fallback face
 *   (the engine's font names are commented out in BaseEngine.ini, so `GetSmallFont` and
 *   `GetLargeFont` return one runtime face made at size 10) - which meant the speedometer was a
 *   10 px raster stretched to 22, and making the HUD larger that way would only have made it
 *   blurrier. Every size in here is multiplied by `UIScale`, so the whole HUD grows as one piece.
 * - **The curve.** `Warp` squeezes everything towards the middle of the screen the further out it
 *   sits, which is what a cylinder wrapped around the eye does to anything printed on it, and
 *   `Bent` bows each line along it as well. The two arcs of the visor frame bow the same way, which
 *   is what makes the curve legible rather than just slightly odd.
 * - **The glow.** `Glow` lays rings of the text's own colour under it before drawing it, so it
 *   reads as lit rather than printed. Canvas text cannot be drawn additively - there is a `check()`
 *   in the engine that fires if you try, because the runtime font cache is an alpha-only texture -
 *   so the halo is stacked translucent copies instead, which over this game's black world comes to
 *   the same thing.
 *
 * `WrapAmount` 0, `TextBend` 0, `GlowStrength` 0, `bVisorFrame` off and `UIScale` 1 give back the
 * flat HUD this was, at its old size.
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

	/* ---------------- The size and shape of it ---------------- */

	/**
	 * Every size and every gap on screen is multiplied by this, so the whole HUD scales as one piece
	 * and nothing has to be re-spaced by hand. 1 is the HUD at the size it was before 2026-11-06.
	 * Text is re-rasterised at the new size rather than stretched, so a big number here costs
	 * nothing in sharpness.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD", meta = (ClampMin = "0.5", ClampMax = "3"))
	float UIScale = 1.3f;

	/**
	 * How far the HUD bows around your view. It is the fraction of its distance from the middle of
	 * the screen that an element loses out at the very edge, so the top corners hang lower and the
	 * bottom corners ride higher, as if the whole HUD were printed inside a visor. 0 is flat glass.
	 *
	 * **0 since 2026-11-06.** The curve went in with the visor arcs and the long key lists, which
	 * were what made it read as a curve; once both were stripped out the only thing left showing it
	 * was the hand line hanging low in the corner, which reads as crooked rather than wrapped. 0.15
	 * is where it was set when there was something to see it against.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD", meta = (ClampMin = "0", ClampMax = "0.6"))
	float WrapAmount = 0.f;

	/**
	 * How much of the curve a line of text takes on *along its own length*, on top of being placed
	 * on the curve. 1 bows a line exactly as the surface bows, which over a long hint line is a
	 * dramatic sweep; 0 leaves every line level and lets the layout do the wrapping on its own.
	 *
	 * Moot while `WrapAmount` is 0, which it is - a flat surface has nothing to bend along. 0.4 was
	 * the setting when the curve was on.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD", meta = (ClampMin = "0", ClampMax = "1"))
	float TextBend = 0.f;

	/** Px the halo reaches at `UIScale` 1, widened by the text's own size. 0 is no glow at all. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD", meta = (ClampMin = "0", ClampMax = "12"))
	float GlowRadius = 2.f;

	/**
	 * Alpha of one copy of the halo. There are eight copies to a ring and they overlap, so this is
	 * much lower than the brightness you end up seeing - 0.18 reads as about half-lit at the edge
	 * of a glyph. 0 is no glow.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD", meta = (ClampMin = "0", ClampMax = "1"))
	float GlowStrength = 0.18f;

	/** Rings of halo. 1 is a tight edge, 2 a soft bloom, 3 a fog. Each ring costs eight draws. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD", meta = (ClampMin = "0", ClampMax = "4"))
	int32 GlowRings = 2;

	/** Half the crosshair dot, in px at `UIScale` 1. It is the one thing you aim with: keep it small. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD", meta = (ClampMin = "1", ClampMax = "8"))
	float CrosshairSize = 2.f;

	/**
	 * The two stitched arcs across the top and bottom of the screen. They make the curve legible -
	 * a bent line of text on its own is easy to miss, and a frame that bows the same way says the
	 * whole HUD is wrapped around you.
	 *
	 * **Off since 2026-11-06**, asked for the same day they were built: against a black world with
	 * three readouts on it they were the loudest thing on screen, and what is left reads cleaner
	 * without a frame around nothing. Everything that draws them is still here and still tuned.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD")
	bool bVisorFrame = false;

	/** The frame's colour. The look's own pink, because pink is what this game is. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD")
	FLinearColor VisorColour = FLinearColor(1.f, 0.35f, 0.68f);

	/** How bright the frame is. It is scenery, not a readout, so it stays well under the text. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD", meta = (ClampMin = "0", ClampMax = "1"))
	float VisorAlpha = 0.35f;

	/** Stitches in each arc. More is smoother and costs one tile each. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Getaway|HUD", meta = (ClampMin = "8", ClampMax = "160"))
	int32 VisorStitches = 56;

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

	/* ---------------- drawing: sized, curved and glowing ---------------- */

	/** A layout px at `UIScale` 1 turned into one on this screen. Every offset in here goes through it. */
	float S(float Px) const { return Px * FMath::Max(0.01f, UIScale); }

	/**
	 * A face at a real size, `UIScale` already in it. Roboto out of the engine's own Slate
	 * resources, which is what the menu is built from, so it needs no asset and survives a cook.
	 */
	FSlateFontInfo Face(float Px, bool bBold = false) const;

	/** What a string will occupy, in canvas px. */
	FVector2D Measure(const FString& Text, const FSlateFontInfo& Font) const;
	/** How far along the line the first `Count` characters reach, kerning and all. */
	float MeasureTo(const FString& Text, int32 Count, const FSlateFontInfo& Font) const;

	/** This frame's canvas, kept so the curve can be worked out without passing it down everywhere. */
	float ScreenW = 0.f;
	float ScreenH = 0.f;

	/**
	 * A flat layout position moved onto the curve. The sideways position is untouched and the
	 * vertical distance from the middle of the screen is squeezed by how far out you are.
	 */
	FVector2D Warp(float X, float Y) const;

	/** One run of text, once, with no halo and no curve. Everything below ends up here. */
	void Blit(const FText& Text, const FSlateFontInfo& Font, float X, float Y, const FLinearColor& Colour);

	/** One run of text with the halo under it. Nothing in here bends: `Bent` cuts the line up. */
	void Glow(const FString& Text, const FSlateFontInfo& Font, float X, float Y, const FLinearColor& Colour);

	/**
	 * Text from X, sat on the curve and bent along it. The line is walked a character at a time and
	 * broken wherever the bow has moved on, then each piece is drawn as a string - so spacing and
	 * kerning inside a piece are the font's own, and a line near the middle of the screen, or any
	 * short one, still costs a single draw.
	 *
	 * `AnchorX` is the sideways position whose height on the curve the whole line is hung from, and
	 * it matters for a column: right-aligned rows all have to hang off the same X or rows of
	 * different widths end up unevenly spaced. Left alone it is X, which is what left-aligned text
	 * wants.
	 */
	void Bent(const FString& Text, const FSlateFontInfo& Font, float X, float Y, const FLinearColor& Colour,
		float AnchorX = TNumericLimits<float>::Max());

	/** Text centred on CX, since four different things here want it. */
	void Centred(const FString& Text, const FSlateFontInfo& Font, float CX, float Y, const FLinearColor& Colour);
	/** Text ending at Right, for the scoreboard column. */
	void RightAligned(const FString& Text, const FSlateFontInfo& Font, float Right, float Y, const FLinearColor& Colour);

	/** A glowing dot, and nothing else: whatever is drawn here is drawn over what you are aiming at. */
	void DrawCrosshair(float CX, float CY);
	/** The two arcs. Stitched out of tiles rather than drawn as lines - canvas lines ignore alpha. */
	void DrawVisorFrame();
};
