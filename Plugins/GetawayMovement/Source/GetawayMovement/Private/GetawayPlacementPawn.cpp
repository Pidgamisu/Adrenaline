#include "GetawayPlacementPawn.h"
#include "GetawayBreakable.h"
#include "GetawayGameMode.h"
#include "GetawayGameState.h"
#include "GetawayJumpPad.h"
#include "GetawayPlayerState.h"
#include "GetawayRamp.h"
#include "GetawaySpikes.h"
#include "GetawayStickInput.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AGetawayPlacementPawn::AGetawayPlacementPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	// A fly camera should pass through geometry, so the pawn has no collision at all.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bUsePawnControlRotation = true;
	Camera->SetFieldOfView(BaseFOV);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = true;
	bUseControllerRotationRoll = false;

	// It replicates, but not its transform: the fly camera is driven locally and nobody else needs
	// to see where you are shopping from. Nor does anyone else need the pawn itself, which is why
	// it goes only to its owner - without that, every client holds a copy of every other player's
	// placement camera for no reason. What you drafted is no longer carried here: the hand lives on
	// AGetawayPlayerState, so it survives the pawn being destroyed at the start of your run.
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	SetReplicateMovement(false);
}

AGetawayPlayerState* AGetawayPlacementPawn::GetGetawayPlayerState() const
{
	// GetPlayerState() is set on possession and replicates, but there is a frame or two on a
	// joining client where the pawn has arrived and the PlayerState has not. The controller is the
	// other way round often enough that it is worth asking both.
	if (AGetawayPlayerState* PS = GetPlayerState<AGetawayPlayerState>())
	{
		return PS;
	}
	return Controller ? Controller->GetPlayerState<AGetawayPlayerState>() : nullptr;
}

bool AGetawayPlacementPawn::CanBuildNow() const
{
	const UWorld* World = GetWorld();
	const AGetawayGameState* GS = World ? World->GetGameState<AGetawayGameState>() : nullptr;
	// No game state yet is the first frame or two of a join, before anything has replicated. Allow
	// it: the server checks this again anyway, and refusing here would eat an honest opening click.
	if (!GS) return true;
	if (GS->GetPhase() != EGetawayPhase::Placement || GS->IsRoundOver()) return false;

	// Saying you are done is final for the round: the others are now waiting on nobody, and letting
	// you carry on building while they wait would make the shared phase meaningless.
	const AGetawayPlayerState* PS = GetGetawayPlayerState();
	return !PS || !PS->IsDonePlacing();
}

int32 AGetawayPlacementPawn::GetHandCount() const
{
	const AGetawayPlayerState* PS = GetGetawayPlayerState();
	return PS ? PS->GetHandCount() : 0;
}

bool AGetawayPlacementPawn::GetHandPiece(int32 Slot, FGetawayPieceDef& OutDef) const
{
	const AGetawayPlayerState* PS = GetGetawayPlayerState();
	return PS && PS->GetHandPiece(Slot, OutDef);
}

void AGetawayPlacementPawn::BeginPlay()
{
	Super::BeginPlay();
	if (Camera)
	{
		Camera->SetFieldOfView(BaseFOV);
	}

#if !UE_BUILD_SHIPPING
	// Two headless clients cannot click, so the net test drives a placement from a timer instead.
	FString Mode;
	if (FParse::Value(FCommandLine::Get(), TEXT("GetawayNetTest="), Mode))
	{
		FTimerHandle Place, Count;
		if (Mode.Equals(TEXT("place"), ESearchCase::IgnoreCase))
		{
			GetWorldTimerManager().SetTimer(Place,
				FTimerDelegate::CreateUObject(this, &AGetawayPlacementPawn::NetSelfTestPlace, true), 6.f, false);
		}
		GetWorldTimerManager().SetTimer(Count, this, &AGetawayPlacementPawn::NetSelfTestCount, 14.f, false);
	}
#endif
}

#if !UE_BUILD_SHIPPING
void AGetawayPlacementPawn::NetSelfTestPlace(bool bThenTryAgain)
{
	if (!IsLocallyControlled()) return;

	// Straight out in front, in open air, so the server's own overlap test has nothing to catch on.
	const FVector Centre = GetActorLocation() + GetActorForwardVector() * 900.f;

	if (!bThenTryAgain)
	{
		// The second pass: ask again for the same spot, which now has a piece in it. The slot the
		// first pass spent is gone too, so this exercises both of the server's refusals at once.
		UE_LOG(LogTemp, Log, TEXT("Getaway net test: asking again for the spot that is now taken"));
		ServerPlacePiece(0, Centre, FRotator::ZeroRotator, FVector::ZeroVector);
		return;
	}

	// Only the three replicated piece classes prove anything; the rest of the catalog is out of
	// scope for the spike, so skip a hand that happens to hold none of them. A piece that is not a
	// plain 100 cm cube is worth more here, because then the log also shows whether the scale
	// survived the trip - a crate at scale 1 would look right either way.
	int32 Slot = INDEX_NONE;
	FGetawayPieceDef Chosen;
	const int32 Count = GetHandCount();
	for (int32 i = 0; i < Count; ++i)
	{
		FGetawayPieceDef Def;
		if (!GetHandPiece(i, Def)) continue;
		const UClass* C = Def.ActorClass.Get();
		if (!C || !(C->IsChildOf(AGetawayPiece::StaticClass())
			|| C->IsChildOf(AGetawayBreakable::StaticClass())
			|| C->IsChildOf(AGetawayRamp::StaticClass())))
		{
			continue;
		}
		if (Slot == INDEX_NONE)
		{
			Slot = i;
			Chosen = Def;
		}
		if (Def.bScaleToSize && !Def.Size.Equals(FVector(100.f), 1.f))
		{
			Slot = i;
			Chosen = Def;
			break;
		}
	}
	if (Slot == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("Getaway net test: nothing in this hand replicates, not placing"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("Getaway net test: asking the server for '%s' at %s"),
		*Chosen.Id.ToString(), *Centre.ToCompactString());
	ServerPlacePiece(Slot, Centre, FRotator::ZeroRotator, FVector::ZeroVector);

	FTimerHandle Again;
	GetWorldTimerManager().SetTimer(Again,
		FTimerDelegate::CreateUObject(this, &AGetawayPlacementPawn::NetSelfTestPlace, false), 3.f, false);
}

void AGetawayPlacementPawn::NetSelfTestCount()
{
	UWorld* World = GetWorld();
	if (!World) return;

	int32 Found = 0;
	FString What;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A->IsA(AGetawayPiece::StaticClass()) && !A->IsA(AGetawayBreakable::StaticClass())
			&& !A->IsA(AGetawayRamp::StaticClass()))
		{
			continue;
		}
		// Level-placed actors are startup actors; a piece someone placed this round is not. Only the
		// latter had to cross the wire.
		if (A->IsNetStartupActor()) continue;
		++Found;
		What += FString::Printf(TEXT("  [%s at %s scale %s]"), *A->GetClass()->GetName(),
			*A->GetActorLocation().ToCompactString(), *A->GetActorScale3D().ToCompactString());
	}
	UE_LOG(LogTemp, Log, TEXT("Getaway net test: %d placed piece(s) visible here:%s"), Found, *What);
}
#endif

