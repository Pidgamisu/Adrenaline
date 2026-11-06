#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawaySpeedRing.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class AGetawaySpeedRing;
class AGetawayCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGetawaySpeedRingSignature, AGetawaySpeedRing*, Ring,
	AGetawayCharacter*, Who);

/**
 * A hoop you run through that speeds you up for a few seconds.
 *
 * It is a **gate**, which is what separates it from the other two movement pieces. A jump pad is
 * one kick on the frame you touch it; a vortex holds you for as long as you stay inside it; a
 * speed ring is passed through in a tenth of a second and its effect outlives the piece - the
 * boost runs on the character's own clock from there, so you carry it into the jump, the wallrun
 * or the slide that comes after. That is the whole point: the ring is not where the speed is
 * useful, it is where you *collect* it.
 *
 * **It makes you faster along the line you already took.** It never redirects you. The boost
 * raises `RunSpeed` by `BoostScale` for `BoostTime` seconds, which matters because Source friction
 * always drags you back toward `StopSpeed`: a bare velocity kick would be eaten in about a second
 * and "for three seconds" would mean nothing, whereas a raised ceiling means holding W keeps you
 * up there until the clock runs out. On top of that it scales the speed you arrive with straight
 * up to the new ceiling (`bSnapToSpeed`), so passing through feels like a surge rather than
 * homework - but only if you were already moving at `SnapMinSpeed`, so creeping into a ring does
 * not fling you off in whatever direction you were nudging.
 *
 * It never blocks you, the pattern every trigger piece here follows: the meshes have collision off
 * and the trigger is QueryOnly overlapping pawns only, like AGetawaySpikes, AGetawayJumpPad,
 * AGetawayVortex and AGetawayFinish. **The rim is not solid** - you cannot be stopped by clipping
 * the edge of a ring at 1200 cm/s, which would be the worst thing in the game. The trigger is
 * inset to the hole instead, so what you have to hit is the gap rather than the piece.
 *
 * It is **directional**: the hole's axis is the piece's own +X, which is the way the placement
 * arrow points, so a ring faces you as you place it and R turns it side on. It works going through
 * from either side - the facing says which way the hoop *lies*, not which way it pushes.
 *
 * It is free-standing rather than surface mounted, so it stands on the floor (or against whatever
 * you aimed at), takes the 50 cm grid and the "everything touches something" rule unchanged.
 * Unlike the vortex's cage the hoop is **not** held upright in world space: the vortex always
 * lifts straight up whatever it is sitting on, so a tilted cage would lie about it, whereas a
 * ring's axis is only the gap you have to fly through and it is free to tilt with the piece. In
 * practice it never does, because free-standing pieces stand square on a slope since 2026-11-05.
 *
 * Works with no assets (engine BasicShapes).
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawaySpeedRing : public AActor
{
	GENERATED_BODY()

public:
	AGetawaySpeedRing();

	/**
	 * The hole. Overlapping this is what boosts you, and it is inset to the inside of the rim so
	 * you have to go through the gap rather than brush the piece.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UBoxComponent> Trigger;

	/** Holds the rim segments, and spins. Separate from the trigger so the hole does not turn. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<USceneComponent> Ring;

	/* ---------------- The boost ---------------- */

	/**
	 * What the boost multiplies your top running speed by. 1.5 takes the tuned RunSpeed 800 to
	 * 1200 cm/s - clearly faster than anything you can reach on foot, and still steerable. It
	 * reaches running, crouch-walking, air strafing and a wallrun at once, because they all read
	 * the same number.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring", meta = (ClampMin = "1"))
	float BoostScale = 1.5f;

	/** How long the boost lasts after you pass through, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring", meta = (ClampMin = "0"))
	float BoostTime = 3.f;

	/**
	 * On: the ring lifts you to the boosted speed the instant you go through, so it hits like a
	 * surge. Off: only the ceiling goes up and you accelerate into it yourself, which is subtler
	 * and rewards already being fast. Either way the ring can only ever speed you up.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring")
	bool bSnapToSpeed = true;

	/**
	 * The least speed you must already be going for `bSnapToSpeed` to lift you, in cm/s. Below it
	 * you keep what you have and the raised ceiling does the work. It is here so walking carefully
	 * into a ring does not throw you at full speed in whatever direction you happened to be
	 * nudging - 300 is a bit over a third of a run, so anything that reads as "going somewhere"
	 * gets the surge.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring", meta = (ClampMin = "0"))
	float SnapMinSpeed = 300.f;

	/** Seconds before the same player can set this ring off again. One pass should be one boost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring", meta = (ClampMin = "0"))
	float Retrigger = 0.4f;

	/**
	 * Camera kick as you go through, in degrees and shake units. Lighter than a jump pad's 1.2 and
	 * 0.35: the ring does not change your direction, so a big punch would read as a collision.
	 * The lasting signal is the FOV, which is `BoostFOV` on AGetawayCharacter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring", meta = (ClampMin = "0"))
	float BoostViewPunch = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring", meta = (ClampMin = "0"))
	float BoostShake = 0.2f;

	/* ---------------- Look ---------------- */

	/**
	 * How many arc segments the rim is built from. There is no torus in the engine's BasicShapes
	 * set and this plugin ships no content, so the hoop is a circle of stretched cylinders - the
	 * same approach as the vortex's cage of bars.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring", meta = (ClampMin = "3", ClampMax = "24"))
	int32 SegmentCount = 12;

	/**
	 * How much of each segment's share of the circle is actually filled, so the rim is a dashed
	 * hoop rather than a smooth one. The gaps are what make `SpinRate` visible at all; 1 closes
	 * them up and the spin becomes invisible.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring", meta = (ClampMin = "0.2", ClampMax = "1"))
	float SegmentFill = 0.72f;

	/** World thickness of the rim, held constant however the piece is scaled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring", meta = (ClampMin = "1"))
	float RingThickness = 20.f;

	/** How fast the hoop turns about its own axis, in degrees a second. It stops when disarmed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring")
	float SpinRate = 70.f;

	/**
	 * What the rim is made of. The segments are pooled in the constructor, so they cannot be
	 * dressed from the Components panel the way a jump pad's plate can - this covers all of them
	 * at once. Empty leaves the engine default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Speed Ring")
	TObjectPtr<UMaterialInterface> RingMaterial;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The axis of the hole: the piece's own +X, which is the way the placement arrow pointed. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FVector GetGateAxis() const;

	/** Armed only during a run, like the spikes, the jump pad and the vortex. Off between rounds. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsArmed() const { return bArmed; }

	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void SetArmed(bool bNew);

	/** Fired on the machine that owns a character, the frame they pass through. */
	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawaySpeedRingSignature OnBoosted;

protected:
	virtual void BeginPlay() override;

private:
	/** Build the rim and size the hole for whatever scale the catalog gave this piece. */
	void LayOutRing();

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Segments;

	/** When each character was last boosted, so one pass through the hoop is one boost. */
	TMap<TWeakObjectPtr<AGetawayCharacter>, float> LastBoostTime;

	float SpinAngle = 0.f;
	bool bArmed = true;
};
