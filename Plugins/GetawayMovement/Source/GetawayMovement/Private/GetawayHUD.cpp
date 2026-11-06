#include "GetawayHUD.h"

#include "GetawayCharacter.h"
#include "GetawayGameState.h"
#include "GetawayMovementComponent.h"
#include "GetawayPlacementPawn.h"
#include "GetawayPlayerState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
	const FLinearColor Dim(0.64f, 0.66f, 0.71f);
	const FLinearColor Live(0.37f, 0.77f, 0.84f);
	const FLinearColor Warn(0.94f, 0.70f, 0.24f);
	const FLinearColor Urgent(1.f, 0.42f, 0.3f);
	const FLinearColor Go(0.31f, 0.82f, 0.58f);

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

void AGetawayHUD::Centred(const FString& Text, UFont* Font, float Scale, float CX, float Y, const FLinearColor& Color)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, CX - W * 0.5f, Y, Font, Scale);
}

void AGetawayHUD::RightAligned(const FString& Text, UFont* Font, float Scale, float Right, float Y, const FLinearColor& Color)
{
	float W = 0.f, H = 0.f;
	GetTextSize(Text, W, H, Font, Scale);
	DrawText(Text, Color, Right - W, Y, Font, Scale);
}

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

	UFont* Small = GEngine->GetSmallFont();
	float Y = CY + 48.f;
	for (const FGetawayMessage& Line : Messages)
	{
		FLinearColor Colour = Line.Colour;
		Colour.A = FMath::Clamp(static_cast<float>(Line.Until - Now) / FMath::Max(0.05f, MessageFadeTime), 0.f, 1.f);
		Centred(Line.Text, Small, 1.3f, CX, Y, Colour);
		Y += 26.f;
	}
}

void AGetawayHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine) return;

	// The front end draws none of this: no crosshair over the menu, no speedometer for a camera on
	// a rail, and no scoreboard for a round nobody is playing. Checked before the crosshair rather
	// than after, because the crosshair is the one thing that is drawn unconditionally below.
	{
		const AGetawayGameState* MenuGS = GetWorld() ? GetWorld()->GetGameState<AGetawayGameState>() : nullptr;
		if (MenuGS && MenuGS->GetPhase() == EGetawayPhase::Menu) return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	DrawRect(FLinearColor::White, CX - 2.f, CY - 2.f, 4.f, 4.f);

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

	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();

	const int32 Ups = FMath::RoundToInt32(Player->GetVelocity().Size2D() / 2.5f);
	const FString State = Move->GetStateName();
	FLinearColor StateColor = Dim;
	if (State == TEXT("SLIDE")) StateColor = Warn;
	else if (State == TEXT("WALLRUN")) StateColor = Live;
	else if (State == TEXT("MANTLE")) StateColor = Go;
	else if (State == TEXT("AIR")) StateColor = FLinearColor::White;

	const float Bottom = Canvas->ClipY;
	// Sliding is on a cooldown, so say when the next one is ready. Without this a crouch at speed
	// that does nothing reads as a dropped input rather than as the rule.
	const float SlideWait = Move->GetSlideCooldownLeft();
	if (SlideWait > 0.f)
	{
		Centred(FString::Printf(TEXT("slide  %.1fs"), SlideWait), Small, 1.1f, CX, Bottom - 172.f, Dim);
	}
	Centred(State, Small, 1.4f, CX, Bottom - 150.f, StateColor);
	Centred(FString::Printf(TEXT("%d u/s"), Ups), Big, 2.2f, CX, Bottom - 125.f, FLinearColor::White);
	Centred(FString::Printf(TEXT("%.1f m/s"), Player->GetVelocity().Size2D() / 100.f), Small, 1.1f, CX, Bottom - 62.f, Dim);

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
	DrawText(HandLine, HandColour, 24.f, 24.f, Small, 1.4f);

	// The keys, and only the ones that are any use to you: the hook's key when you are holding one
	// or swinging on it, C4's when you are carrying a charge or have one stuck to something.
	const bool bPad = Player->IsUsingGamepad();
	FString Hint;
	if (Hooks > 0 || bRopeOut)
	{
		Hint += bPad ? TEXT("LT hook  |  ") : TEXT("Left click hook  |  ");
	}
	Hint += bPad
		? TEXT("RB throw C4  |  RT detonate  |  View gives up the run")
		: TEXT("G throw C4  |  F detonate  |  Backspace gives up the run");
	DrawText(Hint, Dim, 24.f, 50.f, Small, 1.f);

	// The run only keeps it in the corner: it was announced in the middle of the screen as the build
	// phase opened, and playing that again as you set off would be telling you the same thing twice.
	DrawEffectBanner(GS, CX, CY, -1.f, 24.f);
}

/* ============================================================ the round's effect */