/* ============================================================ input */

void AGetawayPlacementPawn::BuildDefaultInput()
{
	if (MappingContext) return;

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};
	MoveAction      = MakeAction(TEXT("IA_PlaceMove"), EInputActionValueType::Axis2D);
	UpDownAction    = MakeAction(TEXT("IA_PlaceUpDown"), EInputActionValueType::Axis1D);
	// Separate actions for the pad, for the same reasons as on the character: a stick is a rate
	// where a mouse is a delta, and the stick's dead zone must not reshape the keyboard's input.
	MoveStickAction   = MakeAction(TEXT("IA_PlaceMoveStick"), EInputActionValueType::Axis2D);
	UpDownStickAction = MakeAction(TEXT("IA_PlaceUpDownStick"), EInputActionValueType::Axis1D);
	LookStickAction   = MakeAction(TEXT("IA_PlaceLookStick"), EInputActionValueType::Axis2D);
	GamepadProbeAction = MakeAction(TEXT("IA_PlacePadProbe"), EInputActionValueType::Boolean);
	LookAction      = MakeAction(TEXT("IA_PlaceLook"), EInputActionValueType::Axis2D);
	FastAction      = MakeAction(TEXT("IA_PlaceFast"), EInputActionValueType::Boolean);
	PlaceAction     = MakeAction(TEXT("IA_PlacePlace"), EInputActionValueType::Boolean);
	RotateAction    = MakeAction(TEXT("IA_PlaceRotate"), EInputActionValueType::Boolean);
	NextPieceAction = MakeAction(TEXT("IA_PlaceNext"), EInputActionValueType::Boolean);
	PrevPieceAction = MakeAction(TEXT("IA_PlacePrev"), EInputActionValueType::Boolean);
	StartRunAction  = MakeAction(TEXT("IA_PlaceStartRun"), EInputActionValueType::Boolean);

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_GetawayPlacement"));

	// WASD into one 2D axis: X = right, Y = forward, same shape as the character's move action.
	{
		FEnhancedActionKeyMapping& M = MappingContext->MapKey(MoveAction, EKeys::W);
		M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
	}
	{
		FEnhancedActionKeyMapping& M = MappingContext->MapKey(MoveAction, EKeys::S);
		M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(this));
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	MappingContext->MapKey(MoveAction, EKeys::D);
	{
		FEnhancedActionKeyMapping& M = MappingContext->MapKey(MoveAction, EKeys::A);
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}

	MappingContext->MapKey(UpDownAction, EKeys::SpaceBar);
	{
		FEnhancedActionKeyMapping& M = MappingContext->MapKey(UpDownAction, EKeys::C);
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}

	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(FastAction, EKeys::LeftShift);
	MappingContext->MapKey(PlaceAction, EKeys::LeftMouseButton);
	MappingContext->MapKey(RotateAction, EKeys::R);
	MappingContext->MapKey(NextPieceAction, EKeys::MouseScrollUp);
	MappingContext->MapKey(PrevPieceAction, EKeys::MouseScrollDown);
	MappingContext->MapKey(StartRunAction, EKeys::Enter);

	// The pad. Up and down go on the analogue triggers rather than two buttons, so you can ease a
	// piece into place; A places, because A is the confirm button everywhere else; Start says you
	// are done building, which is the same promise Enter makes.
	MappingContext->MapKey(MoveStickAction, EKeys::Gamepad_Left2D);
	MappingContext->MapKey(LookStickAction, EKeys::Gamepad_Right2D);
	MappingContext->MapKey(UpDownStickAction, EKeys::Gamepad_RightTriggerAxis);
	{
		FEnhancedActionKeyMapping& M = MappingContext->MapKey(UpDownStickAction, EKeys::Gamepad_LeftTriggerAxis);
		M.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
	MappingContext->MapKey(FastAction, EKeys::Gamepad_LeftShoulder);
	MappingContext->MapKey(PlaceAction, EKeys::Gamepad_FaceButton_Bottom);
	MappingContext->MapKey(RotateAction, EKeys::Gamepad_FaceButton_Left);
	MappingContext->MapKey(NextPieceAction, EKeys::Gamepad_DPad_Right);
	MappingContext->MapKey(PrevPieceAction, EKeys::Gamepad_DPad_Left);
	MappingContext->MapKey(StartRunAction, EKeys::Gamepad_Special_Right);

	for (const FKey& Key : GetawayStick::ProbeKeys())
	{
		MappingContext->MapKey(GamepadProbeAction, Key);
	}
}

void AGetawayPlacementPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildDefaultInput();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				// Mapping contexts live on the local player, not the pawn, so the other pawn's
				// context is still registered when we take over - and its actions consume the same
				// WASD and Mouse2D keys. Leaving it in place swallows all movement input, because
				// the pawn that would have handled it has been destroyed. Clear before adding.
				Subsystem->ClearAllMappings();
				Subsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("GetawayPlacementPawn needs Enhanced Input. Set Project Settings > Input > Default Input Component Class to EnhancedInputComponent."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGetawayPlacementPawn::OnMove);
	Input->BindAction(MoveAction, ETriggerEvent::Completed, this, &AGetawayPlacementPawn::OnMove);
	Input->BindAction(UpDownAction, ETriggerEvent::Triggered, this, &AGetawayPlacementPawn::OnUpDown);
	Input->BindAction(UpDownAction, ETriggerEvent::Completed, this, &AGetawayPlacementPawn::OnUpDown);
	Input->BindAction(MoveStickAction, ETriggerEvent::Triggered, this, &AGetawayPlacementPawn::OnMoveStick);
	Input->BindAction(MoveStickAction, ETriggerEvent::Completed, this, &AGetawayPlacementPawn::OnMoveStick);
	Input->BindAction(UpDownStickAction, ETriggerEvent::Triggered, this, &AGetawayPlacementPawn::OnUpDownStick);
	Input->BindAction(UpDownStickAction, ETriggerEvent::Completed, this, &AGetawayPlacementPawn::OnUpDownStick);
	Input->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &AGetawayPlacementPawn::OnLookStick);
	Input->BindAction(GamepadProbeAction, ETriggerEvent::Started, this, &AGetawayPlacementPawn::OnGamepadUsed);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGetawayPlacementPawn::OnLook);
	Input->BindAction(FastAction, ETriggerEvent::Started, this, &AGetawayPlacementPawn::OnFast);
	Input->BindAction(FastAction, ETriggerEvent::Completed, this, &AGetawayPlacementPawn::OnSlow);
	Input->BindAction(PlaceAction, ETriggerEvent::Started, this, &AGetawayPlacementPawn::OnPlace);
	Input->BindAction(RotateAction, ETriggerEvent::Started, this, &AGetawayPlacementPawn::OnRotate);
	Input->BindAction(NextPieceAction, ETriggerEvent::Started, this, &AGetawayPlacementPawn::OnNextPiece);
	Input->BindAction(PrevPieceAction, ETriggerEvent::Started, this, &AGetawayPlacementPawn::OnPrevPiece);
	Input->BindAction(StartRunAction, ETriggerEvent::Started, this, &AGetawayPlacementPawn::OnStartRun);
}

void AGetawayPlacementPawn::OnMove(const FInputActionValue& Value) { MoveInput = Value.Get<FVector2D>(); }
void AGetawayPlacementPawn::OnUpDown(const FInputActionValue& Value) { UpInput = Value.Get<float>(); }
void AGetawayPlacementPawn::OnFast(const FInputActionValue&) { bFast = true; }
void AGetawayPlacementPawn::OnSlow(const FInputActionValue&) { bFast = false; }

void AGetawayPlacementPawn::OnMoveStick(const FInputActionValue& Value)
{
	bUsingGamepad = true;
	StickMoveInput = GetawayStick::ApplyDeadZone(Value.Get<FVector2D>(), MoveStickDeadZone);
}

void AGetawayPlacementPawn::OnUpDownStick(const FInputActionValue& Value)
{
	bUsingGamepad = true;
	StickUpInput = GetawayStick::ApplyDeadZone(Value.Get<float>(), TriggerDeadZone);
}

void AGetawayPlacementPawn::OnGamepadUsed(const FInputActionValue&)
{
	bUsingGamepad = true;
}

void AGetawayPlacementPawn::OnLook(const FInputActionValue& Value)
{
	if (!Controller) return;
	// Any mouse movement at all means a mouse is what you are holding; the pad side is the probe.
	bUsingGamepad = false;
	const FVector2D Delta = Value.Get<FVector2D>();
	const float K = MouseSensitivity * 0.022f / FMath::Max(MouseInputScale, 0.0001f);
	FRotator R = Controller->GetControlRotation();
	R.Yaw += Delta.X * K;
	R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch + Delta.Y * K), -89.f, 89.f);
	Controller->SetControlRotation(R);
}

void AGetawayPlacementPawn::OnLookStick(const FInputActionValue& Value)
{
	if (!Controller) return;
	bUsingGamepad = true;

	const UWorld* World = GetWorld();
	const float Dt = World ? World->GetDeltaSeconds() : 0.f;
	if (Dt <= 0.f) return;

	const FVector2D Stick = GetawayStick::ApplyCurve(
		GetawayStick::ApplyDeadZone(Value.Get<FVector2D>(), StickLookDeadZone), StickLookExponent);
	if (Stick.IsNearlyZero()) return;

	// No spin-up here, unlike the character: this camera is for aiming at a grid square, and a
	// ramp means the first moments of a nudge move you less than you asked for.
	FRotator R = Controller->GetControlRotation();
	R.Yaw += Stick.X * StickLookYawSpeed * Dt;
	// Flipped, the same way and for the same reason as AGetawayCharacter::OnLookStick. Keep in step.
	const float PitchStep = (bInvertStickY ? 1.f : -1.f) * Stick.Y * StickLookPitchSpeed * Dt;
	R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch + PitchStep), -89.f, 89.f);
	Controller->SetControlRotation(R);
}

void AGetawayPlacementPawn::OnRotate(const FInputActionValue&)
{
	ManualRotation = (ManualRotation + 1) % 4;
}

void AGetawayPlacementPawn::OnNextPiece(const FInputActionValue&)
{
	const int32 Num = GetHandCount();
	if (Num > 0) HandSlot = (HandSlot + 1) % Num;
}

void AGetawayPlacementPawn::OnPrevPiece(const FInputActionValue&)
{
	const int32 Num = GetHandCount();
	if (Num > 0) HandSlot = (HandSlot + Num - 1) % Num;
}

void AGetawayPlacementPawn::OnStartRun(const FInputActionValue&)
{
	// Only this player stops building. Whoever else is still placing keeps their clock.
	ServerStartRun();
}

void AGetawayPlacementPawn::ServerStartRun_Implementation()
{
	UWorld* World = GetWorld();
	AGetawayGameMode* GM = World ? World->GetAuthGameMode<AGetawayGameMode>() : nullptr;
	if (!GM) return;
	// Enter means "I have finished building", not "start my run". The placement phase is shared, so
	// this only lifts the round's wait on you; everybody sets off together once the last one is in.
	GM->MarkDonePlacing(Cast<APlayerController>(GetController()));
}

void AGetawayPlacementPawn::OnPlace(const FInputActionValue&)
{
	// Checked here only to save a pointless round trip; the server's copy of this is the one that
	// actually decides, since a client could skip straight to the RPC.
	if (!CanBuildNow()) return;

	FGetawayPieceDef Def;
	if (!CurrentPiece(Def)) return;

	// Place the ghost you clicked on, not a fresh answer to the same question.
	//
	// This used to re-run ComputePlacement here, and a click is handled a hair after the tick that
	// drew the ghost, by which time the mouse has moved a pixel or two. For a surface-mounted piece
	// that is enough to land the aim point in the next grid cell along, and the trim starts from
	// the cell you are pointing at: near the edge of a wall or a ledge one cell is backed and its
	// neighbour is not, so the answer flipped between the frame you saw and the frame you clicked.
	// A cyan box did nothing, with nothing in the log, because the request was never sent.
	//
	// Tick recomputes Planned every frame and the ghost is drawn from it, so this is exactly what
	// was on screen. The slot is checked because scrolling the hand in the same frame as the click
	// would otherwise send one piece's box with another piece's slot.
	FGetawayPlacement P = (PlannedSlot == HandSlot) ? Planned : ComputePlacement(Def);
	if (!P.CanPlace()) return;

	// The client decides where it is aiming, which only it can know, but it does not place anything:
	// it asks. The server re-checks the fit and spawns the actual piece. The rotation goes with it
	// rather than a quarter-turn count, because a surface-mounted piece is turned by the face it
	// landed on and the server has no way to work that out from a number of turns - and the trimmed
	// size goes too, for the same reason, with the server checking rather than trusting it.
	ServerPlacePiece(HandSlot, P.Centre, P.Rotation, P.LocalSize);
}

