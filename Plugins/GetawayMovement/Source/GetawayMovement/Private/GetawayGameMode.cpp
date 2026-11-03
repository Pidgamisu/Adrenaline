#include "GetawayGameMode.h"

#include "GetawayBreakable.h"
#include "GetawayC4.h"
#include "GetawayC4Pickup.h"
#include "GetawayCharacter.h"
#include "GetawayCollapsingBridge.h"
#include "GetawayFinish.h"
#include "GetawayGameState.h"
#include "GetawayHUD.h"
#include "GetawayJumpPad.h"
#include "GetawayMenuCamera.h"
#include "GetawayPiece.h"
#include "GetawayPlacementPawn.h"
#include "GetawayPlayerController.h"
#include "GetawayPlayerState.h"
#include "GetawayRamp.h"
#include "GetawaySpikes.h"
#include "GetawayVortex.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	/** Sizes are the lab's PTYPES table in cm, with its Y-up swapped into Unreal's Z-up. */
	FGetawayPieceDef MakePiece(const TCHAR* Id, const TCHAR* Name, const FVector& Size,
		UClass* Class, bool bScale = true, bool bDirectional = false, bool bSurfaceMounted = false)
	{
		FGetawayPieceDef Def;
		Def.Id = Id;
		Def.DisplayName = Name;
		Def.Size = Size;
		Def.ActorClass = Class;
		Def.bScaleToSize = bScale;
		Def.bDirectional = bDirectional;
		Def.bSurfaceMounted = bSurfaceMounted;
		return Def;
	}
}

AGetawayGameMode::AGetawayGameMode()
{
	DefaultPawnClass = AGetawayCharacter::StaticClass();
	HUDClass = AGetawayHUD::StaticClass();
	PlacementPawnClass = AGetawayPlacementPawn::StaticClass();
	MenuPawnClass = AGetawayMenuCamera::StaticClass();

	// The front end: the menu panel, Escape, and the only place in the plugin that travels.
	PlayerControllerClass = AGetawayPlayerController::StaticClass();

	// The round used to live entirely here, where no client can see it. These two are what the
	// players actually read: the shared phase and clocks, and their own hand, run and score.
	GameStateClass = AGetawayGameState::StaticClass();
	PlayerStateClass = AGetawayPlayerState::StaticClass();

	Catalog.Add(MakePiece(TEXT("floor"), TEXT("Floor"), FVector(400.f, 400.f, 50.f), AGetawayPiece::StaticClass()));
	Catalog.Add(MakePiece(TEXT("wall"), TEXT("Wall"), FVector(400.f, 50.f, 400.f), AGetawayPiece::StaticClass()));
	Catalog.Add(MakePiece(TEXT("ledge"), TEXT("Ledge"), FVector(200.f, 200.f, 130.f), AGetawayPiece::StaticClass()));
	Catalog.Add(MakePiece(TEXT("break"), TEXT("Breakable panel"), FVector(300.f, 50.f, 400.f), AGetawayBreakable::StaticClass()));
	Catalog.Add(MakePiece(TEXT("crate"), TEXT("Crate"), FVector(100.f, 100.f, 100.f), AGetawayBreakable::StaticClass()));
	// Directional pieces run their length along X, which is the way the placement arrow points: the
	// ramp rises that way and the bridge drops that way. The lab's ramp is 4 x 6 on the floor, so it
	// is 600 along the rise and 400 across it.
	Catalog.Add(MakePiece(TEXT("ramp"), TEXT("Ramp"), FVector(600.f, 400.f, 200.f), AGetawayRamp::StaticClass(), true, true));
	Catalog.Add(MakePiece(TEXT("bridge"), TEXT("Collapsing bridge"), FVector(1200.f, 400.f, 60.f), AGetawayCollapsingBridge::StaticClass(), true, true));
	// The pickup builds its own meshes, so it is placed at its own size; Size is only the ghost.
	Catalog.Add(MakePiece(TEXT("c4"), TEXT("C4 pickup"), FVector(110.f, 110.f, 110.f), AGetawayC4Pickup::StaticClass(), false));
	// Surface mounted: the same three numbers as a wall, read as a plate - 400 x 400 across the
	// face it sticks to, 50 thick out from it. It cannot be placed without something to stick to.
	Catalog.Add(MakePiece(TEXT("spikes"), TEXT("Spikes"), FVector(400.f, 400.f, 50.f), AGetawaySpikes::StaticClass(), true, false, true));
	// Surface mounted the other way round: the spikes are what you must not touch, a jump pad is
	// what you aim for. It fires you straight out of the face it is stuck to, so the surface you
	// place it on is the launch direction. 200 x 200 rather than the bed's 400, because a pad is a
	// route you have to hit rather than a hazard you have to miss.
	Catalog.Add(MakePiece(TEXT("jumppad"), TEXT("Jump pad"), FVector(200.f, 200.f, 50.f), AGetawayJumpPad::StaticClass(), true, false, true));
	// Free-standing, not surface mounted: a column you stand in. 300 x 300 x 500 - three quarters of
	// a floor tile across, and a storey and a half tall, so it is a wide enough mouth to run into at
	// speed and long enough that how far up you come out is a decision. It lifts straight up however
	// it is placed, so it needs nothing aimed and nothing stuck to.
	Catalog.Add(MakePiece(TEXT("vortex"), TEXT("Vortex"), FVector(300.f, 300.f, 500.f), AGetawayVortex::StaticClass()));
}

