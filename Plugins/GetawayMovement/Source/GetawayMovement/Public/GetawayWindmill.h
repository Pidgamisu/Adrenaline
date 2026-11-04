#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayWindmill.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class AGetawayWindmill;
class AGetawayCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGetawayWindmillSignature, AGetawayWindmill*, Windmill,
	AGetawayCharacter*, Who);

/**
 * A paddle wheel: four plates orbiting a horizontal axle, carrying you up and over.
 *
 * **The plates stay level** (decided 2026-11-03, asked for as "4 walls rotating kind of like
 * windmill blades you can stand on"). A true windmill, whose blades tilt as the wheel turns, tips
 * you off the moment the top face passes the walkable angle - about a quarter turn of ride. These
 * orbit the hub without turning, like the cars of a ferris wheel, so a plate is a floor the whole
 * way round and the piece is a lift you ride rather than a hazard you time. There is deliberately
 * no tilt option: tilting plates would also need `bIgnoreBaseRotation` on the character, because
 * the engine pipes a movement base's rotation into your view, and nothing else in the game turns
 * underfoot.
 *
 * **Standing on one is the engine's own based movement, not our code.** A plate is a Movable
 * blocking primitive, which is the whole of what `UCharacterMovementComponent::UpdateBasedMovement`
 * asks for: it carries whoever stands on it by the base's transform delta every frame. That is why
 * the plates are moved rather than rotated - a plate that never changes orientation can only ever
 * hand you a translation, so there is no case where the ride fights the camera.
 *
 * **The spin is derived from the run clock, not replicated.** The angle is a pure function of
 * `AGetawayGameState::GetRunTime()`, which is itself computed from the replicated run start time,
 * so every machine works out the same angle every frame with nothing going over the wire and no
 * drift to correct. That matters more here than it did for AGetawayVortex's cosmetic cage: this
 * spin decides where the floor is. It also means the wheel sits still at `StartAngle` through
 * placement and freezes when the round ends, for free - GetRunTime returns 0 outside a run.
 *
 * **A plate sweeping into you bats you off** rather than squeezing you through whatever is behind
 * you. Moving a component without sweeping does not push a character, so each plate carries a
 * contact box and hands its own velocity (omega x r) to anybody it catches who is not standing on
 * it, on their own machine only - the same bargain as AGetawayJumpPad, the spikes and the C4 blast.
 * `ShoveScale = 0` turns it off and leaves the plates as plain blocking geometry.
 *
 * It never kills you. Only spikes and falling do that - though a plate throwing you off the course
 * certainly can.
 *
 * Works with no assets (engine BasicShapes).
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayWindmill : public AActor
{
	GENERATED_BODY()

public:
	AGetawayWindmill();

	/** Fixed. The axle runs along its local +X, which placement lines up with the way you look. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<USceneComponent> Mount;

	/** Turns with the wheel. The hub and the arms hang off it; the plates do not, so they stay level. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<USceneComponent> Wheel;

	/** The barrel on the axle. Cosmetic, like the arms - you cannot stand on it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Hub;

	/* ---------------- The wheel ---------------- */

	/**
	 * How many plates, spaced evenly round the axle. Four is the piece as asked for; two makes a
	 * see-saw lift with a long wait, six a near-continuous escalator.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "1", ClampMax = "8"))
	int32 BladeCount = 4;

	/**
	 * Hub centre to plate centre, in cm. This is the wheel's radius, so it sets both how tall the
	 * piece is and how far up a ride takes you: at 250 the top of the stroke is 500 above the
	 * bottom, which is the vortex's whole column.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "50"))
	float BladeReach = 250.f;

	/** Plate size in cm: along the axle, across it, and thick. 350 x 200 is a landing you can miss. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "10"))
	float BladeLength = 350.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "10"))
	float BladeWidth = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "5"))
	float BladeThickness = 40.f;

	/**
	 * Degrees a second, and the piece's whole feel. 60 is a turn every six seconds, so three to
	 * ride from the bottom of the stroke to the top, and a plate centre moving at 262 cm/s against
	 * a run of 800 - fast enough to throw you, slow enough to step onto. Negative turns it the
	 * other way, which is what you want on the far side of a gap.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill")
	float SpinRate = 60.f;

	/** Where the wheel stands while nobody is running, in degrees. 0 puts a plate at the top. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill")
	float StartAngle = 0.f;

	/**
	 * On: the wheel turns on world time whenever there is no run on, so one dropped into a map by
	 * hand still moves. Off (the default) is what a placed piece wants - it holds at `StartAngle`
	 * through placement so you can build against a still wheel, and freezes when the round ends.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill")
	bool bSpinOutsideRun = false;

	/* ---------------- Being hit by one ---------------- */

	/**
	 * How much of a plate's own velocity it hands to somebody it sweeps into. 1 is exactly the
	 * plate's speed, which reads as being shoved along by it; above that it bats you. 0 leaves the
	 * plates as plain blocking geometry, so a plate moving into you only ever pushes you out of
	 * itself, which can mean through the wall behind you.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "0"))
	float ShoveScale = 1.3f;

	/** Seconds before the wheel can hit the same player again, so one pass is one hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "0"))
	float ShoveRetrigger = 0.5f;

	/** Camera kick on being hit, in degrees. The hit still punches your view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "0"))
	float ShoveViewPunch = 1.2f;

	/**
	 * Camera shake on being hit, in shake units. **0 on purpose** (2026-11-03, after playing): a
	 * plate can catch you more than once in a pass, and the shake read as the screen wobbling
	 * rather than as being hit - which the view punch says better on its own. Put a number back
	 * here to have it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "0"))
	float ShoveShake = 0.f;

	/* ---------------- Look ---------------- */

	/** The spokes, hub to plate. Cosmetic: no collision, so they never catch you. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "1"))
	float ArmThickness = 24.f;

	/** The barrel on the axle: how fat, and how far it runs along the axle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "5"))
	float HubRadius = 55.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill", meta = (ClampMin = "5"))
	float HubWidth = 140.f;

	/**
	 * What the plates and the spokes are made of. Both are pooled in the constructor, so the
	 * Components panel cannot reach them one by one; these two fields cover the lot. The hub is a
	 * named component and is dressed there instead. Empty leaves the engine default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill")
	TObjectPtr<UMaterialInterface> BladeMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Windmill")
	TObjectPtr<UMaterialInterface> ArmMaterial;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * The box the whole turning wheel sweeps out, in the piece's own axes. The catalog entry's Size
	 * has to match this or the placement ghost lies about the footprint, so it is here to be read
	 * rather than worked out by hand: at the defaults it is 350 x 700 x 540.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FVector GetSweptSize() const;

	/** The axle, in world space. Plates orbit about it; a hit is worked out along it, omega x r. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FVector GetAxleDirection() const;

	/** Where the wheel is now, in degrees. Derived, so every machine agrees without being told. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetWheelAngle() const;

	/** Armed only during a run, like the spikes, the pads and the vortex. It gates the shove. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsArmed() const { return bArmed; }

	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void SetArmed(bool bNew);

	/** Fired on the machine that owns a character, the frame a plate catches them. */
	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayWindmillSignature OnStruck;

protected:
	virtual void BeginPlay() override;

private:
	/** Size the hub, the spokes and the plates from the properties above. Runs on every machine. */
	void LayOutWheel();

	/** Put the plates and their contact boxes where the given wheel angle says they are. */
	void PositionBlades(float Angle);

	/** Hand each plate's own velocity to anybody it has swept into, on their own machine only. */
	void ShoveStruck();

	/** The direction of plate `Index` at wheel angle zero, in the piece's own axes. */
	FVector RestDirection(int32 Index, int32 Used) const;

	/** What you stand on. Movable blocking primitives, which is all based movement needs. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Blades;

	/** One per plate, a little bigger than it: what a sweep is noticed in. Never blocks anything. */
	UPROPERTY()
	TArray<TObjectPtr<UBoxComponent>> Contacts;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Arms;

	/** Who the wheel last hit and when, so one pass is one hit. Keyed like AGetawayJumpPad's. */
	TMap<TWeakObjectPtr<AGetawayCharacter>, float> LastShoveTime;

	bool bArmed = true;
};
