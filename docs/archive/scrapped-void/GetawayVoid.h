#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayVoid.generated.h"

class USphereComponent;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class AGetawayVoid;
class AGetawayCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGetawayVoidSignature, AGetawayVoid*, Void,
	AGetawayCharacter*, Who);

/**
 * The void: a gravity well you slingshot off, with a lethal centre.
 *
 * A see-through sphere that pulls you toward its middle. Cut close to the core on your way through
 * and it bends your line and spits you out fast; cut too close and it kills you. **The best route
 * through a void is the one that nearly kills you**, and judging that is the whole piece.
 *
 * It is the first hazard that is **a field rather than a surface**. Spikes are a plate you must not
 * touch, so you route around the face they are on; a void has no face and no safe side. It occupies
 * a volume of the course and bends every line that passes through it.
 *
 * **How it got here, because the first version was wrong in a way worth remembering.** It started
 * as a capture: a strong pull that lifted you off the floor and damped your input, so that once the
 * field had you the outcome was decided and you spent two seconds watching it happen. It played
 * awful, and not because of any number - because every other piece in this catalog either *grants*
 * you movement (pad, ring, vortex, grapple, windmill) or *tests your precision* (spikes, the
 * panel), and that version did neither. It took ability away, in a game whose whole appeal is
 * movement control. The redesign on 2026-11-06 threw out the damping and the lift and gave the
 * piece a **payoff** instead, so the question changed from "can I escape it" (no) to "how close
 * dare I cut it" (your call). See *The slingshot* in docs/placement.md.
 *
 * **What it does now:**
 *
 * - **Pulls you**, uniformly, at `PullAccel` and never faster than `MaxPullSpeed`. It never brakes
 *   you and never removes speed, so arriving fast costs you nothing.
 * - **Leaves you your feet and your controls.** On the ground the pull lays flat along the floor
 *   (`bFlatPullOnGround`), so a wallrun, slide or bunny hop beside a void all still work, and a
 *   full run out-runs the drag. That is deliberate: the void is not trying to hold you.
 * - **Curves you in the air**, which is where it is interesting, and cancels gravity in proportion
 *   to how much of the pull points up so the curve reads as an orbit rather than sagging into a
 *   drop. A little weightlessness, which is a thing the piece gives you.
 * - **Pays you for cutting close.** On the way out you get a speed boost and a low-friction window
 *   scaled by your closest approach to the core - nothing at the rim, everything at the edge of
 *   death. See `bSlingshot`.
 * - **Kills you at the centre**, tested as a swept line so it cannot be tunnelled at pad speed.
 *
 * It never blocks you, the pattern every trigger piece here follows: the meshes have collision off
 * and the trigger is QueryOnly overlapping pawns only, like AGetawaySpikes, AGetawayJumpPad,
 * AGetawayVortex, AGetawaySpeedRing and AGetawayFinish. You can be pulled by a void, flung by one
 * and killed by one, but you can never be *stopped* by one.
 *
 * It is free-standing rather than surface mounted, so it stands on the floor (or against whatever
 * you aimed at), takes the 50 cm grid and the "everything touches something" rule unchanged. At the
 * catalog's 500 cm the core sits 250 cm up, well over your head. **A void wants placing in the air,
 * over a gap, or at the end of a jump pad** - somewhere you have no floor to stroll away on, which
 * is where the slingshot is worth taking.
 *
 * Non-directional: a sphere has nothing to face, so the placement arrow would be a lie.
 *
 * Works with no assets (engine BasicShapes).
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayVoid : public AActor
{
	GENERATED_BODY()

public:
	AGetawayVoid();

	/**
	 * The field. Overlapping it is what pulls you, and it is the whole piece: the shell you can see
	 * is drawn on its surface and the core sits at its centre.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<USphereComponent> Field;

	/** Holds the shell, and turns. Separate from the field so the trigger does not spin with it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<USceneComponent> Shell;

	/**
	 * The lethal middle, and the only solid-looking thing on the piece. It is deliberately a small
	 * ball you can see through the cage: the shell says where the field reaches and the core says
	 * what it is for.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Core;

	/**
	 * The graze line: a translucent ball around the core at `DangerRingScale` times the kill
	 * radius, so you have something to judge your distance against.
	 *
	 * Added with the slingshot, and it is not decoration. In first person you cannot read your
	 * distance to an 80 cm ball floating inside a five metre wireframe sphere, so before this you
	 * died without ever learning how close "too close" was. This is the line you *want* to graze -
	 * reading it is the skill the piece is asking for.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Danger;

	/* ---------------- The pull ---------------- */

	/**
	 * How hard the field pulls, in cm/s^2. Gravity here is about 2000, so this is a bit over a
	 * third of it - enough to bend an airborne line noticeably and not enough to stop a run.
	 *
	 * It is a *net* figure: the pull pays back the ground friction and the gravity that would
	 * otherwise eat it, so the number means what it says. See
	 * `UGetawayMovementComponent::ApplyVoidPull`.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "0"))
	float PullAccel = 700.f;

	/**
	 * What fraction of `PullAccel` is left at the very rim, rising linearly to the full value at
	 * the core. **1 since the slingshot redesign, so the field is uniform.**
	 *
	 * It was 0.25, which made the pull get stronger the deeper you went - and that is a funnel: the
	 * further in you were, the less you could do about it. A skill piece has to be predictable
	 * before it can be hard, and a uniform field is the one you can learn. Lower it again for a
	 * tighter, more dramatic orbit at the cost of being able to judge it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void",
		meta = (ClampMin = "0", ClampMax = "1"))
	float EdgePullScale = 1.f;

	/**
	 * The most speed **the field itself** will ever give you over one visit, in cm/s. Since the
	 * redesign this is the **curve strength**: how hard the well can swing an airborne player.
	 *
	 * Note what it is not: a cap on your own speed. Reading it that way is what made this piece
	 * play as doing nothing on the day it was built - running at a void means the pull points where
	 * you are already going, so a sprint spent the whole allowance before the field added anything.
	 * Speed you brought yourself is untouched by this number; 0 removes the cap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "0"))
	float MaxPullSpeed = 800.f;

	/**
	 * On: the pull is laid flat along the floor while you are stood on something, keeping its full
	 * strength but never lifting you.
	 *
	 * **On since the slingshot redesign, and it should stay on.** Off, a void peels you off the
	 * ground - which takes away your wallrun, your slide and your bunny hop all at once, and those
	 * are the game. On the ground a void is a drag you lean against; in the air it is a well that
	 * curves you. That division is the piece.
	 *
	 * It turns the pull along the floor rather than discarding its vertical part, which matters: a
	 * void standing on the floor has its core well above you, so the pull on a runner at ground
	 * level is steep, and simply dropping the Z would leave almost nothing of it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void")
	bool bFlatPullOnGround = true;

	/* ---------------- The kill ---------------- */

	/**
	 * How close your middle has to get to the centre to die, in world cm. 40 is an 80 cm ball
	 * inside a 500 cm sphere: small enough that reaching it is a decision rather than bad luck, and
	 * it is also the point where the slingshot pays out in full. `Danger` draws a ball around it at
	 * `DangerRingScale` times this, which is what you actually aim at.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "1"))
	float KillRadius = 40.f;

	/* ---------------- The slingshot ---------------- */

	/**
	 * On: leaving the field pays you for how close you cut, which is what turns the void from a
	 * punishment into a route. Off leaves a pull with a lethal middle and no reason to enter.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void")
	bool bSlingshot = true;

	/**
	 * What a perfect graze multiplies your top speed by on the way out, falling to 1 at the rim.
	 * 1.5 is the speed ring's figure, so a full slingshot gives the same 1200 cm/s ceiling a ring
	 * does - deliberately equal, because a void is a harder way to earn it.
	 *
	 * It pays out through `AddSpeedBoost` rather than a velocity kick, for the reason recorded in
	 * *A speed boost raises your ceiling, it does not shove you*: Source friction eats a kick in
	 * about a second, so "fast out of a void" has to mean a raised ceiling you can hold.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "1"))
	float SlingBoostScale = 1.5f;

	/**
	 * What a perfect graze multiplies ground and slide friction by on the way out, rising to 1 at
	 * the rim. The jump pad's other lever (`AddLowFriction`), here for the same reason: the well
	 * gives you the velocity and this is what stops the first touchdown taking it straight back.
	 * Landing out of a slingshot into a slide is the fast line.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void",
		meta = (ClampMin = "0", ClampMax = "1"))
	float SlingFrictionScale = 0.4f;

	/** How long both of those last after you leave, in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "0"))
	float SlingTime = 2.5f;

	/** Camera kick as the slingshot pays out, scaled by how close you cut. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "0"))
	float SlingViewPunch = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "0"))
	float SlingShake = 0.3f;

	/* ---------------- Look ---------------- */

	/**
	 * How many rings the shell is drawn with, stacked up the sphere like latitudes - widest at the
	 * equator, shortest near the poles. There is no sphere wireframe in the engine's BasicShapes
	 * set and this plugin ships no content, so the shell is a cage of stretched cylinders - the same
	 * approach as the vortex's bars and the speed ring's rim.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void",
		meta = (ClampMin = "1", ClampMax = "5"))
	int32 ShellRings = 5;

	/** How many dashes each ring is drawn with. The gaps are what make `SpinRate` visible. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void",
		meta = (ClampMin = "4", ClampMax = "16"))
	int32 ShellSegments = 14;

	/** How much of each dash's share of its ring is filled. 1 closes the gaps and hides the spin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void",
		meta = (ClampMin = "0.2", ClampMax = "1"))
	float SegmentFill = 0.62f;

	/** World thickness of a shell dash, held constant however the piece is scaled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "1"))
	float RingThickness = 14.f;

	/** How fast the shell turns, in degrees a second. It stops when the piece is disarmed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void")
	float SpinRate = 35.f;

	/**
	 * How big the graze ball is, as a multiple of `KillRadius`. 2.5 puts a 200 cm ball around the
	 * 80 cm core, which pays out about 70% of a full slingshot at the catalog's size - so the line
	 * you can see is a line worth aiming at. 0 or `bShowDangerLine` off hides it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "1"))
	float DangerRingScale = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void")
	bool bShowDangerLine = true;

	/**
	 * What the shell is made of. The dashes are pooled in the constructor, so they cannot be
	 * dressed from the Components panel the way the core can - this covers all of them at once. A
	 * translucent or glowing material is the one that reads right: you have to be able to see the
	 * course, and the core, through the cage. Empty leaves the engine default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void")
	TObjectPtr<UMaterialInterface> ShellMaterial;

	/** What the lethal core is made of. Applied unconditionally, so clearing it works. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void")
	TObjectPtr<UMaterialInterface> CoreMaterial;

	/**
	 * What the graze ball is made of. **It has to be translucent or it hides the core**, which is
	 * why it has a slot of its own rather than sharing `CoreMaterial`: it is the bigger of the two
	 * nested balls. `MI_VB_Danger` is the one, and `dress_pieces.py` puts it there. Undressed it is
	 * an opaque engine default and the core disappears inside it - the accepted cost on a fresh
	 * clone, as with the other hand-dressed pieces.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void")
	TObjectPtr<UMaterialInterface> DangerMaterial;

	/* ---------------- Feel ---------------- */

	/** Camera kick as the field first takes hold, in degrees and shake units. The lightest in the game. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "0"))
	float EnterViewPunch = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Void", meta = (ClampMin = "0"))
	float EnterShake = 0.15f;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** Where the pull points, and where the core is. The centre of the piece. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FVector GetCoreLocation() const { return GetActorLocation(); }

	/** How far the field reaches, in world cm. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetFieldRadius() const;

	/**
	 * How much of a slingshot a closest approach of `Closest` cm earns, 0 at the rim to 1 at the
	 * edge of the kill radius. Public because it is the piece's whole risk curve, and worth being
	 * able to read from Blueprint or a test.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetSlingFraction(float Closest) const;

	/** Armed only during a run, like the spikes, the jump pad, the vortex and the ring. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsArmed() const { return bArmed; }

	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void SetArmed(bool bNew);

	/** Fired on the machine that owns a character, the frame the field first takes hold of them. */
	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayVoidSignature OnEntered;

	/** Fired on the machine that owns a character, the frame the core kills them. */
	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayVoidSignature OnConsumed;

	/** Fired on the machine that owns a character, the frame they leave with a slingshot paid out. */
	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayVoidSignature OnSlung;

protected:
	virtual void BeginPlay() override;

private:
	/** Build the shell and size the two inner balls for whatever scale the catalog gave this piece. */
	void LayOutShell();

	/** Pull everybody inside, eat whoever reaches the middle, and sling whoever leaves. */
	void PullRiders(float DeltaSeconds);

	/** Hand out the boost and the low-friction window for one visit's closest approach. */
	void PaySlingshot(AGetawayCharacter* Who, float Closest);

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> ShellBars;

	/**
	 * One player's visit to this field: where they were last tick, and the closest they have come
	 * to the core since they entered.
	 *
	 * The previous point does two jobs - it is the start of the swept line the core is tested
	 * against, which is what stops the kill being tunnelled, and having no entry at all is how a
	 * new arrival is recognised for the camera kick. The closest approach is the slingshot: it is
	 * the *swept* distance, so a graze that happens between two frames still counts, which matters
	 * because the fastest passes are exactly the ones that skip over the core.
	 */
	struct FVisit
	{
		FVector Last = FVector::ZeroVector;
		float Closest = TNumericLimits<float>::Max();
	};

	TMap<TWeakObjectPtr<AGetawayCharacter>, FVisit> Visits;

	float SpinAngle = 0.f;
	bool bArmed = true;
};
