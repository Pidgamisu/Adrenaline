#include "GetawayCharacter.h"
#include "GetawayBreakable.h"
#include "GetawayC4.h"
#include "GetawayFinish.h"
#include "GetawayGameState.h"
#include "GetawayGameMode.h"
#include "GetawayHUD.h"
#include "GetawayMovementComponent.h"
#include "GetawayPlayerState.h"
#include "GetawayStickInput.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

#if !UE_BUILD_SHIPPING
// Both of these exist only for ShowRemoteState's debug readout, so they go when it does rather
// than sitting in a Shipping build as functions with internal linkage that nothing calls.
namespace
{
	/** The six states that cross the wire, named for logs and the on-screen readout. */
	const TCHAR* NetMoveName(EGetawayNetMove State)
	{
		switch (State)
		{
		case EGetawayNetMove::Crouch:  return TEXT("CROUCH");
		case EGetawayNetMove::Slide:   return TEXT("SLIDE");
		case EGetawayNetMove::Air:     return TEXT("AIR");
		case EGetawayNetMove::WallRun: return TEXT("WALLRUN");
		case EGetawayNetMove::Mantle:  return TEXT("MANTLE");
		default:                       return TEXT("GROUND");
		}
	}

	FColor NetMoveColour(EGetawayNetMove State)
	{
		switch (State)
		{
		case EGetawayNetMove::Slide:   return FColor(240, 179, 61);
		case EGetawayNetMove::WallRun: return FColor(95, 196, 214);
		case EGetawayNetMove::Mantle:  return FColor(79, 209, 148);
		case EGetawayNetMove::Air:     return FColor(235, 235, 235);
		default:                       return FColor(163, 168, 181);
		}
	}
}
#endif

AGetawayCharacter::AGetawayCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UGetawayMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	// 80 cm wide, 180 cm tall (the lab's player). Crouched height is set on the movement component.
	GetCapsuleComponent()->InitCapsuleSize(40.f, 90.f);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	BaseEyeHeight = StandingEyeHeight - 90.f;
	CrouchedEyeHeight = CrouchedEyeHeightAboveFeet - 50.f;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(GetCapsuleComponent());
	Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	Camera->bUsePawnControlRotation = false; // rotation is set every tick so we can add roll and punch
	Camera->SetFieldOfView(BaseFOV);

	C4Class = AGetawayC4::StaticClass();

	/* ---------------- client-authoritative networking ---------------- */

	bReplicates = true;

	// The engine's own movement replication is off: with client authority the owning client's
	// FGetawayNetMove is the only source of truth, and leaving ReplicatedMovement on would have the
	// server's (always slightly wrong) simulation fighting it on every other machine.
	SetReplicateMovement(false);

	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		// This is the whole reason the movement component did not have to change. The server still
		// receives the engine's ServerMoves and still replays them, but it is forbidden from
		// correcting the client, and it takes the client's position as truth. Without the first
		// flag every wallrun, slide and mantle - none of which is in a saved move - would read as
		// client error and the server would yank the player back several times a second.
		CMC->bIgnoreClientMovementErrorChecksAndCorrection = true;
		CMC->bServerAcceptClientAuthoritativePosition = true;
	}

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.8f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetGenerateOverlapEvents(false);
	Body->SetOwnerNoSee(true); // you are in first person; this is for everyone else
	Body->SetMobility(EComponentMobility::Movable);

	Nose = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Nose"));
	Nose->SetupAttachment(GetCapsuleComponent());
	Nose->SetRelativeLocation(FVector(45.f, 0.f, 60.f));
	Nose->SetRelativeScale3D(FVector(0.5f, 0.15f, 0.15f));
	Nose->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Nose->SetGenerateOverlapEvents(false);
	Nose->SetOwnerNoSee(true);
	Nose->SetMobility(EComponentMobility::Movable);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Nose->SetStaticMesh(Cube.Object);
	}
}

void AGetawayCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// The owner already knows where it is - it is the one telling everyone else.
	DOREPLIFETIME_CONDITION(AGetawayCharacter, NetMove, COND_SimulatedOnly);
}

UGetawayMovementComponent* AGetawayCharacter::GetGetawayMovement() const
{
	return Cast<UGetawayMovementComponent>(GetCharacterMovement());
}

void AGetawayCharacter::BeginPlay()
{
	Super::BeginPlay();
	EyeAboveFeet = StandingEyeHeight;
	CurrentFOV = BaseFOV;
	ChargesHeld = FMath::Clamp(StartingCharges, 0, MaxCharges);

	// After the charges rather than before them, because a Loaded round overrides both the charge
	// you start with and how many you may hold.
	ApplyRoundEffect();

#if !UE_BUILD_SHIPPING
	// A headless client has no hands, so it cannot run a course. -GetawayFinishAfter=<seconds>
	// teleports it onto the objective instead, which is how the round loop was checked over a real
	// connection: give it to one client and not the other and you get a solo finish, give it to
	// both and you get the "everybody got there, so nobody scores" case. It goes through the same
	// path a real run does - the client moves itself, the server takes that position as truth and
	// its own overlap is what counts. Delete it with the rest of the net-test scaffolding.
	float FinishAfter = 0.f;
	if (FParse::Value(FCommandLine::Get(), TEXT("GetawayFinishAfter="), FinishAfter)
		&& FinishAfter > 0.f && IsLocallyControlled())
	{
		GetWorldTimerManager().SetTimer(NetTestFinishTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			UWorld* World = GetWorld();
			if (!World) return;
			for (TActorIterator<AGetawayFinish> It(World); It; ++It)
			{
				const FVector Where = It->GetActorLocation() + FVector(0.f, 0.f, 50.f);
				SetActorLocation(Where, false, nullptr, ETeleportType::TeleportPhysics);
				UE_LOG(LogTemp, Log, TEXT("Getaway net test: %s jumping to the objective at %s"),
					*GetName(), *Where.ToCompactString());
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("Getaway net test: no objective to jump to"));
		}), FinishAfter, false);
	}