AGetawayGameState* AGetawayGameMode::State() const
{
	return GetGameState<AGetawayGameState>();
}

AGetawayPlayerState* AGetawayGameMode::StateOf(const APlayerController* PC)
{
	return PC ? PC->GetPlayerState<AGetawayPlayerState>() : nullptr;
}

EGetawayPhase AGetawayGameMode::GetPhase() const
{
	const AGetawayGameState* GS = State();
	return GS ? GS->GetPhase() : EGetawayPhase::Placement;
}

void AGetawayGameMode::InitGameState()
{
	Super::InitGameState();

	// Before anybody connects, so the first client to arrive is told the right phase straight away
	// rather than assuming placement and correcting itself a frame later.
	if (AGetawayGameState* GS = State())
	{
		GS->SetPhase(ShouldShowMenu()
			? EGetawayPhase::Menu
			: (bStartInPlacement ? EGetawayPhase::Placement : EGetawayPhase::Run));
	}
}

void AGetawayGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (ShouldShowMenu())
	{
		StartMenu();
	}
	else if (bStartInPlacement)
	{
		StartPlacement();
	}
	else if (AGetawayGameState* GS = State())
	{
		GS->SetPhase(EGetawayPhase::Run);
		GS->BeginRunClock();
	}
}

bool AGetawayGameMode::ShouldShowMenu() const
{
	if (!bStartInMenu) return false;

	// A listen server or a dedicated server is a game somebody can already be in, so it is never
	// the front end. This is what keeps the two launchers in Tools/ and the two-player test in
	// docs/multiplayer.md working exactly as they did: both of them open the map with ?listen.
	if (GetNetMode() != NM_Standalone) return false;

	// And the Host button's own `?Play=1`, for a game that is being hosted but has nobody in it
	// yet - the frame or two before the net driver is up, where the net mode is still standalone.
	return !UGameplayStatics::HasOption(OptionsString, TEXT("Play"));
}

void AGetawayGameMode::StartMenu()
{
	if (AGetawayGameState* GS = State())
	{
		GS->SetPhase(EGetawayPhase::Menu);
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = Cast<APlayerController>(It->Get()))
		{
			StartMenuFor(PC);
		}
	}
}

void AGetawayGameMode::StartMenuFor(APlayerController* PC)
{
	UWorld* World = GetWorld();
	if (!World || !PC) return;

	// No tracking list, unlike the placement cameras: a menu session is standalone with one player
	// in it, and the camera is thrown away the moment Host or Join travels out of here. Where it
	// spawns does not matter either - AGetawayMenuCamera measures the level and flies its own
	// circle from BeginPlay.
	//
	// It does have to cope with being called twice for the same player, though, because it is:
	// HandleStartingNewPlayer runs for the first player *before* the game mode's BeginPlay, so
	// that player gets here once on login and again from StartMenu. Without this the second call
	// would throw away a perfectly good camera and build another.
	AGetawayMenuCamera* Camera = Cast<AGetawayMenuCamera>(PC->GetPawn());
	if (!Camera)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* Class = MenuPawnClass ? MenuPawnClass.Get() : AGetawayMenuCamera::StaticClass();
		Camera = World->SpawnActor<AGetawayMenuCamera>(
			Class, FVector::ZeroVector, FRotator::ZeroRotator, Params);
		if (!Camera) return;

		if (APawn* Old = PC->GetPawn())
		{
			PC->UnPossess();
			Old->Destroy();
		}
		PC->Possess(Camera);

		UE_LOG(LogTemp, Log, TEXT("Getaway: %s is at the menu, with %s"), *PC->GetName(), *Camera->GetName());
	}

	// The game mode puts the panel up, rather than the controller working it out for itself. The
	// phase is replicated, so a controller asking the game state in BeginPlay would be reading it
	// before the first update has landed - the same ordering trap as
	// AGetawayPlacementPawn::EnsureGhostMaterials.
	if (AGetawayPlayerController* Ours = Cast<AGetawayPlayerController>(PC))
	{
		Ours->ShowMainMenu();
	}
}