void AGetawayHUD::DrawEffectBanner(const AGetawayGameState* GS, float CX, float CY, float Elapsed, float RestX)
{
	// Big in the middle of the screen for a couple of seconds, then it travels up to the corner and
	// sits there for the rest of the round. Both halves matter: you cannot miss what everybody has
	// been given, and you are not left reading it over the top of the game.
	if (!GS || !GS->HasActiveEffect()) return;

	// The large font for the name, which spends two seconds at twice the size of anything else on
	// screen: the small one does not have the pixels for it and goes soft when it is blown up.
	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();
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

	const float Scale = FMath::Lerp(1.9f, 1.f, A);
	float W = 0.f, H = 0.f;
	GetTextSize(Name, W, H, Big, Scale);
	// Measured at the size it is being drawn at this frame, so the travel reads as one movement
	// rather than as a jump at the end.
	const float X = FMath::Lerp(CX - W * 0.5f, RestX, A);
	const float Y = FMath::Lerp(CY - 110.f, 76.f, A);

	DrawText(Name, Warn, X, Y, Big, Scale);

	// The explanation is for the moment you are reading it and no longer, so it fades out with the
	// move and the name travels on alone.
	if (A < 1.f)
	{
		const FString Blurb = AGetawayGameState::GetEffectBlurb(Effect);
		FLinearColor Colour = Dim;
		Colour.A = 1.f - A;
		Centred(Blurb, Small, FMath::Lerp(1.4f, 1.1f, A), CX, Y + H + 8.f, Colour);
	}
}

/* ============================================================ the scoreboard */

void AGetawayHUD::DrawScoreboard(const AGetawayGameState* GS, const AGetawayPlayerState* Mine)
{
	if (!GS) return;

	const TArray<AGetawayPlayerState*> Standings = GS->GetStandings();
	// Alone there is nothing to stand against, and the line would just sit there reading "1".
	if (Standings.Num() < 2) return;

	UFont* Small = GEngine->GetSmallFont();
	const float Right = Canvas->ClipX - 32.f;
	float Y = 24.f;

	RightAligned(FString::Printf(TEXT("ROUND %d"), GS->GetRoundNumber()), Small, 1.1f, Right, Y, Dim);
	Y += 22.f;

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
			Small, bMe ? 1.3f : 1.1f, Right, Y, bMe ? FLinearColor::White : Colour);
		Y += 22.f;
	}
}

/* ============================================================ placing */