#endif
}

/* ============================================================ input */

void AGetawayCharacter::BuildDefaultInput()
{
	if (MappingContext) return;

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};
	MoveAction     = MakeAction(TEXT("IA_GetawayMove"), EInputActionValueType::Axis2D);
	LookAction     = MakeAction(TEXT("IA_GetawayLook"), EInputActionValueType::Axis2D);
	// The pad's sticks get their own actions rather than extra keys on the two above. Look, because
	// a stick is a rate and a mouse is a delta, so they cannot share a formula; move, because the
	// stick wants a dead zone and a full-tilt threshold that would also reshape WASD if it were the
	// same action - a diagonal on the keyboard is already longer than 1 and would get squashed.
	MoveStickAction = MakeAction(TEXT("IA_GetawayMoveStick"), EInputActionValueType::Axis2D);
	LookStickAction = MakeAction(TEXT("IA_GetawayLookStick"), EInputActionValueType::Axis2D);
	GamepadProbeAction = MakeAction(TEXT("IA_GetawayPadProbe"), EInputActionValueType::Boolean);
	JumpAction     = MakeAction(TEXT("IA_GetawayJump"), EInputActionValueType::Boolean);
	JumpTapAction  = MakeAction(TEXT("IA_GetawayJumpWheel"), EInputActionValueType::Boolean);
	CrouchAction   = MakeAction(TEXT("IA_GetawayCrouch"), EInputActionValueType::Boolean);
	ThrowAction    = MakeAction(TEXT("IA_GetawayThrowC4"), EInputActionValueType::Boolean);
	DetonateAction = MakeAction(TEXT("IA_GetawayDetonate"), EInputActionValueType::Boolean);
	RestartAction  = MakeAction(TEXT("IA_GetawayRestart"), EInputActionValueType::Boolean);
	BackToPlacementAction = MakeAction(TEXT("IA_GetawayBackToPlacement"), EInputActionValueType::Boolean);

	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Getaway"));

	// WASD into one 2D axis: X = right, Y = forward.
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

	MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	MappingContext->MapKey(JumpAction, EKeys::SpaceBar);
	MappingContext->MapKey(JumpTapAction, EKeys::MouseScrollUp);
	MappingContext->MapKey(JumpTapAction, EKeys::MouseScrollDown);
	MappingContext->MapKey(CrouchAction, EKeys::C);
	MappingContext->MapKey(CrouchAction, EKeys::LeftShift);
	MappingContext->MapKey(CrouchAction, EKeys::LeftControl);
	MappingContext->MapKey(ThrowAction, EKeys::G);
	MappingContext->MapKey(DetonateAction, EKeys::F);
	MappingContext->MapKey(RestartAction, EKeys::R);
	MappingContext->MapKey(BackToPlacementAction, EKeys::BackSpace);

	// The pad. Left stick moves, right stick looks; A jumps, so holding it bunny hops exactly like
	// holding space. Crouch is on LB as well as B: it has to be held to slide, and a shoulder is
	// easier to hold than a face button you also need your thumb for. There is no pad key on
	// JumpTapAction - the mouse wheel is there for hop timing, which auto-hop already does for you.
	MappingContext->MapKey(MoveStickAction, EKeys::Gamepad_Left2D);
	MappingContext->MapKey(LookStickAction, EKeys::Gamepad_Right2D);
	MappingContext->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	MappingContext->MapKey(CrouchAction, EKeys::Gamepad_LeftShoulder);
	MappingContext->MapKey(CrouchAction, EKeys::Gamepad_FaceButton_Right);
	MappingContext->MapKey(ThrowAction, EKeys::Gamepad_RightShoulder);
	MappingContext->MapKey(DetonateAction, EKeys::Gamepad_RightTrigger);
	MappingContext->MapKey(RestartAction, EKeys::Gamepad_DPad_Down);
	MappingContext->MapKey(BackToPlacementAction, EKeys::Gamepad_Special_Left);

	for (const FKey& Key : GetawayStick::ProbeKeys())
	{
		MappingContext->MapKey(GamepadProbeAction, Key);
	}
}

void AGetawayCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	BuildDefaultInput();

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				// Drop the placement pawn's context first - see the matching note in
				// AGetawayPlacementPawn. Contexts outlive the pawn that added them, and a leftover
				// one consumes WASD before this character ever sees it.
				Subsystem->ClearAllMappings();
				Subsystem->AddMappingContext(MappingContext, 0);
			}
		}
	}

	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!Input)
	{
		UE_LOG(LogTemp, Error, TEXT("GetawayCharacter needs Enhanced Input. Set Project Settings > Input > Default Input Component Class to EnhancedInputComponent."));
		return;
	}

	Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGetawayCharacter::OnMove);
	Input->BindAction(MoveStickAction, ETriggerEvent::Triggered, this, &AGetawayCharacter::OnMoveStick);
	Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGetawayCharacter::OnLook);
	Input->BindAction(LookStickAction, ETriggerEvent::Triggered, this, &AGetawayCharacter::OnLookStick);
	Input->BindAction(LookStickAction, ETriggerEvent::Completed, this, &AGetawayCharacter::OnLookStickReleased);
	Input->BindAction(GamepadProbeAction, ETriggerEvent::Started, this, &AGetawayCharacter::OnGamepadUsed);
	Input->BindAction(JumpAction, ETriggerEvent::Started, this, &AGetawayCharacter::OnJumpPressed);
	Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &AGetawayCharacter::OnJumpReleased);
	Input->BindAction(JumpTapAction, ETriggerEvent::Started, this, &AGetawayCharacter::OnJumpTap);
	Input->BindAction(CrouchAction, ETriggerEvent::Started, this, &AGetawayCharacter::OnCrouchPressed);
	Input->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AGetawayCharacter::OnCrouchReleased);
	Input->BindAction(ThrowAction, ETriggerEvent::Started, this, &AGetawayCharacter::OnThrow);
	Input->BindAction(DetonateAction, ETriggerEvent::Started, this, &AGetawayCharacter::OnDetonate);
	Input->BindAction(RestartAction, ETriggerEvent::Started, this, &AGetawayCharacter::OnRestart);
	Input->BindAction(BackToPlacementAction, ETriggerEvent::Started, this, &AGetawayCharacter::OnBackToPlacement);
}

void AGetawayCharacter::ApplyMoveInput(const FVector2D& Axis)
{
	const FRotator YawOnly(0.f, GetControlRotation().Yaw, 0.f);
	const FRotationMatrix M(YawOnly);
	AddMovementInput(M.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(M.GetUnitAxis(EAxis::Y), Axis.X);
}

void AGetawayCharacter::OnMove(const FInputActionValue& Value)
{
	ApplyMoveInput(Value.Get<FVector2D>());
}

void AGetawayCharacter::OnMoveStick(const FInputActionValue& Value)
{
	bUsingGamepad = true;

	const FVector2D Stick = GetawayStick::SnapToFullTilt(
		GetawayStick::ApplyDeadZone(Value.Get<FVector2D>(), MoveStickDeadZone), MoveStickFullTilt);
	if (Stick.IsNearlyZero()) return;

	ApplyMoveInput(Stick);
}

void AGetawayCharacter::OnLook(const FInputActionValue& Value)
{
	if (!Controller) return;
	// Any mouse movement at all means a mouse is what you are holding. The pad side of this is the
	// probe action, because the shared bindings cannot tell which key fired them.
	bUsingGamepad = false;
	const FVector2D Delta = Value.Get<FVector2D>();
	// Unreal pre-scales mouse input by the Axis Config sensitivity (0.07 by default). Divide that back out to get
	// mouse counts, then apply the Source formula. Set directly so legacy yaw/pitch input scales can't change the feel.
	const float K = MouseSensitivity * 0.022f / FMath::Max(MouseInputScale, 0.0001f);
	FRotator R = Controller->GetControlRotation();
	R.Yaw += Delta.X * K;
	R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch + (bInvertMouseY ? -1.f : 1.f) * Delta.Y * K), -89.f, 89.f);
	Controller->SetControlRotation(R);
}

void AGetawayCharacter::OnLookStick(const FInputActionValue& Value)
{
	if (!Controller) return;
	bUsingGamepad = true;

	const UWorld* World = GetWorld();
	const float Dt = World ? World->GetDeltaSeconds() : 0.f;
	if (Dt <= 0.f) return;

	const FVector2D Stick = GetawayStick::ApplyCurve(
		GetawayStick::ApplyDeadZone(Value.Get<FVector2D>(), StickLookDeadZone), StickLookExponent);
	if (Stick.IsNearlyZero())
	{
		// Inside the dead zone counts as letting go, or the spin-up would already be at full rate
		// by the time you actually pushed the stick far enough to turn.
		StickLookHeld = 0.f;
		return;
	}

	StickLookHeld += Dt;
	const float Ramp = (StickLookRampTime > 0.f)
		? FMath::Lerp(StickLookRampStartScale, 1.f, FMath::Min(StickLookHeld / StickLookRampTime, 1.f))
		: 1.f;

	// Degrees per second, so it has to be multiplied by the frame time. This is the whole reason
	// the stick does not go through OnLook: a mouse delta must not be.
	FRotator R = Controller->GetControlRotation();
	R.Yaw += Stick.X * StickLookYawSpeed * Ramp * Dt;

	// Note the sign: the stick's Y has to be flipped where the mouse's does not. Play-tested
	// 2026-10-31 - built without the flip, pushing the stick up looked down. Pitch is the sign trap
	// in this project (see the wallrun roll note in CLAUDE.md); copy the mouse formula's sign here
	// and it comes out inverted, so bInvertStickY *adds* the flip rather than being the flip.
	const float PitchStep = (bInvertStickY ? 1.f : -1.f) * Stick.Y * StickLookPitchSpeed * Ramp * Dt;
	R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch + PitchStep), -89.f, 89.f);
	Controller->SetControlRotation(R);
}