void AGetawayGameMode::Logout(AController* Exiting)
{
	if (APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		ForgetPlacementPawn(PC);
	}
	Super::Logout(Exiting);

	// Somebody quitting must not leave the round waiting on them, in either phase. Their PlayerState
	// is gone by now, so both of these are simply "is anyone left?" asked again.
	CheckEveryoneReady();
	CheckRoundOver();
}

/* ============================================================ the party box */

void AGetawayGameMode::DealHandFor(AGetawayPlayerState* PS)
{
	if (!PS) return;

	TArray<int32> Usable;
	for (int32 i = 0; i < Catalog.Num(); ++i)
	{
		if (Catalog[i].IsValid()) Usable.Add(i);
	}

	// Dealt per player, so two players no longer share one list. That sharing is what let one of
	// them spend a slot and shift the indices under the other, which happened during the spike.
	TArray<FGetawayPieceDef> Dealt;
	for (int32 n = 0; n < HandSize; ++n)
	{
		if (Usable.Num() == 0) break;
		const int32 Pick = FMath::RandRange(0, Usable.Num() - 1);
		Dealt.Add(Catalog[Usable[Pick]]);
		Usable.RemoveAtSwap(Pick);
	}
	PS->SetHand(Dealt);
}

void AGetawayGameMode::TakeFromHand(APlayerController* PC, int32 Slot)
{
	AGetawayPlayerState* PS = StateOf(PC);
	UWorld* World = GetWorld();
	if (!PS || !World) return;

	if (!PS->TakeFromHand(Slot)) return; // still holding something

	// Spending your whole hand says you are done building - there is nothing left to place. It does
	// not start your run: the build is shared, so the round sets off when the *last* player is done.
	// A short beat first, both so you watch the last piece land and because this runs from the
	// placement pawn's own click handler, which the swap to a character would pull apart.
	if (GetPhase() != EGetawayPhase::Placement || !PS->IsStillPlacing()) return;

	const float Delay = FMath::Max(0.05f, LastPieceDelay);
	TWeakObjectPtr<APlayerController> WeakPC(PC);
	World->GetTimerManager().SetTimer(PS->HandSpentTimer, FTimerDelegate::CreateWeakLambda(this, [this, WeakPC]()
	{
		if (WeakPC.IsValid()) MarkDonePlacing(WeakPC.Get());
	}), Delay, false);

	UE_LOG(LogTemp, Log, TEXT("Getaway: %s spent their hand, done building in %.2fs"), *PC->GetName(), Delay);
}

AActor* AGetawayGameMode::PlacePiece(const FGetawayPieceDef& Def, const FVector& Centre,
	const FRotator& Rotation, AGetawayPlayerState* Placer, const FVector& SizeOverride)
{
	UWorld* World = GetWorld();
	if (!World || !Def.IsValid()) return nullptr;
	// The game mode only exists on the server, so this is already server-only; say so anyway, since
	// it is the one function that creates shared level geometry.
	if (!HasAuthority()) return nullptr;

	// Spawn with the scale already in the transform rather than scaling afterwards. An actor's spawn
	// transform travels with the actor to the clients; a later SetActorScale3D does not, so a client
	// would see every piece as a 100 cm cube. It also means AGetawayRamp::OnConstruction builds its
	// wedge at the right scale first time instead of being rebuilt behind it.
	const FVector Size = SizeOverride.IsNearlyZero() ? Def.Size : SizeOverride;
	const FVector Scale = Def.bScaleToSize ? Size / 100.f : FVector::OneVector;
	const FTransform Where(Rotation, Centre, Scale);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Spawned = World->SpawnActor<AActor>(Def.ActorClass, Where, Params);
	if (!Spawned) return nullptr;

	// Remembered so a kill by this piece can be credited to the player who dropped it. Spikes are
	// what finally made that possible; see RegisterTrapKill.
	PlacedPieces.Add(Spawned);
	PlacedBy.Add(Placer);

	UE_LOG(LogTemp, Log, TEXT("Getaway: %s placed %s at %s"),
		Placer ? *Placer->GetPlayerName() : TEXT("someone"), *Def.Id.ToString(), *Centre.ToCompactString());
	return Spawned;
}

AGetawayPlayerState* AGetawayGameMode::GetPiecePlacer(const AActor* Piece) const
{
	if (!Piece) return nullptr;
	for (int32 i = 0; i < PlacedPieces.Num(); ++i)
	{
		if (PlacedPieces[i] == Piece && PlacedBy.IsValidIndex(i))
		{
			return PlacedBy[i];
		}
	}
	return nullptr;
}

void AGetawayGameMode::RegisterTrapKill(AGetawayCharacter* Victim, const AActor* Piece)
{
	if (!HasAuthority() || !Victim) return;
	AGetawayPlayerState* Placer = GetPiecePlacer(Piece);
	if (!Placer) return;
	// Your own trap catching you is bad luck, not a point.
	if (Placer == Victim->GetPlayerState<AGetawayPlayerState>()) return;

	Placer->AddTrapKill();
	UE_LOG(LogTemp, Log, TEXT("Getaway: %s's piece killed %s"), *Placer->GetPlayerName(), *Victim->GetName());
}

