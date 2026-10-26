#include "GetawayMovementComponent.h"

#include "GetawayBreakable.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UGetawayMovementComponent::UGetawayMovementComponent()
{
	// Lab gravity is 20 m/s^2 (sv_gravity 800). Default world gravity is -980 cm/s^2.
	GravityScale = 2000.f / 980.f;

	// Input acceleration is only used for its direction and analog amount; the Source code does the real work.
	MaxAcceleration = 2048.f;
	MaxWalkSpeed = 800.f;
	MaxWalkSpeedCrouched = 272.f;
	MaxStepHeight = 50.f;
	JumpZVelocity = 750.f;

	// Hand the full input to CalcVelocity in the air; Source air acceleration decides how much it matters.
	AirControl = 1.f;
	AirControlBoostMultiplier = 1.f;
	AirControlBoostVelocityThreshold = 0.f;
	FallingLateralFriction = 0.f;
	BrakingDecelerationWalking = 0.f;
	BrakingDecelerationFalling = 0.f;

	bOrientRotationToMovement = false;
	bCanWalkOffLedgesWhenCrouching = true;
	NavAgentProps.bCanCrouch = true;
	NavAgentProps.bCanJump = true;
	SetCrouchedHalfHeight(50.f);

	// The lab simulated at 120 Hz. Small substeps keep strafing and friction consistent at any frame rate.
	// (The Details panel clamps this to 0.0166 if you edit it there; set it in code to keep 120 Hz.)
	MaxSimulationTimeStep = 1.f / 120.f;
	MaxSimulationIterations = 16;
}

/* ============================================================ helpers */

FVector UGetawayMovementComponent::ViewForward() const
{
	const float Yaw = CharacterOwner ? CharacterOwner->GetControlRotation().Yaw : 0.f;
	return FRotationMatrix(FRotator(0.f, Yaw, 0.f)).GetUnitAxis(EAxis::X);
}

FVector UGetawayMovementComponent::ViewRight() const
{
	const float Yaw = CharacterOwner ? CharacterOwner->GetControlRotation().Yaw : 0.f;
	return FRotationMatrix(FRotator(0.f, Yaw, 0.f)).GetUnitAxis(EAxis::Y);
}

float UGetawayMovementComponent::CapsuleRadius() const
{
	return CharacterOwner ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleRadius() : 40.f;
}

float UGetawayMovementComponent::CapsuleHalfHeight() const
{
	return CharacterOwner ? CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
}

FVector UGetawayMovementComponent::FeetLocation() const
{
	return UpdatedComponent->GetComponentLocation() - FVector(0.f, 0.f, CapsuleHalfHeight());
}

float UGetawayMovementComponent::WishAnalog() const
{
	return FMath::Clamp(Acceleration.Size2D() / FMath::Max(GetMaxAcceleration(), 1.f), 0.f, 1.f);
}

bool UGetawayMovementComponent::JumpPending() const
{
	const float Now = GetWorld()->GetTimeSeconds();
	return (Now - JumpQueuedAt < JumpBufferTime) || (bAutoHop && bJumpHeld);
}

FCollisionQueryParams UGetawayMovementComponent::MoveQueryParams() const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GetawayMovement), false, CharacterOwner);
	return Params;
}

bool UGetawayMovementComponent::NearGround(float Distance) const
{
	const FVector Start = FeetLocation() + FVector(0.f, 0.f, 5.f);
	FHitResult Hit;
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, Distance + 5.f),
		UpdatedComponent->GetCollisionObjectType(), MoveQueryParams());
}

void UGetawayMovementComponent::QueueJump()
{
	JumpQueuedAt = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

bool UGetawayMovementComponent::IsWallRunning() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EGetawayCustomMove::WallRun);
}

bool UGetawayMovementComponent::IsMantling() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == static_cast<uint8>(EGetawayCustomMove::Mantle);
}

