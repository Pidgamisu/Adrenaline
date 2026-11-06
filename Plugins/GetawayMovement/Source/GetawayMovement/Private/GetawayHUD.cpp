#include "GetawayHUD.h"

#include "GetawayCharacter.h"
#include "GetawayGameState.h"
#include "GetawayMovementComponent.h"
#include "GetawayPlacementPawn.h"
#include "GetawayPlayerState.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "EngineFontServices.h"
#include "Fonts/FontMeasure.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Styling/CoreStyle.h"

namespace
{
	const FLinearColor Dim(0.64f, 0.66f, 0.71f);
	const FLinearColor Live(0.37f, 0.77f, 0.84f);
	const FLinearColor Warn(0.94f, 0.70f, 0.24f);
	const FLinearColor Urgent(1.f, 0.42f, 0.3f);
	const FLinearColor Go(0.31f, 0.82f, 0.58f);

	/**
	 * Every size on the HUD, in px before `UIScale`. They are the sizes the old HUD worked out to -
	 * it asked for a multiple of a 10 px face, so a scale of 2.2 was 22 px - which makes `UIScale`
	 * mean exactly what it says: 1.5 is half again the size this was always played at.
	 */
	namespace Px
	{
		constexpr float Speedo = 22.f;      // the speedometer's number
		constexpr float Unit = 11.f;        // "best 12.30 s", "build time", the slide tell
		constexpr float Hand = 14.f;        // what is in your hand
		constexpr float Hint = 10.f;        // the key lists
		constexpr float Clock = 16.f;       // the build clock and the run clock
		constexpr float Message = 13.f;     // a line under the crosshair
		constexpr float Row = 11.f;         // a scoreboard row
		constexpr float MyRow = 13.f;       // ...and your own, picked out
		constexpr float Status = 13.f;      // the placement line, "Click to place"
		constexpr float Piece = 11.f;       // a row of your hand
		constexpr float HeldPiece = 14.f;   // ...and the one you are holding
		constexpr float Banner = 26.f;      // YOU MADE IT / ROUND OVER
		constexpr float BannerSub = 15.f;   // your time under it
		constexpr float EffectBig = 30.f;   // the round's effect, announced
		constexpr float EffectRest = 15.f;  // ...and up in its corner
	}

	/** The eight directions a ring of halo is laid in. */
	const FVector2D HaloDirs[8] =
	{
		FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f), FVector2D(0.f, 1.f), FVector2D(0.f, -1.f),
		FVector2D(0.7071f, 0.7071f), FVector2D(-0.7071f, 0.7071f),
		FVector2D(0.7071f, -0.7071f), FVector2D(-0.7071f, -0.7071f)
	};

	/** 1st, 2nd, 3rd, 4th. The teen rule costs a line and a wrong "11st" would be remembered. */
	FString Ordinal(int32 N)
	{
		const int32 Mod100 = N % 100;
		const TCHAR* Suffix = TEXT("th");
		if (Mod100 < 11 || Mod100 > 13)
		{
			switch (N % 10)
			{
			case 1: Suffix = TEXT("st"); break;
			case 2: Suffix = TEXT("nd"); break;
			case 3: Suffix = TEXT("rd"); break;
			default: break;
			}
		}
		return FString::Printf(TEXT("%d%s"), N, Suffix);
	}
}

/* ============================================================ type, curve and glow */

FSlateFontInfo AGetawayHUD::Face(float Px, bool bBold) const
{
	// Taken off the engine's own font object and then resized, rather than asked for by name from
	// FCoreStyle the way the menu does it. The two are the same Roboto family - a UFont whose
	// RuntimeFontSource is CoreStyleDefault returns FCoreStyle::GetDefaultFont() as its composite
	// font, and "Bold" is a typeface in it - but **canvas text with no UFont behind it is dropped,
	// silently**:
	//
	//   FCanvasSimpleTextItem keeps `Font = Cast<const UFont>(InFontInfo.FontObject)`,
	//   FCanvasSimpleTextItem::HasValidText() is `return Font != nullptr`,
	//   and FCanvasTextItemBase::Draw() returns early when that is false.
	//
	// FCoreStyle::GetDefaultFontStyle builds its font info around a composite font and leaves
	// FontObject null, so the first version of this HUD drew every tile and not one letter, with
	// nothing in the log to say why. Slate widgets are fine with it - which is why the menu can do
	// it - and canvas text is not. Found 2026-11-06 by the HUD going blank.
	UFont* Legacy = GEngine ? GEngine->GetSmallFont() : nullptr;
	FSlateFontInfo Info = Legacy
		? Legacy->GetLegacySlateFontInfo()
		: FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), 10.f);

	// The size is the whole point: the glyphs are rasterised at whatever this says, so text is sharp
	// at any UIScale instead of being a stretched 10 px raster.
	Info.TypefaceFontName = bBold ? TEXT("Bold") : TEXT("Regular");
	Info.Size = FMath::Max(4.f, Px * FMath::Max(0.01f, UIScale));
	return Info;
}

