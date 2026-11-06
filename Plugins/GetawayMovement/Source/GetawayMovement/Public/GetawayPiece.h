#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayPiece.generated.h"

class UStaticMeshComponent;

/**
 * One entry in the party box: what a piece is called, how big it is and what to spawn for it.
 *
 * Sizes are the full extent in cm, unrotated, and come from the browser lab's PTYPES table
 * (lab metres x 100, with the lab's Y-up swapped into Unreal's Z-up).
 */
USTRUCT(BlueprintType)
struct FGetawayPieceDef
{
	GENERATED_BODY()

	/** Short id, handy for logs and for picking a piece from script. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Piece")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Piece")
	FString DisplayName;

	/** Full extent in cm before rotation. The placement ghost is this box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Piece")
	FVector Size = FVector(400.f, 400.f, 50.f);

	/** Spawned when the piece is placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Piece")
	TSubclassOf<AActor> ActorClass;

	/**
	 * Scale the spawned actor to Size / 100. True for anything whose root is the engine's 100 cm cube
	 * (AGetawayPiece, AGetawayBreakable); false for actors that build their own shape, like
	 * AGetawayC4Pickup, which is placed at its own size and only uses Size for the ghost.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Piece")
	bool bScaleToSize = true;

	/**
	 * The piece cares which way round it is: a ramp rises along its own +X, a collapsing bridge
	 * falls that way. The lab calls these DIRTYPES and draws an arrow for them, which is what the
	 * placement ghost does here.
	 *
	 * For a directional piece Size.X is the length along that facing, because placement lines a
	 * piece's +X up with the way you are looking.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Piece")
	bool bDirectional = false;

	/**
	 * The piece mounts flat on whatever it is stuck to, rather than being a free-standing block:
	 * its local +Z turns to face out of the surface, and it **cannot be placed without one**. No
	 * ground-plane fallback, nothing hanging in mid-air - aim at real geometry or you get nothing.
	 *
	 * For a surface-mounted piece Size is the plate in its own axes, so Size.Z is the thickness
	 * measured out from the wall; the two large dimensions lie along it. That is why spikes read as
	 * 400 x 400 x 50 here and still come out the same size as a wall.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Piece")
	bool bSurfaceMounted = false;

	/**
	 * Special, rather than ordinary structure. The party box deals one of each, so a hand is one
	 * piece you build the course *out of* and one piece that changes how it is *moved through*.
	 *
	 * Special is the pickups, the hazards and the speed devices: C4, the grapple pad, spikes, the
	 * jump pad, the vortex, the speed ring. Normal is everything you place and stand on, which
	 * deliberately includes the breakable panel, the crate, the windmill and the collapsing bridge -
	 * since *Breaking is granted by the level* a panel nobody has put a pad in front of is only a
	 * wall, the windmill is a lift you ride, and the bridge is a floor that happens to be a lie.
	 *
	 * Decided 2026-11-06, going through the catalog a row at a time. See AGetawayGameMode::bOneOfEach,
	 * which is what honours it, and *One normal and one special* in docs/placement.md.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Piece")
	bool bSpecial = false;

	bool IsValid() const { return ActorClass != nullptr && !Size.IsNearlyZero(); }
};

/**
 * A plain solid block placed during the placement phase: floors, walls, ledges. Scale it to size.
 *
 * Deliberately dumb - it is just collision and a mesh. Anything with behaviour gets its own class
 * (AGetawayBreakable, AGetawayC4Pickup) and goes in the catalog pointing at that class instead.
 *
 * Works with no assets (engine BasicShapes).
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayPiece : public AActor
{
	GENERATED_BODY()

public:
	AGetawayPiece();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Mesh;
};