FString UGetawayMovementComponent::GetStateName() const
{
	if (IsMantling()) return TEXT("MANTLE");
	if (IsWallRunning()) return TEXT("WALLRUN");
	if (bSliding) return TEXT("SLIDE");
	if (!IsMovingOnGround()) return TEXT("AIR");
	if (IsCrouching()) return TEXT("CROUCH");
	return TEXT("GROUND");
}

float UGetawayMovementComponent::GetMaxSpeed() const
{
	if (MovementMode == MOVE_Custom) return RunSpeed * 2.f;
	if (IsCrouching() && !bSliding) return RunSpeed * CrouchSpeedScale;
	return RunSpeed;
}

bool UGetawayMovementComponent::CanCrouchInCurrentState() const
{
	// Stay crouched through a mantle onto a low-ceilinged ledge.
	return Super::CanCrouchInCurrentState() || IsMantling();
}

/* ============================================================ Source movement */

void UGetawayMovementComponent::SourceFriction(float Fric, float DeltaTime)
{
	const float Speed = Velocity.Size2D();
	if (Speed < 1.f)
	{
		Velocity.X = Velocity.Y = 0.f;
		return;
	}
	const float Drop = FMath::Max(Speed, StopSpeed) * Fric * DeltaTime;
	const float Scale = FMath::Max(Speed - Drop, 0.f) / Speed;
	Velocity.X *= Scale;
	Velocity.Y *= Scale;
}

void UGetawayMovementComponent::SourceAccelerate(const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime)
{
	if (WishSpeed <= 0.f || WishDir.IsNearlyZero()) return;
	const float Current = Velocity.X * WishDir.X + Velocity.Y * WishDir.Y;
	const float Add = WishSpeed - Current;
	if (Add <= 0.f) return;
	const float Amount = FMath::Min(Accel * DeltaTime * WishSpeed, Add);
	Velocity.X += WishDir.X * Amount;
	Velocity.Y += WishDir.Y * Amount;
}

void UGetawayMovementComponent::SourceAirAccelerate(const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime)
{
	if (WishSpeed <= 0.f || WishDir.IsNearlyZero()) return;
	const float Capped = FMath::Min(WishSpeed, AirSpeedCap);
	const float Current = Velocity.X * WishDir.X + Velocity.Y * WishDir.Y;
	const float Add = Capped - Current;
	if (Add <= 0.f) return;
	// Note: acceleration uses the full wish speed, only the target is capped. That's what lets strafing gain speed.
	const float Amount = FMath::Min(Accel * WishSpeed * DeltaTime, Add);
	Velocity.X += WishDir.X * Amount;
	Velocity.Y += WishDir.Y * Amount;
}

void UGetawayMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	const FVector WishDir = Acceleration.GetSafeNormal2D();
	const float Analog = WishAnalog();

	if (IsMovingOnGround())
	{
		if (bSliding)
		{
			SourceFriction(SlideFriction, DeltaTime);

			// Gravity pulls you down slopes while sliding.
			const FVector N = CurrentFloor.HitResult.ImpactNormal;
			if (CurrentFloor.IsWalkableFloor() && N.Z < 0.999f)
			{
				const FVector G(0.f, 0.f, GetGravityZ());
				const FVector Along = G - FVector::DotProduct(G, N) * N;
				Velocity.X += Along.X * DeltaTime;
				Velocity.Y += Along.Y * DeltaTime;
			}
			SourceAirAccelerate(WishDir, RunSpeed * Analog, AirAccelerate * 0.5f, DeltaTime);
		}
		else
		{
			// Source never applies friction on the tick you jump. Our jump happens on the next tick,
			// so skip friction while a jump is waiting to keep bunny hops lossless.
			if (!JumpPending())
			{
				SourceFriction(GroundFrictionSource, DeltaTime);
			}
			const float WishSpeed = (IsCrouching() ? RunSpeed * CrouchSpeedScale : RunSpeed) * Analog;
			SourceAccelerate(WishDir, WishSpeed, GroundAccelerate, DeltaTime);
		}
	}
	else
	{
		// PhysFalling calls this with the vertical velocity stripped out, so this only touches X/Y.
		SourceAirAccelerate(WishDir, RunSpeed * Analog, AirAccelerate, DeltaTime);
	}
}

