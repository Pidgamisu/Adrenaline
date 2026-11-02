#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GetawayMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EGetawayCustomMove : uint8
{
	None    UMETA(Hidden),
	WallRun UMETA(DisplayName = "Wall Run"),
	Mantle  UMETA(DisplayName = "Mantle"),
};

/**
 * A ledge the mantle probe found: what it is, and where a climb would put you.
 *
 * Plain struct, not a USTRUCT: it lives for the length of one test and never leaves the component.
 */
struct FGetawayMantleFit
{
	/** The face you would climb, and the thing you would stand on. Usually the same actor. */
	const AActor* Wall = nullptr;
	const AActor* Ledge = nullptr;
	/** Flat direction into the wall, i.e. the way you go over the top. */
	FVector Dir = FVector::ZeroVector;
	/** Where the capsule ends up, X and Y only. */
	FVector LandXY = FVector::ZeroVector;
	/** World Z of the surface up there, and how much a slope lifts the landing capsule. */
	float Top = 0.f;
	float SlopeLift = 0.f;
	/** False when there is only room to crouch on top, so the climb has to duck. */
	bool bStandFits = false;
};

/**
 * Source-engine style movement for a first-person character, ported from the Getaway Movement Lab prototype.
 *
 * - Ground: Source friction (sv_friction / sv_stopspeed) and acceleration (sv_accelerate).
 * - Air: Source air acceleration with a tiny wish-speed cap, so air strafing gains speed and bunny hops keep it.
 * - Slide: crouch at speed for a boost and very low friction; slopes speed you up. A slide can't
 *   start again until SlideCooldown seconds after the last one ended, so it's a committed choice
 *   rather than something you tap over and over.
 * - Wallrun: a wall beside you (not in front) catches you; reduced gravity that ramps up late; Space kicks off.
 * - Mantle: jump into a ledge holding forward; you keep most of your speed going over the top.
 * - Double jump, auto-hop, jump buffering, smashing through AGetawayBreakable at speed, pushing loose scraps.
 *
 * All distances are centimetres. The lab displayed "u/s" where 1 u = 2.5 cm, so 320 u/s = 800 cm/s.
 * Single-player only: there are no custom saved moves, so this is not network-predicted.
 */