/* ============================================================ placement cameras */

AGetawayPlacementPawn* AGetawayGameMode::FindPlacementPawn(const APlayerController* PC) const
{
	for (int32 i = 0; i < PlacementOwners.Num(); ++i)
	{
		if (PlacementOwners[i] == PC && PlacementPawns.IsValidIndex(i) && IsValid(PlacementPawns[i]))
		{
			return PlacementPawns[i];
		}
	}
	return nullptr;
}

void AGetawayGameMode::RememberPlacementPawn(APlayerController* PC, AGetawayPlacementPawn* Pawn)
{
	ForgetPlacementPawn(PC);
	PlacementOwners.Add(PC);
	PlacementPawns.Add(Pawn);
}

void AGetawayGameMode::ForgetPlacementPawn(const APlayerController* PC)
{
	for (int32 i = PlacementOwners.Num() - 1; i >= 0; --i)
	{
		if (PlacementOwners[i] == PC || !IsValid(PlacementOwners[i]))
		{
			PlacementOwners.RemoveAt(i);
			if (PlacementPawns.IsValidIndex(i))
			{
				PlacementPawns.RemoveAt(i);
			}
		}
	}
}

/* ============================================================ phases */

void AGetawayGameMode::StartPlacementFor(APlayerController* PC)
{
	UWorld* World = GetWorld();
	if (!World || !PC) return;

	AGetawayPlacementPawn* Pawn = FindPlacementPawn(PC);
	if (!Pawn)
	{
		// Start where that player is, or above their PlayerStart on the first phase.
		FVector Where(0.f, 0.f, 500.f);
		FRotator Facing = FRotator::ZeroRotator;
		if (APawn* Current = PC->GetPawn())
		{
			Where = Current->GetActorLocation() + FVector(0.f, 0.f, 300.f);
			Facing = Current->GetActorRotation();
		}
		else if (AActor* Start = FindPlayerStart(PC))
		{
			Where = Start->GetActorLocation() + FVector(0.f, 0.f, 300.f);
			Facing = Start->GetActorRotation();
		}

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		UClass* Class = PlacementPawnClass ? PlacementPawnClass.Get() : AGetawayPlacementPawn::StaticClass();
		Pawn = World->SpawnActor<AGetawayPlacementPawn>(Class, Where, Facing, Params);
		if (!Pawn) return;
		RememberPlacementPawn(PC, Pawn);
	}

	if (APawn* Old = PC->GetPawn())
	{
		if (Old != Pawn)
		{
			PC->UnPossess();
			Old->Destroy();
		}
	}
	PC->Possess(Pawn);
	PC->SetControlRotation(Pawn->GetActorRotation());

	const AGetawayPlayerState* PS = StateOf(PC);
	UE_LOG(LogTemp, Log, TEXT("Getaway: %s is building, with %s and %d piece(s)"),
		*PC->GetName(), *Pawn->GetName(), PS ? PS->GetHandCount() : 0);
}