void AGetawayCharacter::OnLookStickReleased(const FInputActionValue&)
{
	StickLookHeld = 0.f;
}

void AGetawayCharacter::OnGamepadUsed(const FInputActionValue&)
{
	bUsingGamepad = true;
}

void AGetawayCharacter::OnJumpPressed(const FInputActionValue&)
{
	if (UGetawayMovementComponent* M = GetGetawayMovement())
	{
		M->QueueJump();
		M->SetJumpHeld(true);
	}
}

void AGetawayCharacter::OnJumpReleased(const FInputActionValue&)
{
	if (UGetawayMovementComponent* M = GetGetawayMovement()) M->SetJumpHeld(false);
}

void AGetawayCharacter::OnJumpTap(const FInputActionValue&)
{
	if (UGetawayMovementComponent* M = GetGetawayMovement()) M->QueueJump();
}

void AGetawayCharacter::OnCrouchPressed(const FInputActionValue&)
{
	Crouch();
	if (UGetawayMovementComponent* M = GetGetawayMovement()) M->SetCrouchHeld(true);
}

void AGetawayCharacter::OnCrouchReleased(const FInputActionValue&)
{
	UnCrouch();
	if (UGetawayMovementComponent* M = GetGetawayMovement()) M->SetCrouchHeld(false);
}

void AGetawayCharacter::OnThrow(const FInputActionValue&) { ThrowC4(); }
void AGetawayCharacter::OnDetonate(const FInputActionValue&) { DetonateC4(); }

void AGetawayCharacter::OnRestart(const FInputActionValue&)
{
	// Reopening the level is a single-player convenience: on a client it would drop the connection,
	// and on a listen server it would throw everyone else out with you.
	if (GetNetMode() != NM_Standalone)
	{
		Notify(TEXT("R restart is single-player only"), 1.5f, FLinearColor(1.f, 0.42f, 0.3f), 7004);
		return;
	}
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}

void AGetawayCharacter::OnBackToPlacement(const FInputActionValue&)
{
	// Drop back to the fly camera. The pieces already placed stay where they are.
	// Only the server owns the phase, so ask it; on a listen server this just runs straight through.
	ServerBackToPlacement();
}

void AGetawayCharacter::ServerBackToPlacement_Implementation()
{
	UWorld* World = GetWorld();
	if (AGetawayGameMode* GM = World ? World->GetAuthGameMode<AGetawayGameMode>() : nullptr)
	{
		// Only this player drops out, and it forfeits their run - they score nothing this round.
		// It used to call StartPlacement(), which re-dealt and restarted the round for everybody;
		// with a second player on the course that is not a key one person gets to press.
		GM->GiveUpRunFor(Cast<APlayerController>(GetController()));
	}
}

/* ============================================================ dying */

bool AGetawayCharacter::WillRespawnOnDeath() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GS = World ? World->GetGameState() : nullptr;
	const UClass* Class = GS ? GS->GameModeClass.Get() : nullptr;

	// No game state yet, or somebody else's game mode: assume the default rather than guess.
	if (!Class || !Class->IsChildOf(AGetawayGameMode::StaticClass())) return true;

	return Class->GetDefaultObject<AGetawayGameMode>()->bRespawnOnDeath;
}

void AGetawayCharacter::Die(AActor* KilledBy)
{
	if (bDead) return;
	bDead = true;

	// Stop where you are. This character is finished either way - it is destroyed a beat later,
	// whether that beat ends in a respawn at the start or in the fly camera - and sliding on
	// after the spikes went through you reads as the trap having missed.
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->DisableMovement();
	}
	AddViewPunch(6.f);
	AddCameraShake(1.f);

	UWorld* World = GetWorld();
	if (!World) return;

	// A client has no game mode to ask, so the knob is read off the replicated game mode *class*
	// default instead. EditDefaultsOnly means that default is the live value.
	Notify(WillRespawnOnDeath() ? TEXT("Killed. Back to the start") : TEXT("Killed. Out until the next round"),
		2.f, FLinearColor(1.f, 0.31f, 0.31f), 7004);

	if (HasAuthority())
	{
		if (AGetawayGameMode* GM = World->GetAuthGameMode<AGetawayGameMode>())
		{
			GM->PlayerDied(Cast<APlayerController>(GetController()), KilledBy);
		}
		return;
	}
	ServerReportDeath(KilledBy);
}

void AGetawayCharacter::CheckFallDeath()
{
	// Only while you are actually running: a character that is still there during the finish
	// banner should not be told it has been killed, and a dead one is already out.
	if (bDead) return;
	const AGetawayPlayerState* PS = GetPlayerState<AGetawayPlayerState>();
	if (PS && !PS->IsRunning()) return;

	const float Feet = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	if (Feet < FallDeathZ)
	{
		Die(nullptr);
	}
}

void AGetawayCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	// Falling off the course kills you. The engine default here is Destroy(), which would leave
	// the controller holding no pawn at all - no death, no score, no way back into the round, just
	// a frozen camera - so a fall was the one hole in the round loop that nothing closed.
	//
	// Routed through Die() it reads exactly like walking into spikes: you die where you fell and
	// the game mode decides what that costs you - the start line, or the round. KilledBy is null
	// because nothing killed you, which RegisterTrapKill already treats as nobody to credit.
	//
	// The plane is the level's own KillZ, in World Settings - TestCourse sets it to -2000. Below
	// z=-300 the course has a catch floor across its whole footprint, so this only fires if you
	// leave the sides.
	if (!IsLocallyControlled())
	{
		// Only the machine that owns a character decides it is dead, the same bargain as spikes and
		// breaking. A simulated proxy interpolating below the plane has not fallen off anything.
		return;
	}

	// Falling past the plane when you are already home is not a death, which is the same rule
	// CheckFallDeath above has always applied. This route did not, and the hole showed up in the
	// first two-machine game (2026-11-03): cross the line at speed, carry on over the edge, and
	// you were told "Killed. Back to the start" after you had won - then left hanging there, because
	// PlayerDied and RespawnRunner both refuse a player who is not running. The game mode lifts a
	// finisher into the fly camera a beat after they cross; until it does, nothing may kill them.
	const AGetawayPlayerState* PS = GetPlayerState<AGetawayPlayerState>();
	if (PS && !PS->IsRunning()) return;

	Die(nullptr);
}

void AGetawayCharacter::ServerReportDeath_Implementation(AActor* KilledBy)
{
	UWorld* World = GetWorld();
	AGetawayGameMode* GM = World ? World->GetAuthGameMode<AGetawayGameMode>() : nullptr;
	if (!GM) return;

	// The server is not asked to agree that you touched it - under client authority it does not
	// have your real position to check against anyway. It decides what the death *means*.
	GM->PlayerDied(Cast<APlayerController>(GetController()), KilledBy);
}

void AGetawayCharacter::ServerReportBreak_Implementation(AGetawayBreakable* Breakable, FVector_NetQuantize100 Velocity)
{
	// Already broken is the normal case, not an error: the server often smashes the same panel
	// itself a frame later, and a chain reaction reports every link from both ends.
	if (!Breakable || Breakable->IsBroken()) return;

	// Straight back into Break on the authority, which multicasts it to everyone.
	Breakable->Break(Velocity);
}

/* ============================================================ C4 */

int32 AGetawayCharacter::GetChargesLeft() const
{
	return FMath::Clamp(ChargesHeld, 0, MaxCharges);
}

int32 AGetawayCharacter::AddCharges(int32 Count)
{
	const int32 Room = FMath::Max(0, MaxCharges - ChargesHeld);
	const int32 Taken = FMath::Clamp(Count, 0, Room);
	ChargesHeld += Taken;
	if (Taken > 0)
	{
		Notify(FString::Printf(TEXT("Picked up C4  (carrying %d)"), ChargesHeld), 1.2f,
			FLinearColor(0.94f, 0.70f, 0.24f), 7003);
	}
	return Taken;
}

void AGetawayCharacter::ThrowC4()
{
	Charges.RemoveAll([](const TObjectPtr<AGetawayC4>& C) { return !IsValid(C); });
	if (ChargesHeld <= 0)
	{
		Notify(TEXT("No C4. Find a pickup"), 1.2f, FLinearColor(1.f, 0.42f, 0.3f), 7002);
		return;
	}

	const FRotator View = GetControlRotation();
	const FVector Dir = View.Vector();
	const FVector Vel = GetVelocity();
	const FVector Start = Camera->GetComponentLocation() - FVector(0.f, 0.f, 15.f) + Dir * 50.f + Vel * (2.f / 120.f);
	// The charge carries your momentum: throw speed is added on top of your velocity.
	const FVector LaunchVelocity = Dir * C4ThrowSpeed + Vel * C4MomentumCarried + FVector(0.f, 0.f, 150.f);

	--ChargesHeld; // spent for good: it comes back from a pickup, not from detonating

	if (HasAuthority())
	{
		SpawnThrownC4(Start, View, LaunchVelocity);
		return;
	}

	// A client does not spawn its own: the server makes the one brick everybody shares. That costs
	// you a round trip before you see your own throw, which a lobbed object can afford - unlike the
	// detonation, which is why that one is predicted and this one is not.
	ServerThrowC4(Start, View, LaunchVelocity);
}

AGetawayC4* AGetawayCharacter::SpawnThrownC4(const FVector& Start, const FRotator& View, const FVector& LaunchVelocity)
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority()) return nullptr;

	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* Class = C4Class ? C4Class.Get() : AGetawayC4::StaticClass();
	AGetawayC4* Charge = World->SpawnActor<AGetawayC4>(Class, Start, View, Params);
	if (!Charge) return nullptr;

	const float GravityScale = GetCharacterMovement() ? GetCharacterMovement()->GravityScale : 1.f;
	Charge->Launch(LaunchVelocity, C4BlastRadius, C4BlastBoost, GravityScale);
	Charges.Add(Charge);
	return Charge;
}

void AGetawayCharacter::ServerThrowC4_Implementation(FVector_NetQuantize100 Start, FRotator View,
	FVector_NetQuantize100 LaunchVelocity)
{
	SpawnThrownC4(Start, View, LaunchVelocity);
}

