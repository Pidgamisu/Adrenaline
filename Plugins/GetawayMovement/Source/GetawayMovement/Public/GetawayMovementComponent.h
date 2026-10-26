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
 * Source-engine style movement for a first-person character, ported from the Getaway Movement Lab prototype.
 *
 * - Ground: Source friction (sv_friction / sv_stopspeed) and acceleration (sv_accelerate).
 * - Air: Source air acceleration with a tiny wish-speed cap, so air strafing gains speed and bunny hops keep it.
 * - Slide: crouch at speed for a boost (on a cooldown) and very low friction; slopes speed you up.
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

	/* ---------------- Input from the character ---------------- */

	void QueueJump();
	void SetJumpHeld(bool bHeld) { bJumpHeld = bHeld; }
	void SetCrouchHeld(bool bHeld) { bCrouchHeld = bHeld; }

	/** Add a velocity kick, e.g. from a C4 blast. Lifts you off the ground if it points upward. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void ApplyBlast(const FVector& DeltaVelocity);

	/* ---------------- State ---------------- */

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsSliding() const { return bSliding; }

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
	bool TryStartMantle();
	void PhysMantle(float DeltaTime, int32 Iterations);

	// Breakables and loose scraps
	void SmashAhead(float DeltaTime);
	void PushScraps();

	// Runtime state
	bool bJumpHeld = false;
	bool bCrouchHeld = false;
	bool bSliding = false;
	float JumpQueuedAt = -100.f;
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

	FVector MantleStart = FVector::ZeroVector;
	FVector MantleEnd = FVector::ZeroVector;
	FVector MantleDir = FVector::ZeroVector;
	FVector MantleSavedVelocity = FVector::ZeroVector;
	float MantleTopCenterZ = 0.f;
	float MantleElapsed = 0.f;
	float LastMantleTime = -100.f;
};
