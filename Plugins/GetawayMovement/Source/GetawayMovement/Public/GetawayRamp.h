#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayRamp.generated.h"

class UProceduralMeshComponent;
class UMaterialInterface;

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

	/**
	 * What the wedge is made of. A procedural mesh has no material slot in the Details panel
	 * until its section exists, and the section is only built on spawn, so this is the only way a
	 * Blueprint child can dress a ramp. Empty leaves the engine default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ramp")
	TObjectPtr<UMaterialInterface> SurfaceMaterial;

	/** Rebuilt on spawn and whenever the actor is edited in the level. */
	virtual void OnConstruction(const FTransform& Transform) override;

	/** The slope angle in degrees for the current scale, which is what decides how it plays. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetSlopeAngle() const;

	/**
	 * The unit wedge's geometry: the vertices, triangles, normals and per-face UVs that fill the same
	 * 100 cm cube AGetawayPiece's cube does, plus the six points of its convex hull.
	 *
	 * Static and public because the placement ghost draws this same wedge
	 * (`AGetawayPlacementPawn::BuildGhostWedge`). A ghost that showed a box for a ramp was what the
	 * old debug lines did, and it meant the shape you lined up was not the shape you got.
	 */
	static void BuildWedgeGeometry(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
		TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FVector>& OutHull);

private:
	/** Builds the wedge that fills the unit cube, plus its convex collision. */
	void BuildWedge();
};