void AGetawayCharacter::DetonateC4()
{
	if (HasAuthority())
	{
		TArray<TObjectPtr<AGetawayC4>> List = MoveTemp(Charges);
		Charges.Reset();
		for (const TObjectPtr<AGetawayC4>& C : List)
		{
			if (IsValid(C)) C->Detonate();
		}
		return;
	}

	// Push yourself now, off your own copies of the charges. A rocket jump is aimed and timed to
	// the frame; putting a round trip between pressing the key and being thrown would ruin it. The
	// server still owns the real bang - what breaks, what gets shoved - and the multicast knows to
	// leave your launch alone, having been beaten to it.
	UWorld* World = GetWorld();
	if (!World) return;
	for (TActorIterator<AGetawayC4> It(World); It; ++It)
	{
		if (It->GetOwner() == this)
		{
			It->PredictLocalBlast();
		}
	}
	ServerDetonateC4();
}

void AGetawayCharacter::ServerDetonateC4_Implementation()
{
	// Now on the authority, so this takes the branch above.
	DetonateC4();
}

/* ============================================================ camera */

void AGetawayCharacter::AddViewPunch(float Degrees)
{
	Punch = Degrees;
}

void AGetawayCharacter::AddCameraShake(float Amount)
{
	Shake = FMath::Max(Shake, Amount);
}

void AGetawayCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	// Velocity still holds the fall speed here.
	const float Fall = -GetCharacterMovement()->Velocity.Z; // cm/s
	const float Impact = Fall / 100.f; // m/s
	if (Impact > 9.f)
	{
		Punch = FMath::Min(3.4f, Impact * 0.172f);
	}

	// The thump, and the one movement sound not fired from inside the movement component: Landed is
	// an override on the character, and it is the only place the fall speed is still readable.
	if (UGetawayMovementComponent* Move = GetGetawayMovement())
	{
		Move->PlayLandSound(Fall);
	}
}

void AGetawayCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// ROLE_AutonomousProxy is set when the actor channel opens, a frame or two before the replicated
	// Controller arrives, so ask about the role as well as about the controller: without it a client
	// briefly treats its own character as someone else's and shuts its movement component down.
	const bool bMine = IsLocallyControlled() || GetLocalRole() == ROLE_AutonomousProxy;
	SetProxySimulation(!bMine && !HasAuthority());

	if (bMine)
	{
		CheckFallDeath();
		UpdateCamera(DeltaSeconds);
		SendNetMove();
	}
	else
	{
		// On the server a remote character is already being driven by the engine's ServerMoves plus
		// ServerUpdateMove; it is only on the other clients that it has to be interpolated.
		if (!HasAuthority())
		{
			ApplyProxyMove(DeltaSeconds);
		}
#if !UE_BUILD_SHIPPING
		ShowRemoteState();
#endif
	}

	UpdateBody();
}

/* ============================================================ client-authoritative networking */

EGetawayNetMove AGetawayCharacter::GetNetMoveState() const
{
	if (IsLocallyControlled() || GetLocalRole() == ROLE_AutonomousProxy)
	{
		if (const UGetawayMovementComponent* M = GetGetawayMovement())
		{
			if (M->IsMantling()) return EGetawayNetMove::Mantle;
			if (M->IsWallRunning()) return EGetawayNetMove::WallRun;
			if (M->IsSliding()) return EGetawayNetMove::Slide;
			if (M->IsFalling()) return EGetawayNetMove::Air;
			return bIsCrouched ? EGetawayNetMove::Crouch : EGetawayNetMove::Ground;
		}
	}
	return static_cast<EGetawayNetMove>(FMath::Min<uint8>(NetMove.State, static_cast<uint8>(EGetawayNetMove::Mantle)));
}

FGetawayNetMove AGetawayCharacter::SampleMove() const
{
	FGetawayNetMove Move;
	Move.Location = GetActorLocation();
	Move.Velocity = GetVelocity();
	const FRotator View = GetControlRotation();
	Move.Yaw = View.Yaw;
	Move.Pitch = View.Pitch;
	Move.State = static_cast<uint8>(GetNetMoveState());
	return Move;
}

void AGetawayCharacter::SendNetMove()
{
	UWorld* World = GetWorld();
	if (!World || GetNetMode() == NM_Standalone) return;

	const float Now = World->GetTimeSeconds();
	if (Now < NextSendTime) return;
	NextSendTime = Now + 1.f / FMath::Max(1.f, NetSendRate);

	const FGetawayNetMove Move = SampleMove();
	if (HasAuthority())
	{
		// The listen-server host owns its character outright, so it publishes its own state on the
		// same channel the clients use. Writing the property is what marks it dirty.
		NetMove = Move;
		LastNetMoveTime = Now;
	}
	else
	{
		ServerUpdateMove(Move);
	}
}

void AGetawayCharacter::ServerUpdateMove_Implementation(FGetawayNetMove Move)
{
	UWorld* World = GetWorld();
	if (!HasAuthority() || !World) return;

	// Relay first: this is what the other clients' simulated proxies read.
	if (LastNetMoveTime < 0.f)
	{
		UE_LOG(LogTemp, Log, TEXT("Getaway net: server is taking client-authoritative updates for %s"), *GetName());
	}
	NetMove = Move;
	LastNetMoveTime = World->GetTimeSeconds();

	if (!bServerFollowsClient || IsLocallyControlled()) return;

	// Take the client at its word. Nothing is checked here - that is the deal with client authority,
	// and it is why this is only good enough for a friends-only party game.
	SetActorLocation(Move.Location, false, nullptr, ETeleportType::TeleportPhysics);
	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->Velocity = Move.Velocity;
	}
	if (AController* C = GetController())
	{
		C->SetControlRotation(FRotator(Move.Pitch, Move.Yaw, 0.f));
	}
}