UCLASS(ClassGroup = (Getaway), meta = (BlueprintSpawnableComponent))
class GETAWAYMOVEMENT_API UGetawayMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UGetawayMovementComponent();

	/* ---------------- Ground ---------------- */

	/** Top running speed (cm/s). Lab default 320 u/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground", meta = (ClampMin = "0"))
	float RunSpeed = 800.f;

	/** sv_accelerate: how fast you reach RunSpeed on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground", meta = (ClampMin = "0"))
	float GroundAccelerate = 10.f;

	/** sv_friction: how fast you slow down on the ground with no input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground", meta = (ClampMin = "0"))
	float GroundFrictionSource = 5.f;

	/** sv_stopspeed (cm/s): below this, friction acts as if you were going this fast, so stops are crisp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground", meta = (ClampMin = "0"))
	float StopSpeed = 250.f;

	/** Crouch-walk speed as a fraction of RunSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground", meta = (ClampMin = "0", ClampMax = "1"))
	float CrouchSpeedScale = 0.34f;

	/* ---------------- Air ---------------- */

	/** sv_airaccelerate: air control and how much speed air strafing can gain. 100+ feels like a surf server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	float AirAccelerate = 12.f;

	/** Air wish-speed cap (cm/s). Source uses 30 u. Keep it small: this is what makes strafing work. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	float AirSpeedCap = 75.f;

	/** Upward speed of a jump (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	float JumpSpeed = 750.f;

	/** Extra jumps in the air. Refilled on landing and when a wallrun starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	int32 AirJumps = 1;

	/** Holding jump re-jumps the moment you land. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air")
	bool bAutoHop = true;

	/** A jump pressed this long before landing still counts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	float JumpBufferTime = 0.15f;

	/* ---------------- Slide ---------------- */

	/** Speed added when a slide starts (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideBoost = 250.f;

	/** Friction while sliding (compare GroundFrictionSource). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideFriction = 0.3f;

	/** Crouching above this speed starts a slide (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideMinSpeed = 550.f;

	/** A slide ends below this speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideStopSpeed = 280.f;

	/** Seconds before the slide boost can trigger again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideBoostCooldown = 1.f;

	/**
	 * Seconds after a slide ends before another one can start, however it ended: crouch released,
	 * slowed below SlideStopSpeed, jumped out of it, or left the ground. 0 lets you slide whenever
	 * you like, which is how it worked before this was added.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideCooldown = 1.5f;

	/* ---------------- Wallrun ---------------- */

	/** Gravity multiplier while wallrunning. It ramps up over the last 40% of the run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0"))
	float WallRunGravityScale = 0.18f;

	/** Longest a single wallrun can last (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0"))
	float WallRunMaxTime = 1.6f;

	/** Push away from the wall when you jump off (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0"))
	float WallKickSpeed = 550.f;

	/** How far from a wall you can be and still grab it (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0"))
	float WallAttachRange = 60.f;

	/** Minimum horizontal speed to start or keep a wallrun (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0"))
	float WallRunMinSpeed = 250.f;

	/** How "beside you" the wall must be: 0.65 means within about 40 degrees of straight left or right of your view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0", ClampMax = "1"))
	float WallSideTolerance = 0.65f;

	/* ---------------- Mantle ---------------- */

	/** Fraction of your run-up speed you keep after a mantle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Mantle", meta = (ClampMin = "0", ClampMax = "1"))
	float MantleSpeedKept = 0.75f;

	/** Seconds spent pulling yourself up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Mantle", meta = (ClampMin = "0.01"))
	float MantleDuration = 0.22f;

	/** Ledges this far above your feet can be mantled (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Mantle", meta = (ClampMin = "0"))
	float MantleMinHeight = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Mantle", meta = (ClampMin = "0"))
	float MantleMaxHeight = 170.f;

	/* ---------------- Breakables ---------------- */

	/** Hitting an AGetawayBreakable above this speed bursts it (cm/s). Slides break things at a little under SlideMinSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Breakables", meta = (ClampMin = "0"))
	float SmashSpeed = 900.f;

	/**
	 * A breakable you are about to mantle is climbed, not burst. Off, a crate in the air always
	 * bursts above SmashSpeed, because the break sweep reaches further ahead than the mantle probe.
	 * Panels and anything too tall to climb still break either way.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Breakables")
	bool bMantleOverSmash = true;

	/* ---------------- Input from the character ---------------- */

	void QueueJump();
	void SetJumpHeld(bool bHeld) { bJumpHeld = bHeld; }
	void SetCrouchHeld(bool bHeld) { bCrouchHeld = bHeld; }

	/** Add a velocity kick, e.g. from a C4 blast. Lifts you off the ground if it points upward. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void ApplyBlast(const FVector& DeltaVelocity);

	/**
	 * A sustained lift, from an AGetawayVortex you are inside. Unlike ApplyBlast this is not a kick:
	 * the vortex hands it over again every frame it holds you, and it expires by itself one frame
	 * after it stops doing so, so nothing has to tell the character it has left.
	 *
	 * UpSpeed is the climb it holds you at (cm/s) and UpAccel how fast you get there (cm/s^2). The
	 * steer pair is Source-style - SteerSpeed is a target in cm/s and SteerAccel a coefficient like
	 * GroundAccelerate - and it only ever adds speed toward the keys you are holding, so the lift
	 * can never slow down a player who arrived fast. The strongest UpSpeed wins if two overlap.
	 */
	void AddVortexLift(float UpSpeed, float UpAccel, float SteerSpeed, float SteerAccel, bool bBlockWallrun);

	/** True on a frame a vortex is holding you up. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsInVortex() const { return bVortexActive; }

	/* ---------------- State ---------------- */

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsSliding() const { return bSliding; }

	/** Seconds left before a slide can start again. 0 means sliding is ready. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetSlideCooldownLeft() const { return FMath::Max(0.f, SlideCooldownLeft); }

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsWallRunning() const;

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsMantling() const;

	UFUNCTION(BlueprintPure, Category = "Getaway")
	FVector GetWallNormal() const { return WallNormal; }

	/** World time of the last mantle / double jump, for camera effects. */
	float GetLastMantleTime() const { return LastMantleTime; }
	float GetLastDoubleJumpTime() const { return LastDoubleJumpTime; }

	/** GROUND, CROUCH, SLIDE, AIR, WALLRUN or MANTLE. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FString GetStateName() const;

	virtual float GetMaxSpeed() const override;
	virtual bool CanCrouchInCurrentState() const override;

protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	// Source movement primitives (PM_Friction, PM_Accelerate, PM_AirAccelerate)
	void SourceFriction(float Friction, float DeltaTime);
	void SourceAccelerate(const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime);
	void SourceAirAccelerate(const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime);

	// Helpers
	FVector ViewForward() const;
	FVector ViewRight() const;
	FVector FeetLocation() const;
	float CapsuleRadius() const;
	float CapsuleHalfHeight() const;
	float WishAnalog() const;
	bool JumpPending() const;
	bool NearGround(float Distance) const;
	FCollisionQueryParams MoveQueryParams() const;

	// Wallrun
	bool TraceWall(const FVector& Dir, float Reach, FHitResult& OutHit, float& OutGap) const;
	bool FindSideWall(float Reach, FHitResult& OutHit, float& OutGap) const;
	bool IsSameWall(const FVector& Normal, const FVector& Point) const;
	bool TryStartWallRun();
	void PhysWallRun(float DeltaTime, int32 Iterations);
	void EndWallRun(float PushOff);
	void WallKick();

	// Mantle
	/**
	 * The whole geometric question - is there a ledge ahead I could pull myself onto, and where
	 * would it put me - with nothing changed. ExtraReach looks further ahead than a climb would
	 * really start from, which is how SmashAhead can ask the same question its break sweep is
	 * about to answer for itself. TryStartMantle passes 0 and then commits to the answer.
	 */
	bool FindMantle(float ExtraReach, FGetawayMantleFit& Out) const;
	bool TryStartMantle();
	void PhysMantle(float DeltaTime, int32 Iterations);

	// Slide. Always stop a slide through EndSlide, so the cooldown starts no matter what ended it.
	void EndSlide();

	// Breakables and loose scraps
	void SmashAhead(float DeltaTime);
	void PushScraps();

	// Vortex lift
	void ApplyVortexLift(float DeltaSeconds);

	// Runtime state
	bool bJumpHeld = false;
	bool bCrouchHeld = false;
	bool bSliding = false;
	float JumpQueuedAt = -100.f;
	float SlideBoostCooldownLeft = 0.f;
	float SlideCooldownLeft = 0.f;
	int32 AirJumpsLeft = 0;
	float LastDoubleJumpTime = -100.f;

	FVector WallNormal = FVector::ZeroVector;
	/**
	 * What the current wallrun is actually on. Kept so a breakable can hold you up while you run
	 * it and give way when you push off: SmashAhead leaves this one alone, and WallKick breaks it.
	 */
	TWeakObjectPtr<AActor> WallActor;
	float WallRunTime = 0.f;
	float WallCooldown = 0.f;
	bool bHasLastWall = false;
	FVector LastWallNormal = FVector::ZeroVector;
	float LastWallPlane = 0.f;

	/**
	 * The lift a vortex has offered for the next update, and whether one is holding you now. Set by
	 * AddVortexLift and consumed once per tick, so a vortex that stops calling stops lifting - no
	 * end-overlap bookkeeping, and two overlapping columns resolve to the stronger one by themselves.
	 */
	float VortexUpSpeed = 0.f;
	float VortexUpAccel = 0.f;
	float VortexSteerSpeed = 0.f;
	float VortexSteerAccel = 0.f;
	bool bVortexBlocksWallrun = false;
	bool bVortexOffered = false;
	bool bVortexActive = false;

	FVector MantleStart = FVector::ZeroVector;
	FVector MantleEnd = FVector::ZeroVector;
	FVector MantleDir = FVector::ZeroVector;
	FVector MantleSavedVelocity = FVector::ZeroVector;
	float MantleTopCenterZ = 0.f;
	float MantleElapsed = 0.f;
	float LastMantleTime = -100.f;
};