FVector2D AGetawayHUD::Measure(const FString& Text, const FSlateFontInfo& Font) const
{
	if (FEngineFontServices::IsInitialized())
	{
		if (const TSharedPtr<FSlateFontMeasure> Measurer = FEngineFontServices::Get().GetFontMeasure())
		{
			return FVector2D(Measurer->Measure(Text, Font, 1.f));
		}
	}
	// There is no font service on a machine with no renderer, and nothing here is worth a crash:
	// half the point size a character is a serviceable guess for one frame of a dedicated server.
	return FVector2D(Text.Len() * Font.Size * 0.5f, Font.Size);
}

float AGetawayHUD::MeasureTo(const FString& Text, int32 Count, const FSlateFontInfo& Font) const
{
	if (Count <= 0) return 0.f;
	if (FEngineFontServices::IsInitialized())
	{
		if (const TSharedPtr<FSlateFontMeasure> Measurer = FEngineFontServices::Get().GetFontMeasure())
		{
			// Measured as a range of the one string rather than as a substring, so there is no
			// allocation per character and the kerning against the character before is kept.
			return FVector2D(Measurer->Measure(Text, 0, Count, Font, true, 1.f)).X;
		}
	}
	return Count * Font.Size * 0.5f;
}

FVector2D AGetawayHUD::Warp(float X, float Y) const
{
	if (WrapAmount <= 0.f || ScreenW < 1.f || ScreenH < 1.f)
	{
		return FVector2D(X, Y);
	}

	// How far out you are, as a fraction of half the screen. Clamped a little past the edge so a
	// glow pass or a string that overhangs does not run away with the curve.
	const float Half = ScreenW * 0.5f;
	const float Out = FMath::Clamp((X - Half) / Half, -1.3f, 1.3f);

	// Squared, so the middle of the screen is flat and the bend builds up towards the edges. This is
	// what a cylinder around the eye does: the surface leans away, and anything printed on it loses
	// height the further round it goes.
	const float Keep = 1.f - WrapAmount * Out * Out;
	const float Middle = ScreenH * 0.5f;
	return FVector2D(X, Middle + (Y - Middle) * Keep);
}

void AGetawayHUD::Blit(const FText& Text, const FSlateFontInfo& Font, float X, float Y, const FLinearColor& Colour)
{
	if (!Canvas) return;

	// Belt and braces after losing a whole HUD to this once: a font info with no UFont behind it
	// would draw nothing and say nothing, so take the old blurry path rather than go blank.
	if (!Font.FontObject)
	{
		UFont* Legacy = GEngine ? GEngine->GetSmallFont() : nullptr;
		const float Fallback = (Legacy && Legacy->LegacyFontSize > 0)
			? Font.Size / static_cast<float>(Legacy->LegacyFontSize) : 1.f;
		DrawText(Text.ToString(), Colour, X, Y, Legacy, Fallback);
		return;
	}

	FCanvasTextItem Item(FVector2D(X, Y), Text, Font, Colour);
	// The blend mode is left alone on purpose: FCanvasSimpleTextItem::GetTextBlendMode has a
	// check() that the mode is one of the translucent ones whenever the runtime font cache is in
	// use, which it always is here, so asking for additive would fire an assert rather than glow.
	Item.Scale = FVector2D(1.f, 1.f);
	Canvas->DrawItem(Item);
}

void AGetawayHUD::Glow(const FString& Text, const FSlateFontInfo& Font, float X, float Y, const FLinearColor& Colour)
{
	const int32 Rings = FMath::Clamp(GlowRings, 0, 4);
	// Widened by the text's own size against a middling 14 px line, or the banners would wear the
	// same hairline halo as a hint line and read as flat next to them. Square-rooted, because a
	// radius straight off the point size swamped the small print.
	const float Radius = S(GlowRadius) * FMath::Sqrt(FMath::Max(1.f, Font.Size / 14.f));

	// Made once and handed to all seventeen draws. FText::FromString allocates, and a halo of two
	// rings around every piece of every line is the one place on this HUD where that would add up.
	const FText Label = FText::FromString(Text);

	if (Rings > 0 && Radius > 0.f && GlowStrength > 0.f && Colour.A > 0.f)
	{
		for (int32 Ring = 1; Ring <= Rings; ++Ring)
		{
			FLinearColor Halo = Colour;
			// Falls off with the square of the ring, so ring two is a quarter the brightness of ring
			// one and ring three is barely there. Eight copies of it overlap around every glyph.
			Halo.A = Colour.A * GlowStrength / static_cast<float>(Ring * Ring);
			for (const FVector2D& Dir : HaloDirs)
			{
				Blit(Label, Font, X + Dir.X * Radius * Ring, Y + Dir.Y * Radius * Ring, Halo);
			}
		}
	}

	Blit(Label, Font, X, Y, Colour);
}