/* ============================================================ per-tick state */

void UGetawayMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	SlideCooldownLeft -= DeltaSeconds;
	WallCooldown -= DeltaSeconds;

	// Keep the capsule size fixed for the whole mantle, so the climb lands where it was measured.
	if (IsMantling())
	{
		return;
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds); // crouch / uncrouch

	if (!HasValidData() || !CharacterOwner)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bFreshJump = Now - JumpQueuedAt < JumpBufferTime;
	const FVector WishDir = Acceleration.GetSafeNormal2D();
	const float Analog = WishAnalog();
	const float HSpeed = Velocity.Size2D();

	/* ---- jumping ---- */
	if (IsMovingOnGround() && JumpPending())
	{
		Velocity.Z = JumpSpeed;
		bSliding = false;
		JumpQueuedAt = -100.f;
		SetMovementMode(MOVE_Falling);
	}
	else if (IsWallRunning() && bFreshJump)
	{
		WallKick();
	}
	else if (IsFalling() && bFreshJump && AirJumpsLeft > 0)
	{
		// Don't spend the double jump on a press meant as a bunny hop just before landing.
		if (!NearGround(60.f) || Velocity.Z > 0.f)
		{
			--AirJumpsLeft;
			JumpQueuedAt = -100.f;
			LastDoubleJumpTime = Now;
			Velocity.Z = JumpSpeed * 0.92f;
			// Steer toward the keys you're holding without losing speed.
			if (Analog > 0.f)
			{
				const float Speed = FMath::Max(HSpeed, RunSpeed * 0.85f);
				Velocity.X = WishDir.X * Speed;
				Velocity.Y = WishDir.Y * Speed;
			}
		}
	}

	/* ---- slide ---- */
	if (IsMovingOnGround())
	{
		const float Speed = Velocity.Size2D();
		if (bSliding && (!bCrouchHeld || Speed < SlideStopSpeed))
		{
			bSliding = false;
		}
		if (!bSliding && bCrouchHeld && Speed >= SlideMinSpeed)
		{
			bSliding = true;
			if (SlideCooldownLeft <= 0.f && SlideBoost > 0.f && Speed > 1.f)
			{
				const float Scale = (Speed + SlideBoost) / Speed;
				Velocity.X *= Scale;
				Velocity.Y *= Scale;
				SlideCooldownLeft = SlideBoostCooldown;
			}
		}
	}
	else
	{
		bSliding = false;
	}

	/* ---- wallrun / mantle ---- */
	if (IsFalling())
	{
		if (!TryStartWallRun())
		{
			TryStartMantle();
		}
	}

	SmashAhead(DeltaSeconds);
	PushScraps();
}

void UGetawayMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		AirJumpsLeft = AirJumps;
		bHasLastWall = false;
	}
}

void UGetawayMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	switch (static_cast<EGetawayCustomMove>(CustomMovementMode))
	{
	case EGetawayCustomMove::WallRun: PhysWallRun(DeltaTime, Iterations); break;
	case EGetawayCustomMove::Mantle:  PhysMantle(DeltaTime, Iterations);  break;
	default: Super::PhysCustom(DeltaTime, Iterations); break;
	}
}

void UGetawayMovementComponent::ApplyBlast(const FVector& DeltaVelocity)
{
	if (IsWallRunning() || IsMantling())
	{
		SetMovementMode(MOVE_Falling);
	}
	Velocity += DeltaVelocity;
	if (IsMovingOnGround() && Velocity.Z > 50.f)
	{
		SetMovementMode(MOVE_Falling);
	}
}

/* ============================================================ wallrun */