void AGetawayGameMode::StartPlacement()
{
	UWorld* World = GetWorld();
	AGetawayGameState* GS = State();
	if (!World || !GS) return;

	// Whatever got us here - a finished round, the linger timer, a restart - no clock from the last
	// round may still be pending.
	World->GetTimerManager().ClearTimer(PlacementTimer);
	World->GetTimerManager().ClearTimer(RunTimer);
	World->GetTimerManager().ClearTimer(LingerTimer);

	GS->BumpRound();
	GS->SetPhase(EGetawayPhase::Placement);
	GS->SetRoundOver(false);
	GS->ClearRunDeadline();
	FinishersThisRound = 0;

	int32 Players = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = Cast<APlayerController>(It->Get());
		AGetawayPlayerState* PS = StateOf(PC);
		if (!PS) continue;
		World->GetTimerManager().ClearTimer(PS->HandSpentTimer);
		PS->BeginRound();
		DealHandFor(PS);
		++Players;
	}

	// A new round starts on an intact course: put the breakables and any fallen bridge back, so you
	// are building against the level as the next run will actually meet it rather than against the
	// wreckage of the last one. Pieces anyone placed stay - those are the point.
	//
	// Thrown C4 is swept up here rather than left lying about. A charge is spent the moment it
	// leaves your hand, so a brick still stuck to a wall from last round is not a resource anybody
	// can spend - it is a live bomb with no owner, and its blast would land in a round nobody
	// threw it in. The charges you are *carrying* are untouched; those you went and collected.
	int32 Swept = 0;
	for (TActorIterator<AGetawayC4> It(World); It; ++It)
	{
		It->Destroy();
		++Swept;
	}
	if (Swept > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Getaway: cleared %d unexploded charge(s) from last round"), Swept);
	}

	int32 Rebuilt = 0;
	for (TActorIterator<AGetawayBreakable> It(World); It; ++It)
	{
		if (It->IsBroken()) ++Rebuilt;
		It->Restore();
	}
	for (TActorIterator<AGetawayCollapsingBridge> It(World); It; ++It)
	{
		It->Restore();
	}
	for (TActorIterator<AGetawaySpikes> It(World); It; ++It)
	{
		It->SetArmed(false);
	}
	for (TActorIterator<AGetawayJumpPad> It(World); It; ++It)
	{
		It->SetArmed(false);
	}
	for (TActorIterator<AGetawayVortex> It(World); It; ++It)
	{
		It->SetArmed(false);
	}
	if (Rebuilt > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Getaway: restored %d breakable(s) for the new round"), Rebuilt);
	}

	// Start the build clock. It is the only thing that ends placement for everybody, so an empty
	// hand dealt from an empty catalog still gets you out rather than stranding you in the fly
	// camera. It lives on the game state, which is how a client can draw it at all.
	GS->BeginPlacementClock(PlacementTime);
	if (PlacementTime > 0.f)
	{
		World->GetTimerManager().SetTimer(PlacementTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			UE_LOG(LogTemp, Log, TEXT("Getaway: build time up"));
			StartRun();
		}), PlacementTime, false);
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = Cast<APlayerController>(It->Get()))
		{
			StartPlacementFor(PC);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("Getaway: round %d, placement for %d player(s), %d piece(s) each, %.0fs to build"),
		GS->GetRoundNumber(), Players, HandSize, PlacementTime);
}

void AGetawayGameMode::StartRunFor(APlayerController* PC)
{
	UWorld* World = GetWorld();
	if (!World || !PC) return;

	AGetawayPlacementPawn* Pawn = FindPlacementPawn(PC);
	// No placement camera means this player is already running. Spawning them another character
	// here would snatch the one they are using out from under them.
	if (!Pawn) return;

	if (AGetawayPlayerState* PS = StateOf(PC))
	{
		// Whichever of the three routes into the run got here - Enter, the build clock, the hand
		// emptying - the others must not fire into a run that has already started.
		World->GetTimerManager().ClearTimer(PS->HandSpentTimer);
		PS->SetRunState(EGetawayRunState::Running);
	}

	AActor* Start = FindPlayerStart(PC);
	const FVector Where = Start ? Start->GetActorLocation() : FVector(0.f, 0.f, 200.f);
	const FRotator Facing = Start ? FRotator(0.f, Start->GetActorRotation().Yaw, 0.f) : FRotator::ZeroRotator;

	if (APawn* Old = PC->GetPawn())
	{
		PC->UnPossess();
		if (Old != Pawn)
		{
			Old->Destroy();
		}
	}
	Pawn->Destroy();
	ForgetPlacementPawn(PC);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	UClass* Class = DefaultPawnClass ? DefaultPawnClass.Get() : AGetawayCharacter::StaticClass();
	if (APawn* Runner = World->SpawnActor<APawn>(Class, Where, Facing, Params))
	{
		PC->Possess(Runner);
		PC->SetControlRotation(Facing);
		UE_LOG(LogTemp, Log, TEXT("Getaway: %s is running as %s"), *PC->GetName(), *Runner->GetName());
	}

	if (GetPhase() != EGetawayPhase::Run)
	{
		BeginRun();
	}
}

void AGetawayGameMode::StartRun()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Stop the build clock first: StartRun is reachable from Enter and from the clock running out,
	// and whichever got here, the other must not fire later.
	World->GetTimerManager().ClearTimer(PlacementTimer);

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		if (APlayerController* PC = Cast<APlayerController>(It->Get()))
		{
			StartRunFor(PC);
		}
	}

	if (GetPhase() != EGetawayPhase::Run)
	{
		BeginRun();
	}
}

/* ============================================================ waiting for everybody */

int32 AGetawayGameMode::CountStillPlacing() const
{
	const AGetawayGameState* GS = State();
	if (!GS) return 0;

	int32 Waiting = 0;
	for (const APlayerState* Base : GS->PlayerArray)
	{
		const AGetawayPlayerState* PS = Cast<AGetawayPlayerState>(Base);
		if (PS && PS->IsStillPlacing()) ++Waiting;
	}
	return Waiting;
}