void AGetawayHUD::Bent(const FString& Text, const FSlateFontInfo& Font, float X, float Y, const FLinearColor& Colour,
	float AnchorX)
{
	if (Text.IsEmpty() || Colour.A <= 0.f) return;

	// Where the line hangs from. One height for the whole line, taken at the alignment's own anchor,
	// so a right-aligned column does not wobble as the names in it change length.
	const float Hang = (AnchorX == TNumericLimits<float>::Max()) ? X : AnchorX;
	const float Base = Warp(Hang, Y).Y;

	const float Bend = FMath::Clamp(TextBend, 0.f, 1.f);
	if (Bend <= 0.f || WrapAmount <= 0.f)
	{
		Glow(Text, Font, X, Base, Colour);
		return;
	}

	// Three samples say whether this line bends enough to be worth cutting up: its two ends and its
	// middle, because a string that straddles the centre of the screen has both ends at the same
	// height and a hump between them.
	const float Width = Measure(Text, Font).X;
	const float YA = Warp(X, Y).Y;
	const float YB = Warp(X + Width * 0.5f, Y).Y;
	const float YC = Warp(X + Width, Y).Y;
	const float Bow = (FMath::Max3(YA, YB, YC) - FMath::Min3(YA, YB, YC)) * Bend;

	// Flat enough: one draw. Most lines land here - anything short, and anything near the middle of
	// the screen - which is what keeps the glow affordable.
	if (Bow < 1.f)
	{
		Glow(Text, Font, X, Base + (YA - Base) * Bend, Colour);
		return;
	}

	// Otherwise walk it a character at a time and break wherever the bow has moved on. The step is
	// a tenth of the line's own bow, so a line is never cut into more than about ten pieces however
	// hard it curves - a 2 px joint under a glow is invisible, and a thousand draw calls for one
	// hint line is not. Each piece is drawn as a string, so spacing and kerning inside it are the
	// font's own.
	const float Step = FMath::Max(1.f, Bow * 0.1f);
	const int32 Last = Text.Len();
	int32 Start = 0;
	float StartAt = 0.f;
	float Advance = 0.f;
	float PieceY = Base;
	for (int32 i = 0; i < Last; ++i)
	{
		const float Reach = MeasureTo(Text, i + 1, Font);
		const float Here = Base + (Warp(X + (Advance + Reach) * 0.5f, Y).Y - Base) * Bend;

		if (i == Start)
		{
			PieceY = Here;
		}
		else if (FMath::Abs(Here - PieceY) >= Step)
		{
			Glow(Text.Mid(Start, i - Start), Font, X + StartAt, PieceY, Colour);
			Start = i;
			StartAt = Advance;
			PieceY = Here;
		}

		Advance = Reach;
	}
	Glow(Text.Mid(Start), Font, X + StartAt, PieceY, Colour);
}

void AGetawayHUD::Centred(const FString& Text, const FSlateFontInfo& Font, float CX, float Y, const FLinearColor& Colour)
{
	Bent(Text, Font, CX - Measure(Text, Font).X * 0.5f, Y, Colour, CX);
}

void AGetawayHUD::RightAligned(const FString& Text, const FSlateFontInfo& Font, float Right, float Y, const FLinearColor& Colour)
{
	Bent(Text, Font, Right - Measure(Text, Font).X, Y, Colour, Right);
}

/* ============================================================ the furniture */

void AGetawayHUD::DrawCrosshair(float CX, float CY)
{
	const float R = FMath::Max(1.f, S(CrosshairSize));

	// Faintest and widest first, so each pass sits under the one before it rather than tinting it.
	if (GlowStrength > 0.f && GlowRings > 0)
	{
		const float Step = FMath::Max(1.f, S(GlowRadius));
		FLinearColor Halo = FLinearColor::White;

		Halo.A = FMath::Clamp(GlowStrength * 0.7f, 0.f, 1.f);
		const float Outer = R + Step * 2.f;
		DrawRect(Halo, CX - Outer, CY - Outer, Outer * 2.f, Outer * 2.f);

		Halo.A = FMath::Clamp(GlowStrength * 1.6f, 0.f, 1.f);
		const float Inner = R + Step;
		DrawRect(Halo, CX - Inner, CY - Inner, Inner * 2.f, Inner * 2.f);
	}

	DrawRect(FLinearColor::White, CX - R, CY - R, R * 2.f, R * 2.f);
}