bool UGetawayMovementComponent::TraceWall(const FVector& Dir, float Reach, FHitResult& OutHit, float& OutGap) const
{
	// The wall has to cover you from hip to chest height, and be close to vertical.
	const FVector Feet = FeetLocation();
	const float R = CapsuleRadius();
	const ECollisionChannel Channel = UpdatedComponent->GetCollisionObjectType();
	const FCollisionQueryParams Params = MoveQueryParams();

	FHitResult Low, High;
	const FVector LowStart = Feet + FVector(0.f, 0.f, 60.f);
	const FVector HighStart = Feet + FVector(0.f, 0.f, 120.f);
	if (!GetWorld()->LineTraceSingleByChannel(Low, LowStart, LowStart + Dir * (R + Reach), Channel, Params)) return false;
	if (!GetWorld()->LineTraceSingleByChannel(High, HighStart, HighStart + Dir * (R + Reach), Channel, Params)) return false;

	for (const FHitResult* H : { &Low, &High })
	{
		if (FMath::Abs(H->ImpactNormal.Z) > 0.3f) return false;
		// A breakable used to be refused here. It is a solid wall until somebody smashes it, so it
		// carries a wallrun like any other - and once it is broken its collision is off, which
		// takes it out of this trace anyway, so "until it breaks" needs no code of its own.
		if (H->GetComponent() && H->GetComponent()->IsSimulatingPhysics()) return false;
	}

	OutHit = Low;
	OutGap = FMath::Max(0.f, FMath::Min(Low.Distance, High.Distance) - R - 1.f);
	return true;
}

bool UGetawayMovementComponent::FindSideWall(float Reach, FHitResult& OutHit, float& OutGap) const
{
	const FVector Right = ViewRight();
	FHitResult RightHit, LeftHit;
	float RightGap = 0.f, LeftGap = 0.f;
	const bool bRight = TraceWall(Right, Reach, RightHit, RightGap);
	const bool bLeft = TraceWall(-Right, Reach, LeftHit, LeftGap);
	if (!bRight && !bLeft) return false;
	if (bRight && (!bLeft || RightGap <= LeftGap))
	{
		OutHit = RightHit;
		OutGap = RightGap;
	}
	else
	{
		OutHit = LeftHit;
		OutGap = LeftGap;
	}
	return true;
}

bool UGetawayMovementComponent::IsSameWall(const FVector& Normal, const FVector& Point) const
{
	return bHasLastWall
		&& FVector::DotProduct(Normal, LastWallNormal) > 0.99f
		&& FMath::Abs(FVector::DotProduct(Normal, Point) - LastWallPlane) < 20.f;
}

bool UGetawayMovementComponent::TryStartWallRun()
{
	if (WallCooldown > 0.f || bCrouchHeld) return false;

	const float HSpeed = Velocity.Size2D();
	if (HSpeed < WallRunMinSpeed || Velocity.Z < -1200.f) return false;

	FHitResult Hit;
	float Gap = 0.f;
	if (!FindSideWall(WallAttachRange, Hit, Gap)) return false;

	const FVector N = Hit.ImpactNormal.GetSafeNormal2D();
	// The same wall can't be grabbed again until you land (stops endless re-grabs up one wall).
	if (IsSameWall(N, Hit.ImpactPoint)) return false;
	if (NearGround(50.f)) return false;

	// Not moving away from the wall, not slamming straight into it, and the wall is beside you rather than ahead.
	const float Into = (Velocity.X * N.X + Velocity.Y * N.Y) / HSpeed;
	const float Facing = FVector::DotProduct(ViewForward(), N);
	if (Into >= 0.5f || Into <= -0.85f || FMath::Abs(Facing) >= WallSideTolerance) return false;

	WallNormal = N;
	WallActor = Hit.GetActor();
	LastWallNormal = N;
	LastWallPlane = FVector::DotProduct(N, Hit.ImpactPoint);
	bHasLastWall = true;
	WallRunTime = 0.f;

	// Pull onto the wall.
	if (Gap > 0.f)
	{
		FHitResult SnapHit;
		SafeMoveUpdatedComponent(-N * Gap, UpdatedComponent->GetComponentQuat(), true, SnapHit);
	}

	// Run along the wall in the direction you were already going (or facing).
	const FVector Along(-N.Y, N.X, 0.f);
	const float S = FVector::DotProduct(Along, Velocity);
	const float Sign = FMath::Abs(S) > 100.f ? FMath::Sign(S) : (FVector::DotProduct(Along, ViewForward()) >= 0.f ? 1.f : -1.f);
	const float Mag = FMath::Max3(FMath::Abs(S), HSpeed * 0.75f, WallRunMinSpeed + 10.f);
	const float NewZ = FMath::Max(Velocity.Z, 0.f) * 0.5f + 100.f;
	Velocity = Along * Sign * Mag;
	Velocity.Z = NewZ;

	AirJumpsLeft = AirJumps;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EGetawayCustomMove::WallRun));
	return true;
}