void AGetawayPlacementPawn::ServerPlacePiece_Implementation(int32 Slot, FVector_NetQuantize100 Centre,
	FRotator Rotation, FVector_NetQuantize100 LocalSize)
{
	UWorld* World = GetWorld();
	AGetawayGameMode* GM = World ? World->GetAuthGameMode<AGetawayGameMode>() : nullptr;
	AGetawayPlayerState* PS = GetGetawayPlayerState();
	if (!GM || !PS) return;

	// Building is a phase, not a camera. Forfeiting a run with Backspace hands this pawn back while
	// the round is still going, with any unspent pieces still in the hand - without this a player
	// who dropped out could keep dropping walls into a course the others are still running.
	if (!CanBuildNow())
	{
		UE_LOG(LogTemp, Log, TEXT("Getaway: refused a placement from %s, the build phase is over"),
			*PS->GetPlayerName());
		return;
	}

	// The piece comes from the server's copy of *this player's* hand, not from anything the client
	// sent, so a client cannot place a piece it was never dealt, one from somebody else's hand, or
	// one that is not in the catalog at all.
	FGetawayPieceDef Def;
	if (!PS->GetHandPiece(Slot, Def))
	{
		UE_LOG(LogTemp, Warning, TEXT("Getaway: %s asked for hand slot %d, which it does not have"),
			*GetName(), Slot);
		return;
	}

	// An upright free-standing piece is only ever turned about Z in quarter turns, so the server
	// snaps it back to one rather than trusting whatever arrived. Two kinds are taken as sent
	// instead, because the server cannot re-derive either without knowing which face the client
	// was looking at: a surface-mounted piece, and - since pieces started lying down on slopes -
	// one that arrives tilted. That is the same trust the aim point already gets, and the aim
	// point is the stronger of the two: a rotation alone cannot put a piece anywhere.
	FRotator Turned = Rotation;
	if (!Def.bSurfaceMounted)
	{
		const float Upright = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(SlantTolerance, 0.5f, 45.f)));
		if (Rotation.RotateVector(FVector::UpVector).Z >= Upright)
		{
			const int32 Quarter = ((FMath::RoundToInt(Rotation.Yaw / 90.f) % 4) + 4) % 4;
			Turned = FRotator(0.f, Quarter * 90.f, 0.f);
		}
	}

	// The client worked out the trim; the server decides whether to believe it. A size can only
	// ever shrink - clamped to the catalog - and whatever is left has to be backed by something,
	// which is the check the trim was doing in the first place. Lie about either and you get
	// nothing. A piece that is not surface mounted has no trim and keeps its catalog size.
	FVector Local = Def.Size;
	if (Def.bSurfaceMounted)
	{
		Local = FVector(
			FMath::Clamp(LocalSize.X, 1.f, Def.Size.X),
			FMath::Clamp(LocalSize.Y, 1.f, Def.Size.Y),
			Def.Size.Z);
	}

	if (IsBlockedAt(Centre, Turned, Local))
	{
		// The client thought it was clear. Either it is lagging behind another player's piece, or it
		// is lying; either way the server's reading is the one that counts.
		UE_LOG(LogTemp, Log, TEXT("Getaway: refused %s at %s, something is already there"),
			*Def.Id.ToString(), *Centre.ToCompactString());
		return;
	}

	if (Def.bSurfaceMounted && !IsBackedAcross(Centre, Turned, Local, Def.Size))
	{
		UE_LOG(LogTemp, Log, TEXT("Getaway: refused %s at %s, it is not backed by anything"),
			*Def.Id.ToString(), *Centre.ToCompactString());
		return;
	}

	// Checked here and not only on the client, because this is the one that counts: the aim point
	// is taken on trust, so without this a client could ask for a piece anywhere in open sky.
	if (!Def.bSurfaceMounted && !IsTouchingSomething(Centre, Turned, Local))
	{
		UE_LOG(LogTemp, Log, TEXT("Getaway: refused %s at %s, it is not touching anything"),
			*Def.Id.ToString(), *Centre.ToCompactString());
		return;
	}

	// Worked out here rather than sent, so a client cannot nominate what its piece is glued to.
	AActor* Host = FindHostAt(Centre, Turned, Def);

	if (AActor* Spawned = GM->PlacePiece(Def, Centre, Turned, PS, Local))
	{
		if (AGetawaySpikes* Spikes = Cast<AGetawaySpikes>(Spawned))
		{
			Spikes->AttachToHost(Host);
		}
		else if (AGetawayJumpPad* Pad = Cast<AGetawayJumpPad>(Spawned))
		{
			Pad->AttachToHost(Host);
		}
		GM->TakeFromHand(Cast<APlayerController>(GetController()), Slot);
	}
}

/* ============================================================ placement */

int32 AGetawayPlacementPawn::GetRotationSteps() const
{
	// Face-relative, like the lab's (yawDir() + ed.rot) % 4: a piece starts aligned to the way you
	// are looking, so turning round and placing a wall gives you a wall across your view either way.
	const float Yaw = Controller ? Controller->GetControlRotation().Yaw : GetActorRotation().Yaw;
	const int32 Facing = ((FMath::RoundToInt(Yaw / 90.f) % 4) + 4) % 4;
	return (Facing + ManualRotation) % 4;
}

FString AGetawayPlacementPawn::GetFacingName() const
{
	// The lab's DIRNAME, read into Unreal's axes: +X north, +Y east.
	static const TCHAR* Names[] = { TEXT("north"), TEXT("east"), TEXT("south"), TEXT("west") };
	return Names[FMath::Clamp(GetRotationSteps(), 0, 3)];
}