void AGetawayHUD::DrawVisorFrame()
{
	if (!bVisorFrame || VisorAlpha <= 0.f || ScreenW < 1.f) return;

	// Stitched out of tiles rather than drawn with DrawLine: FBatchedElements::AddLine forces alpha
	// to 1 on the way in ("some legacy code relies on Color.A being ignored", BatchedElements.cpp),
	// so a canvas line cannot be faint. Tiles are translucent, and the gaps between stitches hide
	// the fact that each one is level while the arc it belongs to is not.
	const int32 Stitches = FMath::Clamp(VisorStitches, 8, 160);
	const float Inset = ScreenW * 0.08f;
	const float Step = (ScreenW - Inset * 2.f) / static_cast<float>(Stitches);
	const float Dash = Step * 0.55f;
	const float Thick = FMath::Max(1.f, S(2.f));

	const float Rows[2] = { S(10.f), ScreenH - S(10.f) };
	for (const float RowY : Rows)
	{
		for (int32 i = 0; i < Stitches; ++i)
		{
			// Faded out at both ends, so the arc trails off instead of stopping dead.
			const float T = (Stitches > 1) ? i / static_cast<float>(Stitches - 1) : 0.5f;
			const float Fade = FMath::Clamp(FMath::Min(T, 1.f - T) / 0.18f, 0.f, 1.f);
			if (Fade <= 0.f) continue;

			const float X = Inset + Step * (i + 0.5f) - Dash * 0.5f;
			const float Y = Warp(X + Dash * 0.5f, RowY).Y;

			FLinearColor Halo = VisorColour;
			Halo.A = VisorAlpha * 0.3f * Fade;
			DrawRect(Halo, X - Thick, Y - Thick, Dash + Thick * 2.f, Thick * 3.f);

			FLinearColor Core = VisorColour;
			Core.A = VisorAlpha * Fade;
			DrawRect(Core, X, Y, Dash, Thick);
		}
	}
}

/* ============================================================ messages */

void AGetawayHUD::ShowMessage(const FString& Text, float Seconds, FLinearColor Colour, int32 Key)
{
	const UWorld* World = GetWorld();
	if (!World || Text.IsEmpty() || Seconds <= 0.f) return;

	const double Until = World->GetTimeSeconds() + Seconds;

	// A keyed line replaces its own: "Picked up C4 (carrying 2)" straight after "(carrying 1)" is a
	// correction, not a second thing that happened.
	if (Key >= 0)
	{
		for (FGetawayMessage& Existing : Messages)
		{
			if (Existing.Key == Key)
			{
				Existing.Text = Text;
				Existing.Colour = Colour;
				Existing.Until = Until;
				return;
			}
		}
	}

	Messages.Add({ Text, Colour, Until, Key });
	while (Messages.Num() > FMath::Max(1, MaxMessages))
	{
		Messages.RemoveAt(0);
	}
}

void AGetawayHUD::DrawMessages(float CX, float CY)
{
	const UWorld* World = GetWorld();
	if (!World || Messages.IsEmpty()) return;

	const double Now = World->GetTimeSeconds();
	Messages.RemoveAll([Now](const FGetawayMessage& M) { return M.Until <= Now; });

	const FSlateFontInfo Font = Face(Px::Message);
	const float Step = S(26.f);
	float Y = CY + S(48.f);

	// Lifted off the speedometer if a full stack of lines would reach it. At UIScale 1.5 on a short
	// window - a 720p Play-in-Editor one, say - the lines coming down from the crosshair and the
	// readout coming up from the bottom edge would otherwise meet in the middle.
	const float Floor = ScreenH - S(190.f);
	const float Needs = Messages.Num() * Step;
	if (Y + Needs > Floor)
	{
		Y = FMath::Max(CY + S(20.f), Floor - Needs);
	}

	for (const FGetawayMessage& Line : Messages)
	{
		FLinearColor Colour = Line.Colour;
		Colour.A = FMath::Clamp(static_cast<float>(Line.Until - Now) / FMath::Max(0.05f, MessageFadeTime), 0.f, 1.f);
		Centred(Line.Text, Font, CX, Y, Colour);
		Y += Step;
	}
}

/* ============================================================ the frame */

void AGetawayHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine) return;

	// Kept for the curve. Everything that bends reads these rather than being handed them, because
	// the bend is a property of where you are on the screen and nothing else.
	ScreenW = Canvas->ClipX;
	ScreenH = Canvas->ClipY;

	// The front end draws none of this: no crosshair over the menu, no speedometer for a camera on
	// a rail, and no scoreboard for a round nobody is playing. Checked before the crosshair rather
	// than after, because the crosshair is the one thing that is drawn unconditionally below.
	{
		const AGetawayGameState* MenuGS = GetWorld() ? GetWorld()->GetGameState<AGetawayGameState>() : nullptr;
		if (MenuGS && MenuGS->GetPhase() == EGetawayPhase::Menu) return;
	}

	const float CX = ScreenW * 0.5f;
	const float CY = ScreenH * 0.5f;

	// Under everything else, and the same in both phases: it is the surface the rest is printed on.
	DrawVisorFrame();
	DrawCrosshair(CX, CY);

	// Before either early return below, so a line still draws during placement and in the frames
	// where the pawn is being swapped over.
	DrawMessages(CX, CY);

	// Both of these replicate, so unlike the game mode they are here on a client too.
	const AGetawayGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGetawayGameState>() : nullptr;
	const AGetawayPlayerState* Mine = PlayerOwner ? PlayerOwner->GetPlayerState<AGetawayPlayerState>() : nullptr;

	DrawScoreboard(GS, Mine);

	// Drawn before the pawn branch below, because all three belong to the round rather than to
	// whatever you happen to be possessing. A player who is home, or who gave up, watches the rest
	// of the run from the fly camera and still wants the clock, their own time and the result -
	// and until this moved up here, the placement branch returned before any of it was drawn.
	if (GS && GS->GetPhase() == EGetawayPhase::Run)
	{
		DrawRunClock(GS, CX);
		DrawFinishedBanner(GS, Mine, CX);
		DrawRoundBanner(GS, Mine, CX);
	}

	if (const AGetawayPlacementPawn* Placing = Cast<AGetawayPlacementPawn>(GetOwningPawn()))
	{
		DrawPlacementHUD(Placing, GS, CX);
		return;
	}

	const AGetawayCharacter* Player = Cast<AGetawayCharacter>(GetOwningPawn());
	const UGetawayMovementComponent* Move = Player ? Player->GetGetawayMovement() : nullptr;
	if (!Move) return;

	// cm/s, because that is what every tuning property in the editor is in - RunSpeed, SmashSpeed,
	// SlideMinSpeed and the rest - so the number on screen is the number you type in the Details
	// panel. It showed the lab's Source units (cm/s / 2.5) until 2026-11-06, inherited from the
	// browser prototype, and that cost a x2.5 conversion every single time anything was tuned
	// against what the speedometer said. It also hid a real one: a slide reading "300" is 750 cm/s,
	// comfortably over a 550 smash threshold, so the threshold looked broken when it was not.
	const float Speed = Player->GetVelocity().Size2D();

	const float Bottom = ScreenH;
	// Sliding is on a cooldown, so say when the next one is ready. Without this a crouch at speed
	// that does nothing reads as a dropped input rather than as the rule. The only thing left above
	// the number, and only while the clock is running.
	const float SlideWait = Move->GetSlideCooldownLeft();
	if (SlideWait > 0.f)
	{
		Centred(FString::Printf(TEXT("slide  %.1fs"), SlideWait), Face(Px::Unit), CX, Bottom - S(128.f), Dim);
	}
	// Sat lower than it used to be, because the movement state above it and the u/s line under it
	// are both gone (2026-11-06) and it would otherwise float in the middle of an empty third of
	// the screen. See *The strip* in docs/hud.md.
	Centred(FString::Printf(TEXT("%d cm/s"), FMath::RoundToInt32(Speed)), Face(Px::Speedo, true), CX, Bottom - S(100.f), FLinearColor::White);

	// What is in your hand. One line for both items rather than one line each, because the hand
	// holds a single thing (AGetawayCharacter::ItemCapacity) and a second line reading "none" for
	// whichever you are not carrying is noise. The rope being out gets its own word, because then
	// the click in your hand is a release rather than a throw.
	const int32 Held = Player->GetChargesLeft();
	const int32 Hooks = Player->GetGrappleShotsLeft();
	const bool bRopeOut = Player->IsGrappling();

	FString HandLine;
	FLinearColor HandColour = Dim;
	if (bRopeOut)
	{
		HandLine = TEXT("GRAPPLE  out");
		HandColour = Live;
	}
	else if (Hooks > 0)
	{
		HandLine = (Hooks > 1) ? FString::Printf(TEXT("GRAPPLE  x%d"), Hooks) : FString(TEXT("GRAPPLE"));
		HandColour = Live;
	}
	else if (Held > 0)
	{
		// The count only shows up when you can hold more than one, which at ItemCapacity 1 is never.
		HandLine = (Held > 1) ? FString::Printf(TEXT("C4  x%d"), Held) : FString(TEXT("C4"));
		HandColour = Urgent;
	}
	else
	{
		HandLine = TEXT("HANDS  empty");
	}
	Bent(HandLine, Face(Px::Hand, true), S(24.f), S(24.f), HandColour);

	// There is no key list here any more (2026-11-06). It named the hook, C4's two keys and the
	// forfeit, and it sat under your hand for the whole run - but a run is where you already know
	// your keys, and the placement phase, which teaches a camera nothing else explains, keeps its
	// own. See *The strip* in docs/hud.md.

	// The run only keeps it in the corner: it was announced in the middle of the screen as the build
	// phase opened, and playing that again as you set off would be telling you the same thing twice.
	DrawEffectBanner(GS, CX, CY, -1.f, S(24.f));
}