void AGetawayCharacter::OnRep_NetMove()
{
	if (const UWorld* World = GetWorld())
	{
		// Once per proxy per life: this line is how you tell from a log whether a remote player is
		// being driven at all, as opposed to standing at its spawn point looking lifelike.
		if (LastNetMoveTime < 0.f)
		{
			UE_LOG(LogTemp, Log, TEXT("Getaway net: first update for remote player %s, at %s"),
				*GetName(), *FVector(NetMove.Location).ToCompactString());
		}
		LastNetMoveTime = World->GetTimeSeconds();
	}
}

void AGetawayCharacter::SetProxySimulation(bool bProxy)
{
	if (bProxySimulationOff == bProxy) return;
	bProxySimulationOff = bProxy;

	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (!CMC) return;

	if (bProxy)
	{
		// A character we do not own is replayed from NetMove, never simulated. Left ticking, the
		// movement component would apply gravity and its own collision on top of the interpolation
		// and drag remote players through floors.
		CMC->StopMovementImmediately();
		CMC->DisableMovement();
		CMC->SetComponentTickEnabled(false);
	}
	else
	{
		CMC->SetComponentTickEnabled(true);
		if (CMC->MovementMode == MOVE_None)
		{
			CMC->SetMovementMode(MOVE_Falling);
		}
	}
}

void AGetawayCharacter::ApplyProxyMove(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	if (!World || LastNetMoveTime < 0.f) return; // nothing received yet: sit at the spawn transform

	// Dead reckon from the last packet so a remote player keeps moving between updates at 25 Hz,
	// then ease toward that. Capped, or a dropped packet would fling them across the level.
	const float Age = FMath::Clamp(World->GetTimeSeconds() - LastNetMoveTime, 0.f, ProxyExtrapolationLimit);
	const FVector Goal = FVector(NetMove.Location) + FVector(NetMove.Velocity) * Age;
	const FVector Here = GetActorLocation();

	const bool bSnap = FVector::DistSquared(Here, Goal) > FMath::Square(FMath::Max(1.f, ProxySnapDistance));
	const FVector Next = bSnap ? Goal : FMath::VInterpTo(Here, Goal, DeltaSeconds, ProxyInterpSpeed);
	SetActorLocation(Next, false, nullptr, ETeleportType::TeleportPhysics);

	if (UCharacterMovementComponent* CMC = GetCharacterMovement())
	{
		CMC->Velocity = NetMove.Velocity; // so GetVelocity() reads right for anything watching a proxy
	}

	// Yaw only, and through the shortest way round, or a proxy spins the long way past 180 degrees.
	const float Yaw = GetActorRotation().Yaw;
	const float Delta = FRotator::NormalizeAxis(NetMove.Yaw - Yaw);
	const float Blend = bSnap ? 1.f : FMath::Clamp(DeltaSeconds * ProxyInterpSpeed, 0.f, 1.f);
	SetActorRotation(FRotator(0.f, Yaw + Delta * Blend, 0.f));
}

void AGetawayCharacter::UpdateBody()
{
	if (!Body) return;
	const EGetawayNetMove State = GetNetMoveState();
	const bool bLow = (State == EGetawayNetMove::Crouch || State == EGetawayNetMove::Slide);
	const FVector Want(0.8f, 0.8f, bLow ? 1.f : 1.8f);
	if (!Body->GetRelativeScale3D().Equals(Want, 0.01f))
	{
		Body->SetRelativeScale3D(Want);
	}
}

/* ============================================================ the round's effect */

void AGetawayCharacter::ApplyRoundEffect()
{
	// The effect is on the game state, which replicates, so this reads the same on every machine -
	// and both players are on the same effect by construction, because there is only the one value.
	// Each machine applies it to its own character and to nobody else's: a proxy's movement
	// component is switched off and its position is streamed, so there is nothing there to tune.
	const UWorld* World = GetWorld();
	const AGetawayGameState* GS = World ? World->GetGameState<AGetawayGameState>() : nullptr;
	const EGetawayEffect Effect = GS ? GS->GetActiveEffect() : EGetawayEffect::None;
	if (Effect == EGetawayEffect::None) return;

	switch (Effect)
	{
	case EGetawayEffect::DoubleJump:
		if (UGetawayMovementComponent* Move = GetGetawayMovement())
		{
			// Max, so an effect round can never hand you *fewer* jumps than normal if the tuned
			// default is ever raised off zero.
			Move->SetAirJumps(FMath::Max(Move->AirJumps, EffectAirJumps));
		}
		break;

	case EGetawayEffect::Loaded:
		// Both halves of it: the charge you start with, and the one-charge hand that goes with it.
		// The cap matters because a C4 pickup anybody placed in an earlier round is still standing.
		MaxCharges = FMath::Max(1, EffectC4MaxCharges);
		ChargesHeld = FMath::Clamp(EffectC4Charges, 0, MaxCharges);
		break;

	case EGetawayEffect::LowGravity:
		if (UCharacterMovementComponent* Move = GetCharacterMovement())
		{
			// Multiplied rather than set, so it stays relative to whatever the character is tuned
			// to. The C4 launch reads GravityScale too, which is why a rocket jump goes further.
			Move->GravityScale *= EffectGravityScale;
		}
		break;

	default:
		break;
	}

	// Nothing is said here on purpose. AGetawayHUD::DrawRunEffect puts the effect in the middle of
	// the screen as the run starts and then moves it up out of the way, and a crosshair message
	// saying the same thing would land in the same place at the same moment.
}

