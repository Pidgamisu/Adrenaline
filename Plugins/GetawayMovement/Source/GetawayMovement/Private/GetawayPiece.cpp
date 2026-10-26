#include "GetawayPiece.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGetawayPiece::AGetawayPiece()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Mesh->SetMobility(EComponentMobility::Movable); // placed at runtime, so it cannot be Static

	// A placed piece is shared level geometry: everyone has to have it, wherever they are standing,
	// so it replicates and ignores net relevancy. Its transform rides along with the spawn (see
	// AGetawayGameMode::PlacePiece) and never changes afterwards, so there is nothing to keep
	// sending - hence no movement replication.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
}
