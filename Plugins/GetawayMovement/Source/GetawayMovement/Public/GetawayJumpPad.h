#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayJumpPad.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class AGetawayJumpPad;
class AGetawayCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGetawayLaunchedSignature, AGetawayJumpPad*, Pad,
	AGetawayCharacter*, Launched);

/**
 * A pad that throws you off whatever it is stuck to.
 *
 * It is **surface mounted** (`bSurfaceMounted` on its catalog entry), like AGetawaySpikes, so it
 * lies flat on the floor, wall, ceiling or ramp face you aim at and cannot be placed in mid-air.
 * **Its own +Z is the launch direction**, and that axis is the surface normal - so one catalog
 * entry gives you a trampoline on the floor, a horizontal shove off a wall and a slam off a
 * ceiling. There is nothing else to aim: the face you stick it to *is* the direction. R spins it
 * about the normal, which on a square plate you cannot see.
 *
 * It never blocks you. The meshes have collision off and the trigger is QueryOnly overlapping
 * pawns only, exactly like AGetawaySpikes and AGetawayFinish, so you run, slide or wallrun through
 * it and it throws you rather than stopping you first.
 *
 * The launch sets your speed *out of* the pad and leaves your speed *across* it alone, so a pad is
 * the same height every time whoever hits it, and a fast player still carries their run-up through.
 * A pad that fires along the ground you are standing on is tilted up by `GroundLift` first, so a
 * wall pad throws you off the floor instead of being eaten by friction.
 *
 * **The launch is the whole piece.** It used to hand you a low-friction window as well, so that
 * Source friction did not eat the launch on your first touchdown - played 2026-11-07 and taken
 * back out, because three seconds of the floor behaving differently is not something you can see
 * or aim, and it turned a kick into a surface condition. `LaunchSpeed` went up instead. The window
 * is still here as `FrictionScale` / `FrictionTime` and it is **off** (1 and 0); see
 * *The window is coming out* in `docs/placement.md`.
 *
 * Works with no assets (engine BasicShapes).
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayJumpPad : public AActor
{
	GENERATED_BODY()

public:
	AGetawayJumpPad();

	/** Overlap volume. Fills the piece; the meshes inside it are decoration. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UBoxComponent> Trigger;

	/** The plate against the surface. Fills the footprint, so a trimmed pad gets a smaller plate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Base;

	/** The disc that sticks out of the plate, so you can see which way the pad fires. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Piston;

	/* ---------------- The launch ---------------- */

	/**
	 * How hard it throws you, in cm/s along the pad's own +Z - **the whole of what a pad gives you**
	 * since the low-friction window came out on 2026-11-07.
	 *
	 * Raised from 1400 to 1800 in that change. Height off a **floor** pad is `LaunchSpeed^2 / 2g`
	 * with g = 1999 cm/s² (`GravityScale` 2.04), against the character's own 1.4 m jump:
	 *
	 *   1400 -> 4.9 m   1600 -> 6.4 m   **1800 -> 8.1 m**   2000 -> 10.0 m
	 *
	 * A **wall** pad spends the same number sideways instead, so it is also how far a wall throws
	 * you - at 1800, about 1690 cm/s across and 630 up once `GroundLift` has tilted it.
	 *
	 * Note what it does *not* change: your speed **across** the pad is untouched, so a floor pad
	 * never makes you faster along the ground, it buys you air - and air in this game is lossless
	 * (`FallingLateralFriction` is 0), which is why more height is itself the answer to friction
	 * eating your landing. It also keeps a pad above `SmashSpeed` 1100 by a wider margin than
	 * before, which is intended: per *Breaking is granted by the level* a pad is meant to be one of
	 * the few things that can burst a breakable.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad", meta = (ClampMin = "0"))
	float LaunchSpeed = 1800.f;

	/**
	 * Off: the pad decides the launch, so it is the same height every time (it still never *slows*
	 * you - arrive faster than LaunchSpeed and you keep what you had). On: LaunchSpeed is added to
	 * whatever you arrive with, so pads stack and jumping into one goes much higher.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad")
	bool bAddToIncoming = false;

	/**
	 * How much of your speed *across* the pad face survives the launch. 1 keeps all of it, so a
	 * running jump stays a long arc; lower values straighten your path out towards the pad's facing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad",
		meta = (ClampMin = "0", ClampMax = "1"))
	float SideSpeedKept = 1.f;

	/**
	 * A pad flat on a wall fires horizontally, and a horizontal shove handed to somebody standing
	 * on the floor is eaten by ground friction before it takes them anywhere. So a launch that flat
	 * is tilted up to this much Z **while you are on the ground** - the same trick the C4 blast
	 * uses, and the same 0.35. Hit the pad in the air, off a wallrun or on a slope and it fires
	 * exactly where it points; 0 turns the lift off everywhere. A pad firing steeply downward is
	 * never lifted, because a pad under a ceiling is meant to slam you into the floor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad",
		meta = (ClampMin = "0", ClampMax = "1"))
	float GroundLift = 0.35f;

	/** Seconds before the same player can set this pad off again. One pass should be one launch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad", meta = (ClampMin = "0"))
	float Retrigger = 0.25f;

	/* ---------------- The low-friction window, which is OFF ---------------- */

	/**
	 * **Off. Leave it off unless you are deliberately re-trying the idea.**
	 *
	 * How much friction would survive for `FrictionTime` seconds after the pad throws you: 1 is no
	 * change at all, 0.3 was the value that was played, 0 is frictionless ice. The pad only calls
	 * into the movement component at all when this is below 1 *and* `FrictionTime` is above 0, so
	 * at these defaults nothing happens and there is no window.
	 *
	 * **Why it is off** (played 2026-11-07, *"honestly it doesn't feel good"*): the reasoning was
	 * sound - `LaunchSpeed` is far above running speed and Source friction really does pull you
	 * back toward `StopSpeed` the moment you touch down - but it fixed that on the wrong axis. A
	 * launch is a moment you aim at; a window is three seconds of the floor behaving differently
	 * afterwards, which you did not ask for and cannot see. `LaunchSpeed` went up instead. The
	 * speed ring's boost survives the same test because a raised ceiling is something you *spend*.
	 *
	 * Setting both back (0.3 and 3) restores the old behaviour exactly, with no build - it covers
	 * sliding as well as running, windows refresh rather than stack, and one can never make you
	 * faster than you landed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad",
		meta = (ClampMin = "0", ClampMax = "1"))
	float FrictionScale = 1.f;

	/** How long the low-friction window lasts after the launch, in seconds. **0 = off.** */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad", meta = (ClampMin = "0"))
	float FrictionTime = 0.f;

	/** Camera kick on launch, in degrees and in shake units. Matches the C4 blast's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad", meta = (ClampMin = "0"))
	float LaunchViewPunch = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad", meta = (ClampMin = "0"))
	float LaunchShake = 0.35f;

	/* ---------------- Look ---------------- */

	/** World size of the disc, held constant however the plate is scaled or trimmed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad", meta = (ClampMin = "10"))
	float PistonWidth = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Jump Pad", meta = (ClampMin = "5"))
	float PistonHeight = 30.f;

	virtual void OnConstruction(const FTransform& Transform) override;

	/** The way this pad fires: its own +Z, which placement turned out of the surface. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FVector GetLaunchDirection() const;

	/** Armed only during a run, like the spikes. Off between rounds. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsArmed() const { return bArmed; }

	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void SetArmed(bool bNew);

	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayLaunchedSignature OnLaunched;

	/**
	 * Stick this pad to whatever it was placed on. Server only; the host replicates so every
	 * machine hooks itself up the same way. Same deal as AGetawaySpikes, including taking the
	 * component rather than the actor so a windmill plate carries the pad: a host that moves
	 * carries the pad, and breaking a breakable host takes the pad with it - so the panel under a
	 * pad is the answer to a route built on one.
	 */
	void AttachToHost(UPrimitiveComponent* InHost);

	UFUNCTION(BlueprintPure, Category = "Getaway")
	AActor* GetHost() const { return Host; }

	/** False while the host is broken: hidden, disarmed, waiting for the host to come back. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsPresent() const { return bPresent; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	/** Size and place the disc for whatever the plate has actually come out as. */
	void LayOutPiston();

	/** The launch itself, on the machine that owns that character and nowhere else. */
	void LaunchLocal(AGetawayCharacter* Who);

	void BindToHost();
	void SetPresent(bool bNew);

	UFUNCTION()
	void OnHostBroken(class AGetawayBreakable* Breakable);

	UFUNCTION()
	void OnHostRestored(class AGetawayBreakable* Breakable);

	UFUNCTION()
	void OnRep_Host() { BindToHost(); }

	/** What it is stuck to. Null for a pad dropped straight onto the level's own geometry. */
	UPROPERTY(ReplicatedUsing = OnRep_Host)
	TObjectPtr<AActor> Host;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	/** When each character was last thrown, so one pass through the pad is one launch. */
	TMap<TWeakObjectPtr<AGetawayCharacter>, float> LastLaunchTime;

	bool bPresent = true;
	bool bArmed = true;
};