void UGetawayMovementComponent::EndWallRun(float PushOff)
{
	WallCooldown = 0.25f;
	WallActor.Reset();
	SetMovementMode(MOVE_Falling);
	Velocity += WallNormal * PushOff;
}

void UGetawayMovementComponent::WallKick()
{
	const FVector N = WallNormal;
	// Read before EndWallRun, which lets go of the wall.
	AGetawayBreakable* Breakable = Cast<AGetawayBreakable>(WallActor.Get());
	const float HSpeed = Velocity.Size2D();

	EndWallRun(0.f);
	Velocity += N * WallKickSpeed + ViewForward() * 120.f;
	Velocity.Z = JumpSpeed * 0.95f;
	JumpQueuedAt = -100.f;
	WallCooldown = 0.2f;

	// Pushing off a breakable is what breaks it. SmashAhead deliberately spares the wall you are
	// running on, so a panel holds you up for the whole run and gives way as you leave it - which
	// looks better than collapsing the instant you touch it, and is fairer, because a wall that
	// vanishes mid-run drops you with no warning. The speed test is the one SmashAhead would have
	// applied, so a gentle wallrun still leaves the panel standing.
	if (Breakable && !Breakable->IsBroken() && HSpeed >= SmashSpeed)
	{
		Breakable->Break(-N * HSpeed * 0.5f + FVector(0.f, 0.f, 150.f));
	}
}

