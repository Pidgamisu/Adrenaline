#include "GetawayRamp.h"

#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

AGetawayRamp::AGetawayRamp()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Mesh->SetMobility(EComponentMobility::Movable); // placed at runtime, so it cannot be Static

	// Use the convex hull for capsule sweeps and traces instead of the raw triangles: the movement
	// component wants one flat slope to read a floor normal off, and a trimesh used as simple
	// collision snags a capsule on its triangle edges.
	Mesh->bUseComplexAsSimpleCollision = false;
	Mesh->SetGenerateOverlapEvents(false);

	// Shared level geometry, same as AGetawayPiece. The wedge itself is not replicated and does not
	// need to be: OnConstruction runs on every machine, and the spawn transform it builds from
	// arrives with the actor.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
}

void AGetawayRamp::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	BuildWedge();
}

float AGetawayRamp::GetSlopeAngle() const
{
	// The unit wedge rises 100 over 100, so the played angle is whatever the scale makes of it.
	const FVector S = GetActorScale3D();
	return FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(S.Z), FMath::Max(KINDA_SMALL_NUMBER, FMath::Abs(S.X))));
}

void AGetawayRamp::BuildWedgeGeometry(TArray<FVector>& OutVerts, TArray<int32>& OutTris,
	TArray<FVector>& OutNormals, TArray<FVector2D>& OutUVs, TArray<FVector>& OutHull)
{
	// The wedge fills the same 100 cm cube AGetawayPiece uses, low at -X and full height at +X.
	const float R = 50.f;
	const FVector A(-R, -R, -R); // low edge, -Y
	const FVector B(-R, +R, -R); // low edge, +Y
	const FVector C(+R, -R, +R); // high edge, top, -Y
	const FVector D(+R, +R, +R); // high edge, top, +Y
	const FVector E(+R, -R, -R); // high edge, bottom, -Y
	const FVector F(+R, +R, -R); // high edge, bottom, +Y

	OutVerts.Reset();
	OutTris.Reset();
	OutNormals.Reset();
	OutUVs.Reset();

	// Unreal winds a front face so that (B-A) x (C-A) points *against* the outward normal; this is
	// the same order UKismetProceduralMeshLibrary::GenerateBoxMesh uses. Getting it backwards would
	// leave the ramp invisible from the outside while still being solid.
	auto AddFace = [&OutVerts, &OutTris, &OutNormals, &OutUVs](const TArray<FVector>& Ring, const FVector& Normal)
	{
		const int32 I = OutVerts.Num();
		for (int32 n = 0; n < Ring.Num(); ++n)
		{
			OutVerts.Add(Ring[n]);
			OutNormals.Add(Normal);
			OutUVs.Add(FVector2D(n == 1 || n == 2 ? 1.f : 0.f, n >= 2 ? 1.f : 0.f));
		}
		OutTris.Append({ I, I + 1, I + 2 });
		if (Ring.Num() == 4)
		{
			OutTris.Append({ I, I + 2, I + 3 });
		}
	};

	const FVector SlopeNormal = FVector(-1.f, 0.f, 1.f).GetSafeNormal();
	AddFace({ A, B, D, C }, SlopeNormal);            // the surface you run up
	AddFace({ A, E, F, B }, FVector(0.f, 0.f, -1.f)); // underside
	AddFace({ C, D, F, E }, FVector(1.f, 0.f, 0.f));  // the tall back wall at the top
	AddFace({ A, C, E }, FVector(0.f, -1.f, 0.f));    // triangular sides
	AddFace({ B, F, D }, FVector(0.f, 1.f, 0.f));

	OutHull = { A, B, C, D, E, F };
}

void AGetawayRamp::BuildWedge()
{
	if (!Mesh) return;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FVector> Hull;
	BuildWedgeGeometry(Verts, Tris, Normals, UVs, Hull);

	Mesh->ClearAllMeshSections();
	Mesh->ClearCollisionConvexMeshes();
	// No section collision: the convex hull below is the collision, and cooking a trimesh for every
	// ramp placed in a round is work for nothing.
	Mesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, TArray<FLinearColor>(),
		TArray<FProcMeshTangent>(), /*bCreateCollision=*/false);
	Mesh->AddCollisionConvexMesh(Hull);

	// After the section, never before: the slot to override only exists once there is a section
	// wearing it. Set unconditionally, so clearing the property in the Details panel puts the
	// engine default back instead of leaving the last material stuck on.
	Mesh->SetMaterial(0, SurfaceMaterial);
}