void AGetawayGameMode::MarkDonePlacing(APlayerController* PC)
{
	AGetawayPlayerState* PS = StateOf(PC);
	if (!PS || GetPhase() != EGetawayPhase::Placement || !PS->IsStillPlacing()) return;

	PS->SetDonePlacing(true);
	if (UWorld* World = GetWorld())
	{
		// Whichever route got here - the hand emptying or Enter - the other must not fire after it.
		World->GetTimerManager().ClearTimer(PS->HandSpentTimer);
	}

	const int32 Waiting = CountStillPlacing();
	UE_LOG(LogTemp, Log, TEXT("Getaway: %s is done building%s"), *PS->GetPlayerName(),
		Waiting > 0 ? *FString::Printf(TEXT(", waiting on %d more"), Waiting) : TEXT(" - everybody is ready"));

	CheckEveryoneReady();
}

void AGetawayGameMode::CheckEveryoneReady()
{
	const AGetawayGameState* GS = State();
	if (!GS || GetPhase() != EGetawayPhase::Placement) return;

	// Nobody to be ready yet: an empty server must not set a run going on its own, and on a listen
	// server BeginPlay runs before the host's own PlayerState exists.
	int32 Players = 0;
	for (const APlayerState* Base : GS->PlayerArray)
	{
		if (Cast<AGetawayPlayerState>(Base)) ++Players;
	}
	if (Players == 0 || CountStillPlacing() > 0) return;

	UE_LOG(LogTemp, Log, TEXT("Getaway: everybody has built, the round sets off"));
	StartRun();
}

void AGetawayGameMode::BeginRun()
{
	UWorld* World = GetWorld();
	AGetawayGameState* GS = State();
	if (!World || !GS) return;

	GS->SetPhase(EGetawayPhase::Run);
	GS->SetRoundOver(false);
	GS->BeginRunClock();
	FinishersThisRound = 0;

	// Done once per run however many players trigger the switch: the clock is shared, so everyone's
	// time is measured from the same instant and the times are comparable.
	int32 Objectives = 0;
	for (TActorIterator<AGetawayFinish> It(World); It; ++It)
	{
		It->Arm();
		++Objectives;
	}
	// Traps are live only while somebody is running. Between rounds everyone is a collisionless fly
	// pawn and could not set them off anyway, but a disarmed trap is one that cannot surprise us.
	for (TActorIterator<AGetawaySpikes> It(World); It; ++It)
	{
		It->SetArmed(true);
	}
	for (TActorIterator<AGetawayJumpPad> It(World); It; ++It)
	{
		It->SetArmed(true);
	}
	for (TActorIterator<AGetawayVortex> It(World); It; ++It)
	{
		It->SetArmed(true);
	}
	if (Objectives == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("Getaway: no AGetawayFinish in the level, so the run cannot end. "
			"Drop one in, or place the Finish piece."));
	}

	ApplyRunDeadline(RunTimeLimit, false);
}

void AGetawayGameMode::ApplyRunDeadline(float Seconds, bool bGrace)
{
	UWorld* World = GetWorld();
	AGetawayGameState* GS = State();
	if (!World || !GS || Seconds <= 0.f) return;

	// The game state holds the deadline so clients can count down to it; the timer here is what
	// actually calls the run. Setting the timer from what the game state ended up with keeps the
	// two from drifting apart when the grace shortens an existing limit.
	GS->SetRunDeadline(Seconds, bGrace);
	if (!GS->IsRunTimed()) return;

	World->GetTimerManager().SetTimer(RunTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		EndRound(true);
	}), FMath::Max(0.05f, GS->GetRunTimeLeft()), false);
}

void AGetawayGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Spawns and possesses the default pawn, i.e. a runner.
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// Nobody is playing: this is the front end, so swap the runner for the orbiting camera and put
	// the panel up. Reached on the first player of a menu session, since BeginPlay runs before any
	// player exists.
	if (GetPhase() == EGetawayPhase::Menu)
	{
		StartMenuFor(NewPlayer);
		return;
	}

	// A player who joins while everyone else is building should be building too, not standing on the
	// start line. This is also what gets the second PIE player a placement camera at all: BeginPlay
	// ran before they existed. They get a hand of their own here, since the deal already happened.
	if (bStartInPlacement && GetPhase() == EGetawayPhase::Placement)
	{
		if (AGetawayPlayerState* PS = StateOf(NewPlayer))
		{
			PS->BeginRound();
			DealHandFor(PS);
		}
		StartPlacementFor(NewPlayer);
	}
	else if (AGetawayPlayerState* PS = StateOf(NewPlayer))
	{
		// Joining mid-run: they run this one, from the start line like everybody else.
		PS->BeginRound();
		PS->SetRunState(EGetawayRunState::Running);
	}
}

/* ============================================================ the objective */