void UGetawayMovementComponent::PhysWallRun(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME) return;

	WallRunTime += DeltaTime;

	FHitResult WallHit;
	float Gap = 0.f;
	const bool bWall = TraceWall(-WallNormal, 35.f, WallHit, Gap);
	const FVector WishDir = Acceleration.GetSafeNormal2D();
	const bool bPushingAway = WishAnalog() > 0.f && FVector::DotProduct(WishDir, WallNormal) > 0.6f;
	const float HSpeed = Velocity.Size2D();

	if (!bWall || !IsSameWall(WallHit.ImpactNormal.GetSafeNormal2D(), WallHit.ImpactPoint) || WallRunTime > WallRunMaxTime
		|| bCrouchHeld || HSpeed < WallRunMinSpeed || bPushingAway)
	{
		EndWallRun(120.f);
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	const FVector N = WallHit.ImpactNormal.GetSafeNormal2D();
	WallNormal = N;
	WallActor = WallHit.GetActor();
	FVector Along(-N.Y, N.X, 0.f);
	if (FVector::DotProduct(Along, Velocity) < 0.f) Along = -Along;

	float Speed = FVector::DotProduct(Along, Velocity);
	const float Target = RunSpeed * 1.15f;
	if (Speed < Target) Speed = FMath::Min(Target, Speed + 1400.f * DeltaTime);

	// Light gravity that ramps up over the last 40% of the run.
	const float Late = WallRunTime / FMath::Max(WallRunMaxTime, 0.01f);
	float Z = Velocity.Z + GetGravityZ() * (WallRunGravityScale + (Late > 0.6f ? (Late - 0.6f) * 1.2f : 0.f)) * DeltaTime;
	if (Z > 0.f) Z *= 1.f - 3.f * DeltaTime;

	Velocity = Along * Speed - N * 30.f; // slight pull keeps you hugging the wall
	Velocity.Z = Z;

	const FVector Delta = Velocity * DeltaTime;
	FHitResult Hit;
	SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);
	if (Hit.IsValidBlockingHit())
	{
		if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
		{
			// ProcessLanded only switches to walking from MOVE_Falling.
			SetMovementMode(MOVE_Falling);
			ProcessLanded(Hit, DeltaTime * (1.f - Hit.Time), Iterations);
			return;
		}
		HandleImpact(Hit, DeltaTime, Delta);
		if (FVector::DotProduct(Hit.Normal, Along) < -0.7f)
		{
			// Ran into something ahead: drop off the wall instead of grinding into the corner.
			EndWallRun(0.f);
			return;
		}
		SlideAlongSurface(Delta, 1.f - Hit.Time, Hit.Normal, Hit, false);
	}

	if (Velocity.Z <= 0.f)
	{
		FFindFloorResult Floor;
		FindFloor(UpdatedComponent->GetComponentLocation(), Floor, false);
		if (Floor.IsWalkableFloor() && Floor.FloorDist < MIN_FLOOR_DIST + 2.f)
		{
			SetMovementMode(MOVE_Falling); // ProcessLanded only lands from Falling, and this fires Landed()
			ProcessLanded(Floor.HitResult, 0.f, Iterations);
		}
	}
}

/* ============================================================ mantle */

