#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GetawayMenuCamera.generated.h"

class UCameraComponent;

/**
 * What you look at behind the menu: the course itself, turning slowly.
 *
 * There is no menu map. The front end runs in whatever map the game starts in - TestCourse - and
 * the player holds this instead of a character, so the background is the real level rather than a
 * copy of it that would drift out of date. See *The front end* in docs/multiplayer.md.
 *
 * It has no movement component and no collision: it is a camera on a stick, flown by Tick alone.
 * By default it measures the level on its first tick and picks its own orbit, so a different map
 * needs no numbers typed in.
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayMenuCamera : public APawn
{
	GENERATED_BODY()

public:
	AGetawayMenuCamera();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UCameraComponent> Camera;

	/* ---------------- The orbit ---------------- */

	/**
	 * Degrees per second. 4 is a full turn every 90 seconds - slow enough that it reads as drift
	 * rather than as a turntable. Negative goes the other way round.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera")
	float OrbitSpeed = 4.f;

	/** Where on the circle it starts, in degrees. Change it to pick a different first view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera")
	float StartAngle = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera", meta = (ClampMin = "40", ClampMax = "140"))
	float FieldOfView = 90.f;

	/* ---------------- Framing ---------------- */

	/**
	 * Measure the level on the first tick and orbit that, writing the answer into OrbitCentre,
	 * OrbitRadius and OrbitHeight below so you can see what it picked. Turn it off to fly the
	 * circle you have typed in instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera|Framing")
	bool bAutoFrameLevel = true;

	/** How far outside the level's own footprint the camera sits. 1 would clip the near edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera|Framing", meta = (ClampMin = "1"))
	float FramePadding = 1.55f;

	/**
	 * Camera height above the middle of the level, as a fraction of the orbit radius - so 0.38 is
	 * always about 20 degrees above the horizontal, whatever shape the level is.
	 *
	 * It used to be a fraction of how tall the level is, which sounds more natural and is wrong
	 * for this game: a race course is long and flat, so TestCourse came out with the camera 8 m up
	 * looking at a 97 m ribbon edge-on from ground level. Off the radius it gives the same gentle
	 * look down on a short course and a long one.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera|Framing")
	float HeightFraction = 0.38f;

	/**
	 * What it aims at, as a fraction of the level's height above its middle. Below the camera, so
	 * it looks slightly down and the course fills the frame instead of sitting on the horizon.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera|Framing")
	float LookAtHeightFraction = 0.15f;

	/**
	 * Half extents above this, in cm, are left out of the measurement. 50 m is far larger than any
	 * single piece and far smaller than the things this is here to exclude - chiefly the gameplay
	 * debugger's 10 km rendering bounds, which every non-Shipping build carries. See FrameLevel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera|Framing", meta = (ClampMin = "100"))
	float IgnoreActorsBiggerThan = 5000.f;

	/* ---------------- The circle itself ---------------- */

	/*
	 * Filled in by the auto frame when it is on, and read as-is when it is off. Either way these
	 * three are what the camera actually flies, so they are the ones to nudge.
	 */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera|Circle")
	FVector OrbitCentre = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera|Circle", meta = (ClampMin = "100"))
	float OrbitRadius = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera|Circle")
	float OrbitHeight = 1200.f;

	/** Where the camera points, as a height off OrbitCentre. Filled in by the auto frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Menu Camera|Circle")
	float LookAtHeight = 0.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	/**
	 * Union the bounds of everything in the world that has a primitive root, and turn that box
	 * into a circle to fly. Skips actors with no size at all, which is every PlayerStart, light,
	 * volume and manager in the level.
	 */
	void FrameLevel();

	/** Put the camera on the circle at this angle, looking at the middle. */
	void ApplyOrbit(float AngleDegrees);

	float Angle = 0.f;
};
