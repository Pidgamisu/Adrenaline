#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayRamp.generated.h"

class UProceduralMeshComponent;

/**
 * A wedge you run up: the lab's ramp piece. It rises along its own +X, which is the direction the
 * placement ghost's arrow points, so a ramp you place slopes away from you until you turn it.
 *
 * The engine's BasicShapes set has no wedge and this plugin ships no content, so the mesh is built
 * in code: six vertices, five faces, and a convex hull for collision. The hull matters - the
 * character sweeps its capsule against *simple* collision, and UGetawayMovementComponent reads the
 * floor normal to add gravity along a slope while sliding, so the slope has to be one clean plane
 * rather than a triangle soup.
 *
 * The wedge is built to fill a 100 cm cube, exactly like AGetawayPiece's cube, so the catalog sizes
 * it the same way: actor scale = Size / 100. At the catalog's 600 x 400 x 200 that is a 18.4 degree
 * slope.
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayRamp : public AActor
{
	GENERATED_BODY()

public:
	AGetawayRamp();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UProceduralMeshComponent> Mesh;

	/** Rebuilt on spawn and whenever the actor is edited in the level. */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** The slope angle in degrees for the current scale, which is what decides how it plays. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetSlopeAngle() const;

private:
	/** Builds the wedge that fills the unit cube, plus its convex collision. */
	void BuildWedge();
};
