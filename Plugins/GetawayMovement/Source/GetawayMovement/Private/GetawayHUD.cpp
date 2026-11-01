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

void AGetawayHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine) return;

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	DrawRect(FLinearColor::White, CX - 2.f, CY - 2.f, 4.f, 4.f);

	// Both of these replicate, so unlike the game mode they are here on a client too.
	const AGetawayGameState* GS = GetWorld() ? GetWorld()->GetGameState<AGetawayGameState>() : nullptr;
	const AGetawayPlayerState* Mine = PlayerOwner ? PlayerOwner->GetPlayerState<AGetawayPlayerState>() : nullptr;

	DrawScoreboard(GS, Mine);

	if (const AGetawayPlacementPawn* Placing = Cast<AGetawayPlacementPawn>(GetOwningPawn()))
	{
		DrawPlacementHUD(Placing, GS, CX);
		return;
	}

	if (GS && GS->GetPhase() == EGetawayPhase::Run)
	{
		DrawRunClock(GS, CX);
		DrawRoundBanner(GS, Mine, CX);
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

	// What you are holding, and nothing else. It used to read "C4  1 / 2" - the count over the
	// carry cap - which was reported as the HUD claiming you had two charges when you had one. The
	// cap now only shows up as the word "full", where it is the useful thing to know.
	const int32 Held = Player->GetChargesLeft();
	const FString C4Line = (Held <= 0)
		? FString(TEXT("C4  none"))
		: FString::Printf(TEXT("C4  x%d%s"), Held, (Held >= Player->MaxCharges) ? TEXT("  full") : TEXT(""));
	DrawText(C4Line, (Held > 0) ? Urgent : Dim, 24.f, 24.f, Small, 1.4f);
	DrawText(Player->IsUsingGamepad()
			? TEXT("RB throw C4  |  RT detonate  |  View gives up the run")
			: TEXT("G throw C4  |  F detonate  |  Backspace gives up the run"),
		Dim, 24.f, 50.f, Small, 1.f);
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