void AGetawayHUD::DrawPlacementHUD(const AGetawayPlacementPawn* Placing, const AGetawayGameState* GS, float CX)
{
	UFont* Small = GEngine->GetSmallFont();
	const float Bottom = Canvas->ClipY;

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
		UFont* Big = GEngine->GetLargeFont();
		const float Left = GS->GetPlacementTimeLeft();
		const FLinearColor Clock = bImDone ? Go : (Left <= 5.f ? Urgent : (Left <= 10.f ? Warn : Live));

		Centred(FString::Printf(TEXT("%.1f s"), Left), Big, 1.6f, CX, 24.f, Clock);
		Centred(bImDone ? TEXT("you are done - the clock is for the others") : TEXT("build time"),
			Small, 1.1f, CX, 64.f, bImDone ? Go : Dim);
	}

	// This round's effect, announced as the build phase opens: in the middle of the screen where you
	// cannot miss it, then up to the corner so it is out of the way while you build. It is said here
	// rather than at the start of the run because it is meant to change what you place.
	DrawEffectBanner(GS, CX, Canvas->ClipY * 0.5f, GS ? GS->GetPlacementElapsed() : -1.f, 40.f);

	// The dealt hand, with the slot you are holding picked out. It is yours alone: everyone drafts
	// their own three pieces now, so what you see here is nobody else's business.
	const int32 Count = Placing->GetHandCount();
	if (Count == 0)
	{
		DrawText(TEXT("Hand empty"), Dim, CX - 110.f, Bottom - 120.f, Small, 1.3f);
	}
	else
	{
		float Y = Bottom - 120.f - Count * 22.f;
		for (int32 i = 0; i < Count; ++i)
		{
			FGetawayPieceDef Def;
			if (!Placing->GetHandPiece(i, Def)) continue;
			const bool bHeld = (i == Placing->GetHandSlot());
			const FString Line = FString::Printf(TEXT("%s %s  (%.0f x %.0f x %.0f)"),
				bHeld ? TEXT(">") : TEXT(" "), *Def.DisplayName, Def.Size.X, Def.Size.Y, Def.Size.Z);
			DrawText(Line, bHeld ? Live : Dim, 40.f, Y, Small, bHeld ? 1.4f : 1.1f);
			Y += 22.f;
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
	DrawText(Status, StatusColour, CX - 90.f, Bottom - 90.f, Small, 1.3f);

	// A ramp or a bridge plays differently depending on which way round it is, so say which way.
	if (P.bDirectional)
	{
		DrawText(FString::Printf(TEXT("Facing %s  -  %s turns it"), *Placing->GetFacingName(), TurnKey),
			Dim, CX - 90.f, Bottom - 68.f, Small, 1.1f);
	}

	DrawText(bPad
			? TEXT("Left stick + triggers fly  |  LB faster  |  D-pad picks a piece  |  X turns it")
			: TEXT("WASD + Space/C fly  |  Shift faster  |  wheel picks a piece  |  R turns it"),
		Dim, 40.f, 24.f, Small, 1.f);
	DrawText(bPad
			? TEXT("A places  |  Start when you are done  |  everyone sets off together")
			: TEXT("Click places  |  Enter when you are done  |  everyone sets off together"),
		Dim, 40.f, 48.f, Small, 1.f);
}

/* ============================================================ running */

void AGetawayHUD::DrawRunClock(const AGetawayGameState* GS, float CX)
{
	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();

	Centred(FString::Printf(TEXT("%.2f s"), GS->GetRunTime()), Big, 1.6f, CX, 24.f,
		GS->IsRoundOver() ? Go : Dim);

	// The hurry-up, once the first player is home. It is the only thing that can end the run under
	// you, so it gets the loud colour.
	if (GS->IsRunTimed())
	{
		const float Left = GS->GetRunTimeLeft();
		const FString Line = GS->IsGraceRunning()
			? FString::Printf(TEXT("hurry  %.1f s"), Left)
			: FString::Printf(TEXT("%.0f s left"), Left);
		Centred(Line, Small, 1.2f, CX, 64.f, Left <= 5.f ? Urgent : Warn);
	}
	else
	{
		const float Best = GS->GetBestTime();
		if (Best >= 0.f)
		{
			Centred(FString::Printf(TEXT("best %.2f s"), Best), Small, 1.1f, CX, 64.f, Dim);
		}
	}
}

void AGetawayHUD::DrawFinishedBanner(const AGetawayGameState* GS, const AGetawayPlayerState* Mine, float CX)
{
	// Once the round is over DrawRoundBanner owns this spot and says the same thing with the score
	// on it. This is only for the gap in between.
	if (!GS || GS->IsRoundOver() || !Mine || !Mine->HasFinished()) return;

	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();

	const FString Place = Mine->GetFinishPlace() > 0 ? FString::Printf(TEXT("  -  %s"), *Ordinal(Mine->GetFinishPlace())) : TEXT("");
	const FString Time = FString::Printf(TEXT("%.2f seconds%s"), Mine->GetFinishTime(), *Place);

	// Still on foot means the game mode has not lifted you out yet: this is the beat you spend
	// looking at where you landed, so it gets the full banner.
	if (!Cast<AGetawayPlacementPawn>(GetOwningPawn()))
	{
		const float Y = Canvas->ClipY * 0.34f;
		Centred(TEXT("YOU MADE IT"), Big, 2.6f, CX, Y, Go);
		Centred(Time, Small, 1.5f, CX, Y + 56.f, Go);
		Centred(TEXT("watching the others..."), Small, 1.1f, CX, Y + 84.f, Dim);
		return;
	}

	// In the fly camera now, where the big one would sit on top of the course you are watching.
	// Under the run clock at the top, which is where the other player's remaining time already is.
	Centred(FString::Printf(TEXT("you made it  -  %s"), *Time), Small, 1.2f, CX, 92.f, Go);
}

void AGetawayHUD::DrawRoundBanner(const AGetawayGameState* GS, const AGetawayPlayerState* Mine, float CX)
{
	if (!GS->IsRoundOver()) return;

	UFont* Big = GEngine->GetLargeFont();
	UFont* Small = GEngine->GetSmallFont();
	const float Y = Canvas->ClipY * 0.34f;

	const bool bHome = Mine && Mine->HasFinished();
	Centred(bHome ? TEXT("FINISHED") : TEXT("ROUND OVER"), Big, 2.6f, CX, Y, bHome ? Go : Urgent);

	if (Mine)
	{
		const FString Line = bHome
			? FString::Printf(TEXT("%.2f seconds%s"), Mine->GetFinishTime(),
				(GS->GetBestTime() >= 0.f && FMath::IsNearlyEqual(GS->GetBestTime(), Mine->GetFinishTime(), 0.001f))
					? TEXT("  -  best yet") : TEXT(""))
			: TEXT("you did not make it");
		Centred(Line, Small, 1.5f, CX, Y + 56.f, bHome ? Go : Dim);

		const int32 Got = Mine->GetLastRoundPoints();
		Centred(Got > 0 ? FString::Printf(TEXT("+%d  (%d)"), Got, Mine->GetPoints())
			: FString::Printf(TEXT("no points  (%d)"), Mine->GetPoints()),
			Small, 1.4f, CX, Y + 84.f, Got > 0 ? Go : Dim);
	}

	Centred(TEXT("back to building..."), Small, 1.1f, CX, Y + 112.f, Dim);
}