bool AGetawayPlacementPawn::CurrentPiece(FGetawayPieceDef& OutDef) const
{
	return GetHandPiece(HandSlot, OutDef);
}

FVector AGetawayPlacementPawn::WorldSizeFor(const FVector& LocalSize, const FRotator& Rotation)
{
	// The lab's sizeFor(): the size stops being the piece's own extent and becomes the world-space
	// one. Taken off the rotation rather than off the quarter turn, because a surface-mounted piece
	// can be stood on end against a wall, and then it is Z and not Y that swaps.
	//
	// These are the exact axis-aligned bounds of the turned box: every world axis picks up a contribution from
	// all three of the piece's own axes. Rotating the extent and taking the absolute value gives
	// the same answer only for multiples of 90 degrees, which stopped being the whole story once
	// pieces started lying down on slopes.
	const FQuat Q = Rotation.Quaternion();
	const FVector Ax = Q.GetAxisX();
	const FVector Ay = Q.GetAxisY();
	const FVector Az = Q.GetAxisZ();
	const FVector H = LocalSize * 0.5f;
	FVector Bounds;
	for (int32 i = 0; i < 3; ++i)
	{
		Bounds[i] = 2.f * (FMath::Abs(Ax[i]) * H.X + FMath::Abs(Ay[i]) * H.Y + FMath::Abs(Az[i]) * H.Z);
	}
	return Bounds;
}

bool AGetawayPlacementPawn::IsBlockedAt(const FVector& Centre, const FRotator& Rotation,
	const FVector& LocalSize) const
{
	const UWorld* World = GetWorld();
	if (!World) return true;

	// Shrunk slightly so a piece resting flush on a surface is not called an overlap. The box is
	// the piece's own, turned the way the piece is turned: an axis-aligned test would be a box
	// lying the other way round for a quarter turn, and far too fat for a piece on a slope.
	const FVector Extent = (LocalSize * 0.5f - FVector(FitTolerance)).ComponentMax(FVector(1.f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GetawayPlacement), false, this);
	return World->OverlapBlockingTestByChannel(
		Centre, Rotation.Quaternion(), ECC_WorldStatic, FCollisionShape::MakeBox(Extent), Params);
}

bool AGetawayPlacementPawn::IsTouchingSomething(const FVector& Centre, const FRotator& Rotation,
	const FVector& LocalSize) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	// The mirror image of IsBlockedAt: the same oriented box, grown by ContactReach instead of
	// shrunk by FitTolerance. A piece sits flush against the face it was aimed at, so whatever it
	// is resting on falls inside that skin, while a piece hanging in clear air finds nothing.
	//
	// Nothing solid has to be *under* it in particular. Butting onto the side of a wall or hanging
	// off a ceiling counts, which is what you want in a game about building silly routes.
	const FVector Extent = LocalSize * 0.5f + FVector(FMath::Max(ContactReach, 0.1f));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GetawayPlacement), false, this);
	return World->OverlapBlockingTestByChannel(
		Centre, Rotation.Quaternion(), ECC_WorldStatic, FCollisionShape::MakeBox(Extent), Params);
}

namespace
{
	/** How far a probe may be off its surface before it counts as hanging in the air. */
	constexpr float SupportReach = 18.f;

	/**
	 * Probes on the outer edge of a plate are pulled this far in, so a plate ending exactly level
	 * with the edge of a floor is still called supported instead of putting a trace right on the
	 * join and taking whatever floating point says. It is also the most a piece may overhang.
	 */
	constexpr float SupportEdgeBias = 2.f;

	/** Scales a probe offset measured from the middle of a plate so the outermost comes in by the bias. */
	float EdgeShrink(float FullSize)
	{
		const float Half = FullSize * 0.5f;
		return (Half > SupportEdgeBias) ? (Half - SupportEdgeBias) / Half : 0.f;
	}
}

void AGetawayPlacementPawn::SupportCells(const FVector& PieceSize, int32& OutNX, int32& OutNY,
	float& OutStepX, float& OutStepY) const
{
	// One cell per grid square, so a trimmed plate still lands on the same 50 cm lattice as
	// everything else and two players trimming the same ledge get the same answer.
	const float Grid = FMath::Max(1.f, GridSize);
	OutNX = FMath::Max(1, FMath::RoundToInt(PieceSize.X / Grid));
	OutNY = FMath::Max(1, FMath::RoundToInt(PieceSize.Y / Grid));
	OutStepX = PieceSize.X / OutNX;
	OutStepY = PieceSize.Y / OutNY;
}

bool AGetawayPlacementPawn::IsBackedAcross(const FVector& Centre, const FRotator& Rotation,
	const FVector& LocalSize, const FVector& PieceSize) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	const FQuat Q = Rotation.Quaternion();
	const FVector Out = Q.GetAxisZ();   // the way the piece faces, i.e. away from the surface
	const FVector AxisX = Q.GetAxisX();
	const FVector AxisY = Q.GetAxisY();
	const FVector Face = Centre - Out * (LocalSize.Z * 0.5f);

	// The same corner lattice TrimToSupport grew the plate out of, so this asks about the very
	// points the trim found backed. It used to probe four corners pulled in 12 cm from the edges,
	// which disagreed with the trim by up to half a cell: wherever a surface ended inside that
	// margin - anything not lined up with the grid, and every slope - the server refused a piece
	// its own client had just drawn in cyan, and said so only in the log. Same question, one answer.
	//
	// The trim's lattice is laid out on the catalog piece and this one on the trimmed plate, so
	// where a trimmed edge falls inside the catalog piece the two probes differ by the edge bias.
	// Only ever in the safe direction: this one sits further inside the part the trim kept.
	int32 CellsX, CellsY;
	float StepX, StepY;
	SupportCells(PieceSize, CellsX, CellsY, StepX, StepY);
	const int32 NX = FMath::Clamp(FMath::RoundToInt(LocalSize.X / StepX), 1, CellsX);
	const int32 NY = FMath::Clamp(FMath::RoundToInt(LocalSize.Y / StepY), 1, CellsY);
	// Taken off the size that arrived rather than off the step, so a client that asks for a plate
	// no trim would ever produce still gets every bit of it tested.
	const float CellX = LocalSize.X / NX;
	const float CellY = LocalSize.Y / NY;
	const float ShrinkX = EdgeShrink(LocalSize.X);
	const float ShrinkY = EdgeShrink(LocalSize.Y);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GetawaySupport), false, this);
	for (int32 I = 0; I <= NX; ++I)
	{
		for (int32 J = 0; J <= NY; ++J)
		{
			const FVector P = Face
				+ AxisX * ((-LocalSize.X * 0.5f + CellX * I) * ShrinkX)
				+ AxisY * ((-LocalSize.Y * 0.5f + CellY * J) * ShrinkY);
			// Start just clear of the surface and poke into it. Same channel the aim trace uses, so
			// anything you can point at can hold the piece up.
			if (!World->LineTraceTestByChannel(P + Out * 4.f, P - Out * SupportReach,
				ECC_Visibility, Params))
			{
				return false;
			}
		}
	}
	return true;
}