void AGetawayGameMode::ReachedFinish(AGetawayCharacter* Runner)
{
	AGetawayGameState* GS = State();
	if (!HasAuthority() || !GS || !Runner) return;
	if (GS->GetPhase() != EGetawayPhase::Run || GS->IsRoundOver()) return;

	AGetawayPlayerState* PS = Runner->GetPlayerState<AGetawayPlayerState>();
	// Not running means either they are already home - the trigger fires every time you pass
	// through it - or they gave up. Either way there is nothing to record.
	if (!PS || !PS->IsRunning()) return;

	const float Time = GS->GetRunTime();
	++FinishersThisRound;
	PS->MarkFinished(Time, FinishersThisRound);
	GS->ReportFinishTime(Time);

	UE_LOG(LogTemp, Log, TEXT("Getaway: %s finished %d%s in %.2fs"),
		*PS->GetPlayerName(), FinishersThisRound,
		FinishersThisRound == 1 ? TEXT("st") : (FinishersThisRound == 2 ? TEXT("nd") : TEXT("th")), Time);

	// First one home starts the hurry-up, so a round is not held open by someone picking their way
	// across. It can only bring the end nearer, never push it back.
	if (FinishersThisRound == 1)
	{
		ApplyRunDeadline(FinishGraceTime, true);
	}

	CheckRoundOver();
}

void AGetawayGameMode::GiveUpRunFor(APlayerController* PC)
{
	AGetawayPlayerState* PS = StateOf(PC);
	if (!PS) return;

	// Dropping out of a run forfeits it: you keep the points you had, you score nothing this round,
	// and the others are not left waiting on you.
	if (PS->IsRunning())
	{
		PS->SetRunState(EGetawayRunState::Forfeit);
		UE_LOG(LogTemp, Log, TEXT("Getaway: %s gave up on this run"), *PS->GetPlayerName());
	}
	StartPlacementFor(PC);
	CheckRoundOver();
}

void AGetawayGameMode::PlayerDied(APlayerController* PC, AActor* KilledBy)
{
	UWorld* World = GetWorld();
	AGetawayPlayerState* PS = StateOf(PC);
	if (!World || !HasAuthority() || !PS) return;
	if (GetPhase() != EGetawayPhase::Run || !PS->IsRunning()) return;

	// With respawning on, the run state does not change at all: you are still Running, so the
	// round stays open for you and CheckRoundOver below is a no-op. Only the old behaviour ends
	// the run here.
	if (!bRespawnOnDeath)
	{
		PS->SetRunState(EGetawayRunState::Dead);
	}
	UE_LOG(LogTemp, Log, TEXT("Getaway: %s was killed by %s%s"), *PS->GetPlayerName(),
		KilledBy ? *KilledBy->GetName() : TEXT("something"),
		bRespawnOnDeath ? TEXT(" - back to the start") : TEXT(" - out until the next round"));

	// Credit whoever placed it. RegisterTrapKill already refuses to pay a player for their own
	// trap catching them - and this is its first live caller, the hook having been dead since it
	// was written, because until spikes there was nothing in the game that could kill you.
	RegisterTrapKill(Cast<AGetawayCharacter>(PC ? PC->GetPawn() : nullptr), KilledBy);

	// The move is deferred for two reasons. One is feel: a beat looking at what got you beats
	// being yanked away mid-impact. The other is that Die() can be called from inside the
	// victim's own overlap callback, and either route destroys that pawn.
	//
	// Which route it is, is read now rather than when the timer fires, so flipping the knob
	// mid-flight cannot land a player halfway between the two.
	TWeakObjectPtr<APlayerController> WeakPC(PC);
	const bool bRespawn = bRespawnOnDeath;
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakPC, bRespawn]()
	{
		// Skipped if the round ended meanwhile - StartPlacement puts everybody in a camera anyway.
		if (!WeakPC.IsValid() || GetPhase() != EGetawayPhase::Run) return;

		if (bRespawn)
		{
			RespawnRunner(WeakPC.Get());
		}
		else
		{
			StartPlacementFor(WeakPC.Get());
		}
	}), FMath::Max(0.05f, DeathLingerTime), false);

	CheckRoundOver();
}

