#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayVortex.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class AGetawayVortex;
class AGetawayCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGetawayVortexSignature, AGetawayVortex*, Vortex,
	AGetawayCharacter*, Who);

/**
 * An anti-gravity vortex: a column that holds you climbing for as long as you are inside it.
 *
 * The difference from AGetawayJumpPad is sustained versus instant. A pad is one kick on the frame
 * you touch it and gravity owns you afterwards; a vortex cancels gravity and holds you at
 * `LiftSpeed` the whole time you are in the column, so the height you get is decided by how long
 * you stay in it rather than by how hard it hit you.
 *
 * **You keep control the whole way up.** Source air strafing still works, so a fast run-up carries
 * its full arc through the column, and on top of that the vortex gives you direct horizontal
 * steering (`SteerSpeed`) - without which plain air acceleration would leave you riding it wherever
 * you entered. The steering only ever adds speed toward the keys you are holding, so the piece can
 * never slow anybody down.
 *
 * It never blocks you, the pattern every trigger piece here follows: the meshes have collision off
 * and the trigger is QueryOnly overlapping pawns only, like AGetawaySpikes, AGetawayJumpPad and
 * AGetawayFinish. You walk, slide, fall or jump into it and it takes you up.
 *
 * It is a free-standing piece rather than a surface-mounted one, so it stands on the floor (or
 * against whatever you aimed at) and obeys the "everything touches something" rule. The lift is
 * always straight up, never along the piece's own axis: placement lies a piece down on a sloped
 * face, and a vortex that fired sideways out of a ramp would be a different piece. The visible cage
 * is held upright to say so.
 *
 * Works with no assets (engine BasicShapes).
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayVortex : public AActor
{
	GENERATED_BODY()

public:
	AGetawayVortex();

	/** The column itself. Overlapping it is what lifts you; it is the whole piece. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UBoxComponent> Trigger;

	/** Thin discs at the bottom and the top, so you can read where the column starts and ends. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Base;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Cap;

	/** Holds the bars, and spins. Separate from the actor so the trigger box does not turn with them. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<USceneComponent> Column;

	/* ---------------- The lift ---------------- */

	/**
	 * The climb it holds you at, in cm/s. 1000 crosses the catalog's 500 cm column in about half a
	 * second and leaves you with 1000 cm/s, which coasts a further 250 cm - so around 7.5 m of
	 * height from the bottom if you ride the whole thing, against the 140 cm of a plain jump. Less
	 * if you steer out early, which is the point of the piece.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex", meta = (ClampMin = "0"))
	float LiftSpeed = 1000.f;

	/**
	 * How fast you reach LiftSpeed, in cm/s^2. It has to beat gravity (about 2000 here) by a long
	 * way or the catch feels soft: 6000 gets you there in about 0.17 s, which is the first 85 cm of
	 * a 500 cm column. Raise it if entry should feel like a snap rather than a shove.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex", meta = (ClampMin = "0"))
	float LiftAccel = 6000.f;

	/**
	 * How fast you can move sideways inside the column under your own steam, in cm/s. This is what
	 * lets you steer out of it, or line up a wall, instead of riding it straight up. It is a target,
	 * not a limit: run in faster and you keep what you had.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex", meta = (ClampMin = "0"))
	float SteerSpeed = 400.f;

	/** How sharply that steering answers the keys. A Source coefficient, like GroundAccelerate's 10. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex", meta = (ClampMin = "0"))
	float SteerAccel = 8.f;

	/**
	 * On: you cannot grab a wall while the column has you, so a vortex against a wall takes you up
	 * instead of handing you a wallrun whose own gravity fights the lift. Off lets a wallrun win.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex")
	bool bBlocksWallrun = true;

	/** Camera kick as the column catches you, in degrees and shake units. Lighter than a jump pad's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex", meta = (ClampMin = "0"))
	float LiftViewPunch = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex", meta = (ClampMin = "0"))
	float LiftShake = 0.2f;

	/* ---------------- Look ---------------- */

	/**
	 * Vertical bars round the edge of the column. They are what you see; they spin while armed.
	 * Eight rather than six, because the catalog's 300 cm ring leaves six of them over a metre apart.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex", meta = (ClampMin = "0", ClampMax = "12"))
	int32 BarCount = 8;

	/** World thickness of a bar and of the two discs, held constant however the piece is scaled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex", meta = (ClampMin = "1"))
	float BarThickness = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex", meta = (ClampMin = "1"))
	float DiscThickness = 10.f;

	/** How fast the bars turn, in degrees a second. It stops when the piece is disarmed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Vortex")
	float SpinRate = 220.f;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	/** The way this column lifts: straight up, whatever the piece is standing on. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FVector GetLiftDirection() const;

	/** Armed only during a run, like the spikes and the jump pad. Off between rounds. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsArmed() const { return bArmed; }

	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void SetArmed(bool bNew);

	/** Fired on the machine that owns a character, the frame the column first catches them. */
	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayVortexSignature OnEntered;

protected:
	virtual void BeginPlay() override;

private:
	/** Build the bars and size everything for whatever scale the catalog gave this piece. */
	void LayOutColumn();

	/** Hand the lift to everybody inside, on their own machine and nowhere else. */
	void LiftRiders(float DeltaSeconds);

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Bars;

	/** Who was inside last tick, so the camera kick happens once, as the column catches you. */
	TSet<TWeakObjectPtr<AGetawayCharacter>> Riding;

	float SpinAngle = 0.f;
	bool bArmed = true;
};
