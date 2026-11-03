#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPtr.h"
#include "GetawayBlast.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * What a C4 charge looks like going off: an additive flash that swells and fades, and a handful of
 * glowing shards thrown out of it.
 *
 * **It is real geometry, not Niagara, and that is the point.** `AGetawayC4::PlayBlastEffects` used
 * to fall back to `DrawDebugSphere`, whose whole body is compiled out of Shipping and Test builds -
 * so a detonation in a Shipping package was completely invisible, and it was the last thing keeping
 * the packaging config on Development. Same fix as the placement ghost got on 2026-11-02: swap the
 * debug draw for components that exist in every build. See *The explosion* in docs/look.md.
 *
 * Niagara is still supported and still wins: set `ExplosionEffect` on `BP_C4` and this is never
 * spawned. The slot was left alone deliberately.
 *
 * Purely cosmetic and purely local. It does not replicate - `AGetawayC4::MulticastDetonate` already
 * reaches every machine, and each one spawns its own copy. It has no collision and touches nothing.
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayBlast : public AActor
{
	GENERATED_BODY()

public:
	AGetawayBlast();

	/**
	 * Called by the C4 straight after spawning, so the flash is sized to that charge's blast
	 * rather than to a number typed in here. 0 leaves `FlashRadius` as it is.
	 */
	void SizeToBlast(float BlastRadius);

	/* ---------------- The flash ---------------- */

	/** How big the flash gets, in cm. Overwritten by SizeToBlast when the C4 spawns this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Flash", meta = (ClampMin = "1"))
	float FlashRadius = 330.f;

	/**
	 * What fraction of the charge's blast radius the flash fills. Under 1 on purpose: a flash drawn
	 * out to the full radius reads as bigger than the blast actually is, because the edge of the
	 * light is the last thing you see and the damage has no edge at all.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Flash", meta = (ClampMin = "0.05", ClampMax = "2"))
	float FlashRadiusFraction = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Flash", meta = (ClampMin = "0"))
	float FlashStartRadius = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Flash", meta = (ClampMin = "0.02"))
	float FlashTime = 0.22f;

	/** Where `Brightness` on the material starts. It falls to nothing over FlashTime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Flash", meta = (ClampMin = "0"))
	float FlashBrightness = 6.f;

	/**
	 * How solid the flash's see-through face is. Much higher than anything else in the palette,
	 * and it has to be: the edge band is what carries shape on a box, but on a sphere the band has
	 * almost nothing to grip - the surface only touches its own bounding box at six points - so a
	 * flash left on the usual 0.04 would be practically invisible. Here the face *is* the effect.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Flash", meta = (ClampMin = "0", ClampMax = "1"))
	float FlashFaceOpacity = 0.55f;

	/* ---------------- The shards ---------------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Shards", meta = (ClampMin = "0", ClampMax = "64"))
	int32 ShardCount = 16;

	/** Cube side in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Shards", meta = (ClampMin = "1"))
	float ShardSize = 14.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Shards", meta = (ClampMin = "0"))
	float ShardSpeed = 900.f;

	/** Added or taken off ShardSpeed per shard, so they do not travel as a shell. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Shards", meta = (ClampMin = "0"))
	float ShardSpeedVariance = 450.f;

	/**
	 * How much the spray is tilted upwards, 0 for an even ball and 1 for straight up. Some lift
	 * stops half the shards disappearing into the floor on the first frame.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Shards", meta = (ClampMin = "0", ClampMax = "1"))
	float ShardUpwardBias = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Shards", meta = (ClampMin = "0"))
	float ShardGravity = 1400.f;

	/** Degrees per second, each shard a random axis. Tumbling is most of what sells them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Shards", meta = (ClampMin = "0"))
	float ShardSpin = 540.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Shards", meta = (ClampMin = "0.02"))
	float ShardLife = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Shards", meta = (ClampMin = "0"))
	float ShardBrightness = 3.f;

	/**
	 * A shard is small enough that a band this wide covers the whole cube, which is what makes it
	 * read as a spark rather than as a tiny hollow crate.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Shards", meta = (ClampMin = "0"))
	float ShardBorderWidth = 10.f;

	/* ---------------- Colour ---------------- */

	/**
	 * Cyan, not pink, and that follows the rule in docs/look.md rather than breaking it: pink is
	 * architecture, and cyan is the colour of things that help you - which is what C4 is here, a
	 * rocket jump and a way through a wall. `MI_VB_Pickup` is already cyan for the same reason.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Colour")
	FLinearColor FaceColour = FLinearColor(0.55f, 1.f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Colour")
	FLinearColor EdgeColour = FLinearColor(0.40f, 1.f, 0.95f);

	/**
	 * The additive, two-sided, hollow-faced master the whole game is built out of. Additive is
	 * what makes this safe to draw through walls and through itself: it never occludes and it needs
	 * no sorting, so overlapping shards just add up.
	 *
	 * A soft reference resolved on the first frame rather than a constructor `FObjectFinder`, which
	 * would warn on every editor start until `adrenaline_look.py` has been run. Same reasoning as
	 * `AGetawayPlacementPawn::GhostMaterial`.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Blast|Colour")
	TSoftObjectPtr<UMaterialInterface> BlastMaterial =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_VB_Pane.M_VB_Pane")));

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	/** One shard's own motion. Stepped by hand rather than by physics: it is cosmetic, it must not
	 *  touch anything, and a deterministic step costs nothing. */
	struct FShardMotion
	{
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FVector SpinAxis = FVector::UpVector;
		/** A quaternion, not a rotator: two rotators do not compose by adding them. */
		FQuat Rotation = FQuat::Identity;
	};

	/** Resolve the material and make the two dynamic copies. False if there is nothing to draw with. */
	bool EnsureMaterials();

	/** Build the flash sphere and the shards. Runtime rather than constructor components, so
	 *  ShardCount can be changed without a fixed cap built into the class. */
	void BuildPieces();

	/**
	 * Held as hard references found in the constructor, not loaded by path when the blast goes
	 * off. That is what gets them cooked: nothing else in the project referenced
	 * `/Engine/BasicShapes/Sphere`, so a runtime load would have come back null in a packaged
	 * build and the flash would have been missing from the one kind of build this whole class
	 * exists to fix.
	 */
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;

	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Flash;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FlashMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> ShardMaterial;

	/**
	 * The shards as two parallel lists, the same way the game mode keeps its placed pieces: the
	 * components in a UPROPERTY so they are plainly owned and collected, and the motion beside it
	 * in plain memory because none of it is an object.
	 */
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> ShardMeshes;

	TArray<FShardMotion> ShardMotion;

	float Age = 0.f;
};