void AGetawayGameMode::RespawnRunner(APlayerController* PC)
{
	UWorld* World = GetWorld();
	AGetawayPlayerState* PS = StateOf(PC);
	if (!World || !PC || !HasAuthority() || !PS) return;

	// Only a live run gets a respawn. A player who has finished, forfeited or is back in the fly
	// camera must not be handed a character at the start line.
	if (GetPhase() != EGetawayPhase::Run || !PS->IsRunning()) return;

	AActor* Start = FindPlayerStart(PC);
	const FVector Where = Start ? Start->GetActorLocation() : FVector(0.f, 0.f, 200.f);
	const FRotator Facing = Start ? FRotator(0.f, Start->GetActorRotation().Yaw, 0.f) : FRotator::ZeroRotator;

	// A fresh character rather than teleporting the old one. The corpse has had its movement
	// switched off and its bDead flag set by Die(), and a new pawn is the same path the run itself
	// starts down in StartRunFor - no half-reset state to get wrong, at the cost of your C4.
	if (APawn* Old = PC->GetPawn())
	{
		PC->UnPossess();
		Old->Destroy();
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	UClass* Class = DefaultPawnClass ? DefaultPawnClass.Get() : AGetawayCharacter::StaticClass();
	if (APawn* Runner = World->SpawnActor<APawn>(Class, Where, Facing, Params))
	{
		PC->Possess(Runner);
		PC->SetControlRotation(Facing);
		UE_LOG(LogTemp, Log, TEXT("Getaway: %s respawned at the start as %s"), *PC->GetName(),
			*Runner->GetName());
	}
}

void AGetawayGameMode::CheckRoundOver()
{
	AGetawayGameState* GS = State();
	if (!GS || GS->GetPhase() != EGetawayPhase::Run || GS->IsRoundOver()) return;

	bool bAnyUnfinished = false;
	bool bAnyParticipant = false;
	for (APlayerState* Base : GS->PlayerArray)
	{
		const AGetawayPlayerState* PS = Cast<AGetawayPlayerState>(Base);
		if (!PS) continue;
		// Building covers the window between one player starting early and the build clock firing:
		// the round must not be called while somebody has not even set off.
		if (PS->IsRunning() || PS->IsBuilding()) bAnyUnfinished = true;
		if (!PS->IsBuilding()) bAnyParticipant = true;
	}

	if (bAnyUnfinished || !bAnyParticipant) return;
	EndRound(false);
}

void AGetawayGameMode::EndRound(bool bOutOfTime)
{
	UWorld* World = GetWorld();
	AGetawayGameState* GS = State();
	if (!World || !GS || GS->IsRoundOver()) return;

	World->GetTimerManager().ClearTimer(RunTimer);

	// Out of time: anyone still out there, or still in the fly camera, is simply not home.
	for (APlayerState* Base : GS->PlayerArray)
	{
		if (AGetawayPlayerState* PS = Cast<AGetawayPlayerState>(Base))
		{
			if (PS->IsRunning() || PS->IsBuilding())
			{
				PS->SetRunState(EGetawayRunState::Forfeit);
			}
		}
	}

	GS->ClearRunDeadline();
	GS->SetRoundOver(true);
	ScoreRound();

	UE_LOG(LogTemp, Log, TEXT("Getaway: round %d over%s"), GS->GetRoundNumber(),
		bOutOfTime ? TEXT(" (time)") : TEXT(""));

	// Let the result sit on screen, then build the next one. Runners keep control meanwhile, so
	// overshooting the finish at speed still looks like you ran through it rather than hit a wall.
	World->GetTimerManager().SetTimer(LingerTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		StartPlacement();
	}), FMath::Max(0.01f, FinishLingerTime), false);
}

/* ============================================================ scoring */

void AGetawayGameMode::ScoreRound()
{
	AGetawayGameState* GS = State();
	if (!GS) return;

	TArray<AGetawayPlayerState*> Players;
	int32 Finishers = 0;
	for (APlayerState* Base : GS->PlayerArray)
	{
		if (AGetawayPlayerState* PS = Cast<AGetawayPlayerState>(Base))
		{
			Players.Add(PS);
			if (PS->HasFinished()) ++Finishers;
		}
	}
	if (Players.Num() == 0) return;

	const bool bMulti = Players.Num() > 1;

	// Ultimate Chicken Horse's central rule: everybody got there, so the level was too easy and
	// nobody is rewarded for it. Alone there is nobody to compare against, so it does not apply.
	const bool bNullified = bNoPointsIfAllFinish && bMulti && (Finishers == Players.Num());

	for (AGetawayPlayerState* PS : Players)
	{
		int32 Points = 0;

		// A point for getting there, a point for getting there first. That is the whole system.
		if (PS->HasFinished() && !bNullified)
		{
			Points += GoalPoints;
			// Place 1 whether or not anybody else made it: if you were the only one home, you were
			// still first home. The old table paid a separate, larger bonus for being alone.
			if (PS->GetFinishPlace() == 1)
			{
				Points += FirstGoalBonus;
			}
		}

		// Trap kills are counted but deliberately pay nothing: points come from the objective and
		// nowhere else. Killing somebody already helps you by keeping them off the scoreboard.
		PS->SetLastRoundPoints(Points);
		if (Points != 0)
		{
			PS->AwardPoints(Points);
		}

		UE_LOG(LogTemp, Log, TEXT("Getaway:   %s %s%s  +%d  (%d)"),
			*PS->GetPlayerName(),
			PS->HasFinished() ? TEXT("home") : TEXT("did not finish"),
			PS->HasFinished() ? *FString::Printf(TEXT(" in %.2fs"), PS->GetFinishTime()) : TEXT(""),
			Points, PS->GetPoints());
	}

	if (bNullified)
	{
		UE_LOG(LogTemp, Log, TEXT("Getaway: everybody got there, so nobody scores"));
	}
}