bool AGetawayPlacementPawn::TrimToSupport(const FVector& AimPoint, const FVector& UntrimmedCentre,
	const FRotator& Rotation, const FGetawayPieceDef& Def, FVector& OutCentre, FVector& OutLocalSize) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	const FQuat Q = Rotation.Quaternion();
	const FVector Out = Q.GetAxisZ();
	const FVector AxisX = Q.GetAxisX();
	const FVector AxisY = Q.GetAxisY();
	const FVector Face = UntrimmedCentre - Out * (Def.Size.Z * 0.5f);

	int32 NX, NY;
	float StepX, StepY;
	SupportCells(Def.Size, NX, NY, StepX, StepY);

	// A cell counts only if all four of its corners are backed, not just its centre.
	//
	// Centres were what this probed, and a cell whose centre is on the floor but whose outer half
	// hangs past the edge was kept, so the plate overhung the ledge by up to half a cell - 25 cm
	// with the default grid. That is wrong for a bed of spikes and wronger for a jump pad, whose
	// launch direction comes off a face that is not there. Corners are shared between neighbouring
	// cells, so this is one trace per lattice point rather than four per cell, and the whole
	// lattice is pulled in by SupportEdgeBias so a plate flush with the edge still counts.
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GetawaySupport), false, this);
	const float ShrinkX = EdgeShrink(Def.Size.X);
	const float ShrinkY = EdgeShrink(Def.Size.Y);
	TArray<bool> Corner;
	Corner.SetNumZeroed((NX + 1) * (NY + 1));
	for (int32 I = 0; I <= NX; ++I)
	{
		for (int32 J = 0; J <= NY; ++J)
		{
			const float OX = (-Def.Size.X * 0.5f + StepX * I) * ShrinkX;
			const float OY = (-Def.Size.Y * 0.5f + StepY * J) * ShrinkY;
			const FVector P = Face + AxisX * OX + AxisY * OY;
			Corner[I * (NY + 1) + J] = World->LineTraceTestByChannel(
				P + Out * 4.f, P - Out * SupportReach, ECC_Visibility, Params);
		}
	}

	TArray<bool> Backed;
	Backed.SetNumZeroed(NX * NY);
	for (int32 I = 0; I < NX; ++I)
	{
		for (int32 J = 0; J < NY; ++J)
		{
			Backed[I * NY + J] = Corner[I * (NY + 1) + J] && Corner[(I + 1) * (NY + 1) + J]
				&& Corner[I * (NY + 1) + J + 1] && Corner[(I + 1) * (NY + 1) + J + 1];
		}
	}

	// Start from the cell you were actually pointing at and grow outwards.
	const FVector D = AimPoint - Face;
	int32 AI = FMath::Clamp(FMath::FloorToInt32((FVector::DotProduct(D, AxisX) + Def.Size.X * 0.5f) / StepX), 0, NX - 1);
	int32 AJ = FMath::Clamp(FMath::FloorToInt32((FVector::DotProduct(D, AxisY) + Def.Size.Y * 0.5f) / StepY), 0, NY - 1);
	if (!Backed[AI * NY + AJ])
	{
		// Aiming at the lip of a ledge, where the cell under the crosshair is itself half over the
		// edge. Slide to the nearest cell that is fully backed instead of refusing: asking for
		// spikes at the edge of a ledge should give you spikes that stop at the edge. Refusing
		// outright - which is what this did - also made the ghost blink out as the aim point
		// crossed a cell boundary, which is a pixel or two of mouse movement.
		//
		// Nearest by cell, scanned in a fixed order so ties always fall the same way: the server
		// re-checks the answer and the two have to agree.
		int32 BestI = -1;
		int32 BestJ = -1;
		int32 BestDist = MAX_int32;
		for (int32 I = 0; I < NX; ++I)
		{
			for (int32 J = 0; J < NY; ++J)
			{
				if (!Backed[I * NY + J]) continue;
				const int32 Dist = (I - AI) * (I - AI) + (J - AJ) * (J - AJ);
				if (Dist < BestDist)
				{
					BestDist = Dist;
					BestI = I;
					BestJ = J;
				}
			}
		}
		if (BestI < 0) return false; // nothing under any part of the plate
		AI = BestI;
		AJ = BestJ;
	}

	auto ColumnBacked = [&](int32 I, int32 J0, int32 J1)
	{
		for (int32 J = J0; J <= J1; ++J) { if (!Backed[I * NY + J]) return false; }
		return true;
	};
	auto RowBacked = [&](int32 J, int32 I0, int32 I1)
	{
		for (int32 I = I0; I <= I1; ++I) { if (!Backed[I * NY + J]) return false; }
		return true;
	};

	int32 I0 = AI, I1 = AI, J0 = AJ, J1 = AJ;
	for (bool bGrew = true; bGrew; )
	{
		bGrew = false;
		// A fixed order rather than the largest possible rectangle: it is deterministic, which
		// matters more here, because the server re-checks the answer the client arrived at.
		if (I0 > 0 && ColumnBacked(I0 - 1, J0, J1)) { --I0; bGrew = true; }
		if (I1 < NX - 1 && ColumnBacked(I1 + 1, J0, J1)) { ++I1; bGrew = true; }
		if (J0 > 0 && RowBacked(J0 - 1, I0, I1)) { --J0; bGrew = true; }
		if (J1 < NY - 1 && RowBacked(J1 + 1, I0, I1)) { ++J1; bGrew = true; }
	}

	const float X0 = -Def.Size.X * 0.5f + StepX * I0;
	const float X1 = -Def.Size.X * 0.5f + StepX * (I1 + 1);
	const float Y0 = -Def.Size.Y * 0.5f + StepY * J0;
	const float Y1 = -Def.Size.Y * 0.5f + StepY * (J1 + 1);

	OutLocalSize = FVector(X1 - X0, Y1 - Y0, Def.Size.Z);
	const FVector TrimmedFace = Face + AxisX * ((X0 + X1) * 0.5f) + AxisY * ((Y0 + Y1) * 0.5f);
	OutCentre = TrimmedFace + Out * (Def.Size.Z * 0.5f);
	return true;
}