bool UGetawayMovementComponent::TryStartMantle()
{
	const FVector Fwd = ViewForward();
	if (WishAnalog() <= 0.f || FVector::DotProduct(Acceleration.GetSafeNormal2D(), Fwd) < 0.5f) return false;

	UWorld* World = GetWorld();
	const FVector Feet = FeetLocation();
	const float R = CapsuleRadius();
	const ECollisionChannel Channel = UpdatedComponent->GetCollisionObjectType();
	const FCollisionQueryParams Params = MoveQueryParams();

	// Is there a wall just ahead?
	//
	// This used to be a single ray at knee height (feet + 35), which meant the ledge top had to be
	// more than 35 cm above your feet for the wall to be seen at all. On a 60 cm ledge that left a
	// window of feet < 25 cm - about two frames of a jump - so low ledges were effectively
	// unmantleable in the air and only caught in the instant you left the ground. The lab has no
	// such limit: it mantles off whatever you actually collided with, at whatever height contact
	// happened. Probe up the capsule instead and take the lowest wall, which restores the full
	// MantleMinHeight..MantleMaxHeight window.
	FHitResult WallHit;
	bool bFoundWall = false;
	for (float H = 20.f; H <= MantleMaxHeight && !bFoundWall; H += 20.f)
	{
		const FVector Start = Feet + FVector(0.f, 0.f, H);
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, Start + Fwd * (R + 30.f), Channel, Params)) continue;
		if (FMath::Abs(Hit.ImpactNormal.Z) > 0.3f || FVector::DotProduct(Fwd, Hit.ImpactNormal) > -0.5f) continue;
		// Breakables were skipped here too. A crate you have not broken is something to climb.
		WallHit = Hit;
		bFoundWall = true;
	}
	if (!bFoundWall) return false;
	const FVector N = WallHit.ImpactNormal;

	// Find the top of the ledge just past the wall face.
	const FVector Dir = -N.GetSafeNormal2D();
	const FVector Probe = WallHit.ImpactPoint + Dir * (R + 10.f); // where the capsule will stand on top
	FHitResult TopHit;
	const FVector TopStart(Probe.X, Probe.Y, Feet.Z + MantleMaxHeight + 20.f);
	const FVector TopEnd(Probe.X, Probe.Y, Feet.Z + MantleMinHeight - 5.f);
	if (!World->LineTraceSingleByChannel(TopHit, TopStart, TopEnd, Channel, Params)) return false;
	if (TopHit.bStartPenetrating || !IsWalkable(TopHit)) return false;

	const float Top = TopHit.ImpactPoint.Z;
	const float Rel = Top - Feet.Z;
	if (Rel < MantleMinHeight || Rel > MantleMaxHeight) return false;

	// Room to stand (or at least crouch) on top?
	//
	// Top is the surface height at a single probe point, but the landing capsule is R wide, and on
	// a slope the ground keeps climbing across that width. Sitting the capsule 2 cm above one
	// sample buried its uphill side and failed every fit test, which is why a ramp could not be
	// mantled from the side at all. Lift it by the rise the slope makes over the capsule's radius;
	// on flat ground the normal is straight up and this is zero, so nothing else changes.
	const float LandNZ = FMath::Max(0.2f, TopHit.ImpactNormal.Z);
	const float SlopeLift = R * FMath::Sqrt(FMath::Max(0.f, 1.f - LandNZ * LandNZ)) / LandNZ;

	const float StandHalf = CharacterOwner->GetDefaultHalfHeight(); // already scaled
	const float CrouchHalf = GetCrouchedHalfHeight() * CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const FVector LandXY(Probe.X, Probe.Y, 0.f);
	auto Fits = [&](float Half)
	{
		const FVector Center(LandXY.X, LandXY.Y, Top + Half + 2.f + SlopeLift);
		return !World->OverlapBlockingTestByChannel(Center, FQuat::Identity, Channel, FCollisionShape::MakeCapsule(R, Half), Params);
	};
	const bool bStandFits = Fits(StandHalf);
	if (!bStandFits && !Fits(CrouchHalf)) return false;

	// The climb straight up the wall face must be clear too (no overhangs or low ceilings).
	// The lab has no equivalent test, so keep it loose: this sweep starts at the capsule's own
	// position, which is pressed right up against the wall, and a near-full-width shape (R - 2)
	// left only 2 cm of clearance - enough to report a blocking hit on the very wall being climbed
	// and refuse a perfectly good mantle. Half width still catches real overhangs.
	const float FinalHalf = (bStandFits && !IsCrouching()) ? CapsuleHalfHeight() : CrouchHalf;
	const FVector Loc = UpdatedComponent->GetComponentLocation();
	if (World->SweepTestByChannel(Loc, FVector(Loc.X, Loc.Y, Top + FinalHalf + 2.f + SlopeLift), FQuat::Identity, Channel,
		FCollisionShape::MakeCapsule(FMath::Max(10.f, R * 0.5f), FinalHalf), Params))
	{
		return false;
	}

	MantleSavedVelocity = Velocity;
	if (!bStandFits && !IsCrouching())
	{
		bWantsToCrouch = true;
		Crouch(false);
	}

	MantleStart = UpdatedComponent->GetComponentLocation();
	MantleEnd = FVector(Probe.X, Probe.Y, 0.f);
	MantleDir = Dir;
	// Land a touch clear of a sloped top rather than inside it; the floor snap settles the rest.
	MantleTopCenterZ = Top + CapsuleHalfHeight() + 2.f + SlopeLift;
	MantleElapsed = 0.f;
	LastMantleTime = World->GetTimeSeconds();
	bSliding = false;
	Velocity = FVector::ZeroVector;
	SetMovementMode(MOVE_Custom, static_cast<uint8>(EGetawayCustomMove::Mantle));
	return true;
}