/* ============================================================ the round's effect */

void AGetawayHUD::DrawEffectBanner(const AGetawayGameState* GS, float CX, float CY, float Elapsed, float RestX)
{
	// Big in the middle of the screen for a couple of seconds, then it travels up to the corner and
	// sits there for the rest of the round. Both halves matter: you cannot miss what everybody has
	// been given, and you are not left reading it over the top of the game.
	if (!GS || !GS->HasActiveEffect()) return;

	const EGetawayEffect Effect = GS->GetActiveEffect();
	const FString Name = AGetawayGameState::GetEffectName(Effect);

	// Timed off a replicated clock rather than a local one, so it holds for the same two seconds on
	// both machines, it cannot drift, and nothing has to tell the HUD when a round started. Negative
	// Elapsed means it is already up in the corner.
	const float Move = FMath::Max(0.f, EffectMoveTime);
	const float Held = FMath::Max(0.f, EffectBannerTime);
	const float T = (Elapsed < 0.f) ? 1.f
		: ((Move > 0.f) ? FMath::Clamp((Elapsed - Held) / Move, 0.f, 1.f) : (Elapsed >= Held ? 1.f : 0.f));
	// Eased, so it leaves the middle of the screen gently rather than snapping away from your eye.
	const float A = FMath::InterpEaseInOut(0.f, 1.f, T, 2.f);

	// Rasterised at whatever size it is passing through this frame, so the travel is sharp all the
	// way rather than a stretched 30 px face shrinking.
	const FSlateFontInfo Font = Face(FMath::Lerp(Px::EffectBig, Px::EffectRest, A), true);
	const FVector2D Size = Measure(Name, Font);
	// Measured at the size it is being drawn at this frame, so the travel reads as one movement
	// rather than as a jump at the end.
	const float X = FMath::Lerp(CX - Size.X * 0.5f, RestX, A);
	const float Y = FMath::Lerp(CY - S(110.f), S(76.f), A);

	Bent(Name, Font, X, Y, Warn);

	// The explanation is for the moment you are reading it and no longer, so it fades out with the
	// move and the name travels on alone.
	if (A < 1.f)
	{
		const FString Blurb = AGetawayGameState::GetEffectBlurb(Effect);
		FLinearColor Colour = Dim;
		Colour.A = 1.f - A;
		Centred(Blurb, Face(FMath::Lerp(Px::BannerSub, Px::Unit, A)), CX, Y + Size.Y + S(8.f), Colour);
	}
}

/* ============================================================ the scoreboard */

void AGetawayHUD::DrawScoreboard(const AGetawayGameState* GS, const AGetawayPlayerState* Mine)
{
	if (!GS) return;

	const TArray<AGetawayPlayerState*> Standings = GS->GetStandings();
	// Alone there is nothing to stand against, and the line would just sit there reading "1".
	if (Standings.Num() < 2) return;

	const FSlateFontInfo Small = Face(Px::Row);
	const FSlateFontInfo Me = Face(Px::MyRow, true);
	const float Right = ScreenW - S(32.f);
	float Y = S(24.f);

	RightAligned(FString::Printf(TEXT("ROUND %d"), GS->GetRoundNumber()), Small, Right, Y, Dim);
	Y += S(24.f);

	for (const AGetawayPlayerState* PS : Standings)
	{
		if (!PS) continue;
		// How their round is going, in a word, so you can see who is still out there.
		FString How;
		FLinearColor Colour = Dim;
		switch (PS->GetRunState())
		{
		case EGetawayRunState::Building:  How = TEXT("building"); break;
		case EGetawayRunState::Running:   How = TEXT("running"); Colour = Live; break;
		case EGetawayRunState::Finished:  How = FString::Printf(TEXT("%.2fs"), PS->GetFinishTime()); Colour = Go; break;
		case EGetawayRunState::Forfeit:   How = TEXT("out"); Colour = Urgent; break;
		case EGetawayRunState::Dead:      How = TEXT("dead"); Colour = Urgent; break;
		}

		const bool bMe = (PS == Mine);
		RightAligned(FString::Printf(TEXT("%s  %d   %s"), *PS->GetPlayerName(), PS->GetPoints(), *How),
			bMe ? Me : Small, Right, Y, bMe ? FLinearColor::White : Colour);
		Y += S(24.f);
	}
}

/* ============================================================ placing */