AActor* AGetawayPlacementPawn::FindHostAt(const FVector& Centre, const FRotator& Rotation,
	const FGetawayPieceDef& Def) const
{
	const UWorld* World = GetWorld();
	if (!World || !Def.bSurfaceMounted) return nullptr;

	// Straight back out of the middle of the face into whatever is holding it up. Derived rather
	// than sent by the client: the server has the centre and the rotation, so it can find the host
	// itself and does not have to trust an actor reference off the wire.
	const FVector Out = Rotation.Quaternion().GetAxisZ();
	const FVector Face = Centre - Out * (Def.Size.Z * 0.5f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GetawaySupport), false, this);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Face + Out * 4.f, Face - Out * SupportReach,
		ECC_Visibility, Params))
	{
		return Hit.GetActor();
	}
	return nullptr;
}

FGetawayPlacement AGetawayPlacementPawn::ComputePlacement(const FGetawayPieceDef& Def) const
{
	FGetawayPlacement Out;
	UWorld* World = GetWorld();
	if (!World || !Camera) return Out;

	const int32 Steps = GetRotationSteps();
	Out.bDirectional = Def.bDirectional;
	Out.bSurfaceMounted = Def.bSurfaceMounted;

	const FVector Start = Camera->GetComponentLocation();
	const FVector Dir = Camera->GetForwardVector();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GetawayPlacement), false, this);
	FHitResult Hit;
	FVector P;
	FVector N;
	if (World->LineTraceSingleByChannel(Hit, Start, Start + Dir * AimRange, ECC_Visibility, Params))
	{
		P = Hit.ImpactPoint;
		N = Hit.ImpactNormal;
	}
	else if (!Def.bSurfaceMounted && Dir.Z < -0.01f && Start.Z > 0.f)
	{
		// Nothing under the crosshair: fall back to the ground plane, as the lab does. A
		// surface-mounted piece gets no such fallback - it has to be stuck to something real.
		const float T = -Start.Z / Dir.Z;
		if (T > AimRange) return Out;
		P = Start + Dir * T;
		N = FVector::UpVector;
	}
	else
	{
		return Out;
	}

	const FVector A(FMath::Abs(N.X), FMath::Abs(N.Y), FMath::Abs(N.Z));
	const int32 Axis = (A.X > A.Y && A.X > A.Z) ? 0 : (A.Y >= A.Z ? 1 : 2);
	const float Grid = FMath::Max(1.f, GridSize);

	if (Def.bSurfaceMounted)
	{
		// Lie flat on the face you are aiming at: the piece's own +Z turns to point out of the
		// surface, so spikes come off a wall sideways and off a floor upwards without the catalog
		// needing a separate entry for each. R spins it about the normal.
		const FQuat Base = FRotationMatrix::MakeFromZ(N).ToQuat();
		const FQuat Spun = FQuat(N, FMath::DegreesToRadians(Steps * 90.f)) * Base;
		Out.Rotation = Spun.Rotator();

		// Standing off the face by half its thickness puts the back of the plate flush against it.
		// The grid still applies along the surface, so plates line up with everything else.
		FVector Centre = P + N * (Def.Size.Z * 0.5f);
		for (int32 i = 0; i < 3; ++i)
		{
			if (i != Axis)
			{
				Centre[i] = FMath::RoundToFloat(Centre[i] / Grid) * Grid;
			}
		}

		// Then cut it back to the part that is actually held up, rather than refusing an overhang.
		// Placing spikes over the end of a ledge gives you spikes that stop at the edge.
		FVector TrimmedCentre;
		FVector TrimmedLocal;
		if (!TrimToSupport(P, Centre, Out.Rotation, Def, TrimmedCentre, TrimmedLocal))
		{
			return Out;
		}
		Out.Centre = TrimmedCentre;
		Out.LocalSize = TrimmedLocal;
		Out.Size = WorldSizeFor(TrimmedLocal, Out.Rotation);
		Out.bTrimmed = !TrimmedLocal.Equals(Def.Size, 1.f);
	}
	else if (A[Axis] < FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(SlantTolerance, 0.5f, 45.f))))
	{
		// A sloped face - a ramp, or something already lying on one. The piece lies down on it
		// rather than sitting square and burying one corner: yaw it the way you are looking as
		// usual, then tip that whole frame so its up axis follows the face. A floor becomes a
		// slanted platform, a wall leans, a ramp carries on up the slope.
		const FQuat Yaw(FRotator(0.f, Steps * 90.f, 0.f));
		const FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, N);
		Out.Rotation = (Tilt * Yaw).Rotator();
		Out.LocalSize = Def.Size;
		Out.Size = WorldSizeFor(Def.Size, Out.Rotation);

		// Bottom face flush on the slope, then snapped along it so slanted pieces still line up
		// with each other. Snapping moves the piece sideways, which on a slope also means up or
		// down, so the dominant axis is solved for afterwards rather than snapped: it puts the
		// centre back on the face's own plane, exactly half a thickness clear of it. Snapping all
		// three would bury the piece or float it by the slope's rise across half a grid square.
		const float Lift = Def.Size.Z * 0.5f;
		FVector Centre = P + N * Lift;
		float Rest = FVector::DotProduct(N, P) + Lift;
		for (int32 i = 0; i < 3; ++i)
		{
			if (i != Axis)
			{
				Centre[i] = FMath::RoundToFloat(Centre[i] / Grid) * Grid;
				Rest -= N[i] * Centre[i];
			}
		}
		Centre[Axis] = Rest / N[Axis]; // never near zero: Axis is the normal's dominant component
		Out.Centre = Centre;
	}
	else
	{
		Out.Rotation = FRotator(0.f, Steps * 90.f, 0.f);
		Out.LocalSize = Def.Size;
		Out.Size = WorldSizeFor(Def.Size, Out.Rotation);

		// placeBox(): pin the piece to the face you are aiming at on the normal's dominant axis,
		// and snap it to the grid on the other two.
		FVector Min;
		for (int32 i = 0; i < 3; ++i)
		{
			if (i == Axis)
			{
				Min[i] = (N[i] > 0.f) ? P[i] : P[i] - Out.Size[i];
			}
			else
			{
				Min[i] = FMath::RoundToFloat((P[i] - Out.Size[i] * 0.5f) / Grid) * Grid;
			}
		}
		Out.Centre = Min + Out.Size * 0.5f;
	}

	Out.bHasTarget = true;

	// The same tests the server will run when it is asked to place this (see ServerPlacePiece), so
	// the ghost turning red and the server refusing agree unless the level changed underneath.
	Out.bBlocked = IsBlockedAt(Out.Centre, Out.Rotation, Out.LocalSize);

	// Everything has to touch something. A surface-mounted piece is exempt because the trim it has
	// already been through is strictly stronger - it wants the whole footprint backed, not one
	// corner of contact - and running both would be the same question asked twice. That trim is
	// also what the server re-checks with IsBackedAcross, cell for cell, so a cyan ghost here is a
	// piece the server will accept.
	Out.bFloating = !Def.bSurfaceMounted && !IsTouchingSomething(Out.Centre, Out.Rotation, Out.LocalSize);

	return Out;
}