void UGetawayMovementComponent::PhysMantle(float DeltaTime, int32 Iterations)
{
	MantleElapsed += DeltaTime;
	Velocity = FVector::ZeroVector;

	const float K = FMath::Clamp(MantleElapsed / FMath::Max(MantleDuration, 0.01f), 0.f, 1.f);
	if (K < 1.f)
	{
		// Ease up the wall face.
		const float E = 1.f - FMath::Square(1.f - K);
		const FVector Target(MantleStart.X, MantleStart.Y, FMath::Lerp(MantleStart.Z, MantleTopCenterZ, E));
		UpdatedComponent->SetWorldLocation(Target);
		return;
	}

	// Step onto the ledge and carry most of the run-up speed.
	UpdatedComponent->SetWorldLocation(FVector(MantleStart.X, MantleStart.Y, MantleTopCenterZ));
	FHitResult Hit;
	const FVector Over(MantleEnd.X - MantleStart.X, MantleEnd.Y - MantleStart.Y, 0.f);
	SafeMoveUpdatedComponent(Over, UpdatedComponent->GetComponentQuat(), true, Hit);

	FVector Keep(MantleSavedVelocity.X, MantleSavedVelocity.Y, 0.f);
	Keep *= MantleSpeedKept;
	const float Forward = FVector::DotProduct(Keep, MantleDir);
	if (Forward < 400.f)
	{
		Keep += MantleDir * (400.f - Forward);
	}
	Velocity = Keep;
	bWantsToCrouch = bCrouchHeld;
	SetMovementMode(MOVE_Walking);
}

/* ============================================================ breakables and scraps */

void UGetawayMovementComponent::SmashAhead(float DeltaTime)
{
	const float HSpeed = Velocity.Size2D();
	if (!(HSpeed >= SmashSpeed || (bSliding && HSpeed >= SlideMinSpeed * 0.9f))) return;

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start + Velocity * (DeltaTime + 0.05f);
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByObjectType(Hits, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldDynamic),
		FCollisionShape::MakeCapsule(CapsuleRadius(), CapsuleHalfHeight()), MoveQueryParams());

	// The wall you are currently running on is spared: it breaks when you kick off it, in
	// WallKick, rather than disappearing out from under you mid-run.
	const AActor* CurrentWall = IsWallRunning() ? WallActor.Get() : nullptr;

	bool bBroke = false;
	for (const FHitResult& H : Hits)
	{
		if (AGetawayBreakable* B = Cast<AGetawayBreakable>(H.GetActor()))
		{
			if (B == CurrentWall) continue;
			if (!B->IsBroken())
			{
				B->Break(Velocity);
				bBroke = true;
			}
		}
	}
	if (bBroke)
	{
		Velocity.X *= 0.88f;
		Velocity.Y *= 0.88f;
	}
}

void UGetawayMovementComponent::PushScraps()
{
	// Scraps ignore the player's capsule, so we shove them out of the way here: harder the faster you go.
	const FVector Center = UpdatedComponent->GetComponentLocation();
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_PhysicsBody),
		FCollisionShape::MakeCapsule(CapsuleRadius() + 15.f, CapsuleHalfHeight()), MoveQueryParams());

	const FVector Flat(Velocity.X, Velocity.Y, 0.f);
	const float HSpeed = Flat.Size();
	for (const FOverlapResult& O : Overlaps)
	{
		UPrimitiveComponent* P = O.GetComponent();
		if (!P || !P->IsSimulatingPhysics() || !P->ComponentHasTag(AGetawayBreakable::ScrapTag)) continue;

		FVector D = P->GetComponentLocation() - Center;
		D.Z = 0.f;
		if (!D.Normalize())
		{
			D = FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal();
		}
		const float Approach = FMath::Max(0.f, FVector::DotProduct(Flat, D));
		FVector Want = D * (120.f + Approach * 1.25f) + Flat * 0.3f;
		Want.Z = (80.f + HSpeed * 0.22f) * FMath::FRandRange(0.6f, 1.1f);

		if (FVector::DotProduct(P->GetPhysicsLinearVelocity(), D) < FVector::DotProduct(Want, D) * 0.8f)
		{
			P->SetPhysicsLinearVelocity(Want);
			P->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * (200.f + HSpeed * 0.5f));
		}
	}
}