void AGetawayHUD::DrawPlacementHUD(const AGetawayPlacementPawn* Placing, const AGetawayGameState* GS, float CX)
{
	const float Bottom = ScreenH;

	// Who the shared build is still waiting on. Counted off the replicated PlayerArray, so a client
	// can draw it without a game mode.
	const AGetawayPlayerState* Me = Placing->GetGetawayPlayerState();
	const bool bImDone = Me && Me->IsDonePlacing();
	int32 Waiting = 0;
	if (GS)
	{
		for (const APlayerState* Base : GS->PlayerArray)
		{
			const AGetawayPlayerState* PS = Cast<AGetawayPlayerState>(Base);
			if (PS && PS->IsStillPlacing()) ++Waiting;
		}
	}

	// The build clock, top centre - the same place the run clock sits, so time is always read from
	// one spot. It turns amber at 10 s and red at 5 s, which is the only warning you get. It comes
	// off the game state, which is what makes it draw on a client at all.
	if (GS && GS->IsPlacementTimed())
	{
		const float Left = GS->GetPlacementTimeLeft();
		const FLinearColor Clock = bImDone ? Go : (Left <= 5.f ? Urgent : (Left <= 10.f ? Warn : Live));

		Centred(FString::Printf(TEXT("%.1f s"), Left), Face(Px::Clock, true), CX, S(24.f), Clock);
		Centred(bImDone ? TEXT("you are done - the clock is for the others") : TEXT("build time"),
			Face(Px::Unit), CX, S(64.f), bImDone ? Go : Dim);
	}

	// This round's effect, announced as the build phase opens: in the middle of the screen where you
	// cannot miss it, then up to the corner so it is out of the way while you build. It is said here
	// rather than at the start of the run because it is meant to change what you place.
	DrawEffectBanner(GS, CX, ScreenH * 0.5f, GS ? GS->GetPlacementElapsed() : -1.f, S(40.f));

	// The dealt hand, with the slot you are holding picked out. It is yours alone: everyone drafts
	// their own three pieces now, so what you see here is nobody else's business.
	const int32 Count = Placing->GetHandCount();
	if (Count == 0)
	{
		Bent(TEXT("Hand empty"), Face(Px::Status), CX - S(110.f), Bottom - S(120.f), Dim);
	}
	else
	{
		float Y = Bottom - S(120.f) - Count * S(24.f);
		for (int32 i = 0; i < Count; ++i)
		{
			FGetawayPieceDef Def;
			if (!Placing->GetHandPiece(i, Def)) continue;
			const bool bHeld = (i == Placing->GetHandSlot());
			const FString Line = FString::Printf(TEXT("%s %s  (%.0f x %.0f x %.0f)"),
				bHeld ? TEXT(">") : TEXT(" "), *Def.DisplayName, Def.Size.X, Def.Size.Y, Def.Size.Z);
			Bent(Line, bHeld ? Face(Px::HeldPiece, true) : Face(Px::Piece), S(40.f), Y, bHeld ? Live : Dim);
			Y += S(24.f);
		}
	}

	// Say when building is shut rather than inviting a click that the server will throw away. Two
	// different reasons for it, and they want different words: you are done and the round is still
	// waiting on somebody else, or the round has moved on without you (a forfeit hands this camera
	// back mid-run with your unspent pieces still in it).
	const FGetawayPlacement P = Placing->GetPlacement();
	const bool bCanBuild = Placing->CanBuildNow();

	// Name the buttons on the thing they are actually holding. The pawn flips this as soon as the
	// pad or the mouse is touched, so a hint that says "Click" while you hold a pad is at most one
	// twitch of the right stick out of date.
	const bool bPad = Placing->IsUsingGamepad();
	const TCHAR* PlaceVerb = bPad ? TEXT("A") : TEXT("Click");
	const TCHAR* TurnKey = bPad ? TEXT("X") : TEXT("R");

	FString Status;
	if (bImDone)
	{
		Status = (Waiting > 0)
			? FString::Printf(TEXT("Done - waiting for %d other player%s"), Waiting, Waiting == 1 ? TEXT("") : TEXT("s"))
			: TEXT("Done - here we go");
	}
	else if (!bCanBuild) Status = TEXT("Building is closed until the next round");
	else if (!P.bHasTarget) Status = P.bSurfaceMounted
		? TEXT("Aim at a wall or floor - this one has to stick to something")
		: TEXT("Aim at a surface");
	else if (P.bBlocked) Status = TEXT("Blocked - something is in the way");
	else if (P.bFloating) Status = TEXT("Nothing to build off - it has to touch something");
	else if (P.bTrimmed) Status = FString::Printf(TEXT("%s to place  -  trimmed to fit"), PlaceVerb);
	else Status = FString::Printf(TEXT("%s to place"), PlaceVerb);

	const FLinearColor StatusColour = bImDone ? Go : ((bCanBuild && P.CanPlace()) ? Live : Urgent);
	Bent(Status, Face(Px::Status), CX - S(90.f), Bottom - S(90.f), StatusColour);

	// A ramp or a bridge plays differently depending on which way round it is, so say which way.
	if (P.bDirectional)
	{
		Bent(FString::Printf(TEXT("Facing %s  -  %s turns it"), *Placing->GetFacingName(), TurnKey),
			Face(Px::Unit), CX - S(90.f), Bottom - S(66.f), Dim);
	}

	const FSlateFontInfo Hint = Face(Px::Hint);
	Bent(bPad
			? TEXT("Left stick + triggers fly  |  LB faster  |  D-pad picks a piece  |  X turns it")
			: TEXT("WASD + Space/C fly  |  Shift faster  |  wheel picks a piece  |  R turns it"),
		Hint, S(40.f), S(24.f), Dim);
	Bent(bPad
			? TEXT("A places  |  Start when you are done  |  everyone sets off together")
			: TEXT("Click places  |  Enter when you are done  |  everyone sets off together"),
		Hint, S(40.f), S(50.f), Dim);
}