/* ============================================================ tick and ghost */

void AGetawayPlacementPawn::FellOutOfWorld(const UDamageType& DmgType)
{
	// Nothing happens. The fly camera has no collision and is allowed under the level - flying
	// below what you are building to look up at it is a normal thing to do during placement, and
	// the engine default would Destroy() the camera out from under the player at the KillZ that
	// now kills a running character (see AGetawayCharacter::FellOutOfWorld).
}

void AGetawayPlacementPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Flying, aiming and the ghost are all local to whoever is holding this camera. The server keeps
	// a copy of every client's placement pawn, and ticking that would fly it on its own and draw a
	// second ghost into the host's viewport from a camera nobody is looking through.
	if (!IsLocallyControlled()) return;

	// Pieces leave the hand as you spend them, and the hand arrives a frame or two after the pawn
	// on a joining client, so the held slot is kept inside it here rather than at every reader.
	HandSlot = FMath::Clamp(HandSlot, 0, FMath::Max(0, GetHandCount() - 1));

	// Fly. Forward follows the full camera direction, including pitch, so you can dive and climb.
	const FRotator Rot = Controller ? Controller->GetControlRotation() : GetActorRotation();
	const FVector Fwd = FRotationMatrix(Rot).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(FRotator(0.f, Rot.Yaw, 0.f)).GetUnitAxis(EAxis::Y);

	// Keyboard and pad are added together so that holding both does not fight. The direction is
	// normalised and then scaled by how hard you are pushing, capped at 1: a key is always worth at
	// least 1 and so still flies at full speed, while a stick gives a slow crawl near the middle,
	// which is what you want easing up to a ledge.
	const FVector2D Plane = MoveInput + StickMoveInput;
	const float Up = UpInput + StickUpInput;
	FVector Wish = Fwd * Plane.Y + Right * Plane.X + FVector::UpVector * Up;
	if (!Wish.IsNearlyZero())
	{
		const float Push = FMath::Min(FMath::Max(Plane.Size(), FMath::Abs(Up)), 1.f);
		Wish = Wish.GetSafeNormal() * FlySpeed * Push * (bFast ? FastMultiplier : 1.f);
	}
	FlyVelocity = FMath::VInterpTo(FlyVelocity, Wish, DeltaSeconds, FlySmoothing);
	if (!FlyVelocity.IsNearlyZero())
	{
		AddActorWorldOffset(FlyVelocity * DeltaSeconds, false);
	}

	FGetawayPieceDef Def;
	Planned = (CanBuildNow() && CurrentPiece(Def)) ? ComputePlacement(Def) : FGetawayPlacement();
	// Which piece it was worked out for, so a click can tell whether it is still the one in hand.
	PlannedSlot = Planned.bHasTarget ? HandSlot : -1;

	// No ghost once building is shut: a cyan box tracking your crosshair reads as an invitation,
	// and the HUD line alone is easy to miss while you are flying around.
	if (bDrawGhost && CanBuildNow())
	{
		DrawGhost();
	}
}

void AGetawayPlacementPawn::DrawGhost() const
{
	if (!Planned.bHasTarget) return;
	UWorld* World = GetWorld();
	if (!World) return;

	// Debug lines rather than a translucent mesh, so the plugin still needs no content. They are
	// compiled out of Shipping builds, so this wants a real ghost material before release.
	const FColor Colour = Planned.CanPlace() ? FColor(95, 196, 214) : FColor(230, 70, 60);
	// The piece's own box, turned the way the piece is turned - the same oriented box the blocked
	// test uses, so what you see is what gets tested. This used to draw Planned.Size with an
	// identity rotation, because Size is the *world* extent and rotating that swapped X and Y
	// straight back; drawing LocalSize instead makes the rotation the right thing to pass, and it
	// is the only way to show a piece lying over on a slope.
	DrawDebugBox(World, Planned.Centre, Planned.LocalSize * 0.5f, Planned.Rotation.Quaternion(),
		Colour, false, -1.f, 0, 3.f);

	// Only the pieces that care which way round they are get an arrow, as in the lab (DIRTYPES):
	// it points the way a ramp rises and the way a bridge drops.
	if (!Planned.bDirectional) return;

	const FVector Fwd = Planned.Rotation.RotateVector(FVector::ForwardVector);
	// Clear of the piece's own top face rather than straight up, so the arrow still sits above a
	// piece that is lying over on a slope instead of cutting through it.
	const FVector Up = Planned.Rotation.RotateVector(FVector::UpVector);
	const FVector Base = Planned.Centre + Up * (Planned.LocalSize.Z * 0.5f + 20.f);
	DrawDebugDirectionalArrow(World, Base, Base + Fwd * 120.f, 40.f, Colour, false, -1.f, 0, 3.f);

	// A ramp only fills half of its ghost box, so trace the edge it will slope along: low end at the
	// back, high end under the arrow. On a bridge the same two lines read as the way it will drop.
	const FVector Half = Planned.LocalSize * 0.5f;
	const FVector Low = Planned.Centre + Planned.Rotation.RotateVector(FVector(-Half.X, 0.f, -Half.Z));
	const FVector High = Planned.Centre + Planned.Rotation.RotateVector(FVector(Half.X, 0.f, Half.Z));
	const FVector Side = Planned.Rotation.RotateVector(FVector(0.f, Half.Y, 0.f));
	DrawDebugLine(World, Low - Side, High - Side, Colour, false, -1.f, 0, 2.f);
	DrawDebugLine(World, Low + Side, High + Side, Colour, false, -1.f, 0, 2.f);
}