void AGetawayCharacter::Notify(const FString& Text, float Seconds, const FLinearColor& Colour, int32 Key) const
{
	// Only the machine that owns this character has a HUD worth drawing on. On a listen server the
	// host's copy of a client's character does have a Controller, but that controller's HUD only
	// exists on the client, so this would be a null anyway - the check is here to say so plainly.
	if (!IsLocallyControlled()) return;

	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (AGetawayHUD* HUD = PC ? Cast<AGetawayHUD>(PC->GetHUD()) : nullptr)
	{
		HUD->ShowMessage(Text, Seconds, Colour, Key);
	}
}

#if !UE_BUILD_SHIPPING
void AGetawayCharacter::ShowRemoteState() const
{
	if (!bShowRemoteState || !GEngine || GetNetMode() == NM_Standalone) return;

	const EGetawayNetMove State = GetNetMoveState();
	const int32 Ups = FMath::RoundToInt32(FVector(NetMove.Velocity).Size2D() / 2.5f);
	// Keyed on the actor so each remote player gets its own line rather than overwriting the others.
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.2f, NetMoveColour(State),
		FString::Printf(TEXT("%s  %s  %d u/s"), *GetName(), NetMoveName(State), Ups));
}
#endif

void AGetawayCharacter::UpdateCamera(float DeltaSeconds)
{
	UGetawayMovementComponent* M = GetGetawayMovement();
	if (!M || !Camera) return;

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bFx = bCameraEffects;
	const float HalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// Eye height, smoothed relative to your feet so crouching and sliding ease down.
	const float TargetEye = (bIsCrouched || M->IsSliding()) ? CrouchedEyeHeightAboveFeet : StandingEyeHeight;
	EyeAboveFeet = FMath::FInterpTo(EyeAboveFeet, TargetEye, DeltaSeconds, 14.f);

	// Wallrun / slide tilt.
	const FRotator View = GetControlRotation();
	const FVector Right = FRotationMatrix(FRotator(0.f, View.Yaw, 0.f)).GetUnitAxis(EAxis::Y);
	float RollTarget = 0.f;
	if (bFx)
	{
		// Signs are flipped from the lab (lab: `-P.wall.n.dot(right)*0.15`, slide `0.03`). The formula
		// is the same, but three.js rolls about a camera Z that points back at the viewer, while
		// Unreal rolls about forward X in the aviation sense (positive = right side down). The same
		// number therefore tilts the opposite way, which is why a straight port leaned out of the
		// wall instead of into it. Don't "correct" this back to match the lab's source.
		if (M->IsWallRunning()) RollTarget = FVector::DotProduct(M->GetWallNormal(), Right) * 8.6f;
		else if (M->IsSliding()) RollTarget = -1.7f;
	}
	Roll = FMath::FInterpTo(Roll, RollTarget, DeltaSeconds, 8.f);

	// FOV widens with speed above a normal run.
	const float HSpeed = GetVelocity().Size2D();
	float FovTarget = BaseFOV;
	if (bFx)
	{
		FovTarget += FMath::Min(14.f, FMath::Max(0.f, (HSpeed - M->RunSpeed) / 100.f) * 0.9f) + (M->IsSliding() ? 3.f : 0.f);
	}
	CurrentFOV = FMath::FInterpTo(CurrentFOV, FovTarget, DeltaSeconds, 6.f);
	Camera->SetFieldOfView(CurrentFOV);

	// Double jump nudge.
	if (M->GetLastDoubleJumpTime() != SeenDoubleJumpTime)
	{
		SeenDoubleJumpTime = M->GetLastDoubleJumpTime();
		Punch = -1.15f;
	}
	Punch *= FMath::Pow(0.0005f, DeltaSeconds);
	Shake *= FMath::Pow(0.02f, DeltaSeconds);

	// Mantle jiggle: a quick dip and side-to-side wobble while you pull yourself up.
	float JigPitch = 0.f, JigRoll = 0.f;
	const float SinceMantle = Now - M->GetLastMantleTime();
	if (bFx && SinceMantle >= 0.f && SinceMantle < 0.42f)
	{
		const float U = SinceMantle / 0.42f;
		JigPitch = -FMath::Sin(U * PI) * 2.86f;
		JigRoll = FMath::Sin(U * PI * 3.f) * 2.f * (1.f - U);
	}

	const float S = bFx ? Shake * 20.f : 0.f;
	const FVector ShakeOffset(FMath::FRandRange(-S, S), FMath::FRandRange(-S, S), FMath::FRandRange(-S, S));
	Camera->SetRelativeLocation(FVector(0.f, 0.f, EyeAboveFeet - HalfHeight) + ShakeOffset);
	Camera->SetWorldRotation(FRotator(View.Pitch - (bFx ? Punch : 0.f) + JigPitch, View.Yaw, Roll + JigRoll));
}