/* ============================================================ running */

void AGetawayHUD::DrawRunClock(const AGetawayGameState* GS, float CX)
{
	Centred(FString::Printf(TEXT("%.2f s"), GS->GetRunTime()), Face(Px::Clock, true), CX, S(24.f),
		GS->IsRoundOver() ? Go : Dim);

	// The hurry-up, once the first player is home. It is the only thing that can end the run under
	// you, so it gets the loud colour.
	if (GS->IsRunTimed())
	{
		const float Left = GS->GetRunTimeLeft();
		const FString Line = GS->IsGraceRunning()
			? FString::Printf(TEXT("hurry  %.1f s"), Left)
			: FString::Printf(TEXT("%.0f s left"), Left);
		Centred(Line, Face(Px::Unit + 1.f), CX, S(64.f), Left <= 5.f ? Urgent : Warn);
	}
	else
	{
		const float Best = GS->GetBestTime();
		if (Best >= 0.f)
		{
			Centred(FString::Printf(TEXT("best %.2f s"), Best), Face(Px::Unit), CX, S(64.f), Dim);
		}
	}
}

void AGetawayHUD::DrawFinishedBanner(const AGetawayGameState* GS, const AGetawayPlayerState* Mine, float CX)
{
	// Once the round is over DrawRoundBanner owns this spot and says the same thing with the score
	// on it. This is only for the gap in between.
	if (!GS || GS->IsRoundOver() || !Mine || !Mine->HasFinished()) return;

	const FString Place = Mine->GetFinishPlace() > 0 ? FString::Printf(TEXT("  -  %s"), *Ordinal(Mine->GetFinishPlace())) : TEXT("");
	const FString Time = FString::Printf(TEXT("%.2f seconds%s"), Mine->GetFinishTime(), *Place);

	// Still on foot means the game mode has not lifted you out yet: this is the beat you spend
	// looking at where you landed, so it gets the full banner.
	if (!Cast<AGetawayPlacementPawn>(GetOwningPawn()))
	{
		const float Y = ScreenH * 0.34f;
		Centred(TEXT("YOU MADE IT"), Face(Px::Banner, true), CX, Y, Go);
		Centred(Time, Face(Px::BannerSub), CX, Y + S(56.f), Go);
		Centred(TEXT("watching the others..."), Face(Px::Unit), CX, Y + S(86.f), Dim);
		return;
	}

	// In the fly camera now, where the big one would sit on top of the course you are watching.
	// Under the run clock at the top, which is where the other player's remaining time already is.
	Centred(FString::Printf(TEXT("you made it  -  %s"), *Time), Face(Px::Unit + 1.f), CX, S(92.f), Go);
}

void AGetawayHUD::DrawRoundBanner(const AGetawayGameState* GS, const AGetawayPlayerState* Mine, float CX)
{
	if (!GS->IsRoundOver()) return;

	const float Y = ScreenH * 0.34f;

	const bool bHome = Mine && Mine->HasFinished();
	Centred(bHome ? TEXT("FINISHED") : TEXT("ROUND OVER"), Face(Px::Banner, true), CX, Y, bHome ? Go : Urgent);

	if (Mine)
	{
		const FString Line = bHome
			? FString::Printf(TEXT("%.2f seconds%s"), Mine->GetFinishTime(),
				(GS->GetBestTime() >= 0.f && FMath::IsNearlyEqual(GS->GetBestTime(), Mine->GetFinishTime(), 0.001f))
					? TEXT("  -  best yet") : TEXT(""))
			: TEXT("you did not make it");
		Centred(Line, Face(Px::BannerSub), CX, Y + S(56.f), bHome ? Go : Dim);

		const int32 Got = Mine->GetLastRoundPoints();
		Centred(Got > 0 ? FString::Printf(TEXT("+%d  (%d)"), Got, Mine->GetPoints())
			: FString::Printf(TEXT("no points  (%d)"), Mine->GetPoints()),
			Face(Px::BannerSub), CX, Y + S(86.f), Got > 0 ? Go : Dim);
	}

	Centred(TEXT("back to building..."), Face(Px::Unit), CX, Y + S(116.f), Dim);
}
