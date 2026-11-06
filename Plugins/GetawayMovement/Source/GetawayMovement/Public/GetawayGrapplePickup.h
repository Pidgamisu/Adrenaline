#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayGrapplePickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class AGetawayCharacter;
class AGetawayGrapplePickup;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGetawayGrappleTakenSignature, AGetawayGrapplePickup*, Pickup, AGetawayCharacter*, Taker);

/**
 * Hands out the grappling hook. A fired hook is spent for good, so this is where one comes from -
 * the same bargain as C4 and AGetawayC4Pickup, and deliberately the same piece of furniture: a pad
 * on the floor with the thing itself turning above it.
 *
 * Your hand holds **one item**, C4 and hooks counted together (AGetawayCharacter::ItemCapacity), so
 * a hook pad and a C4 pad in the same stretch of course are a choice between them. Arrive holding
 * something and this takes nothing: it waits until your hand is empty, and a pickup that comes back
 * while you are standing on it is taken on the spot.
 *
 * It never blocks movement - the trigger overlaps pawns only and the meshes have no collision - so
 * you can run, slide or wallrun straight through it. While depleted the hook goes away and only the
 * pad is left, and it comes back when the player who took it dies (bReturnsOnDeath), so it is one
 * hook per life.
 *
 * **It is per player**, like the C4 pad and for the same reason: only the machine that owns a
 * character hands anything to it, so each player's copy of this actor keeps its own availability.
 * Take this pad and your rival can still take their own hook from it.
 *
 * Works with no assets (engine BasicShapes). Make a Blueprint child to restyle it; the catalog's
 * entry points at BP_Piece_GrapplePickup, which Content/Python/dress_pieces.py builds.
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayGrapplePickup : public AActor
{
	GENERATED_BODY()

public:
	AGetawayGrapplePickup();

	/** Overlap volume. Pawns only; nothing else notices it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<USphereComponent> Trigger;

	/** Marker on the ground. Stays visible while depleted, so you know a hook comes back here. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Pad;

	/** What the hook hangs off: bobs, spins, and is hidden while depleted. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<USceneComponent> Hook;

	/** The straight shank of the hook. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Shank;

	/* ---------------- Pickup ---------------- */

	/**
	 * Hooks handed over, clamped by the player's MaxGrappleShots and by the one-item hand. One, for
	 * the same reason the C4 pickup grants one: what you are carrying should be a single decision.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Pickup", meta = (ClampMin = "1"))
	int32 ShotsGranted = 1;

	/**
	 * It comes back when the player who took it dies, not on a clock - the C4 pickup's rule, and
	 * the same reasoning: one hook per life, and a pad you have emptied stays empty for as long as
	 * you stay alive. The character is held weakly and that is the whole test; see the matching
	 * note on AGetawayC4Pickup.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Pickup")
	bool bReturnsOnDeath = true;

	/** Seconds before it comes back on its own as well, or 0 for never - the default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Pickup", meta = (ClampMin = "0"))
	float RespawnDelay = 0.f;

	/** How far the hook bobs up and down (cm) and how fast it spins (degrees/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Pickup", meta = (ClampMin = "0"))
	float BobHeight = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Pickup")
	float SpinSpeed = 90.f;

	/* ---------------- Shape ---------------- */

	/** How high above the pad the hook floats (cm), measured to its middle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Hook", meta = (ClampMin = "0"))
	float HookHeight = 52.f;

	/** Thickness of the shank and the bend (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Hook", meta = (ClampMin = "0.5"))
	float HookThickness = 6.f;

	/** Length of the straight part (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Hook", meta = (ClampMin = "1"))
	float ShankLength = 46.f;

	/** Radius of the bend at the bottom (cm). The hook is this wide twice over, plus the curl. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Hook", meta = (ClampMin = "1"))
	float BendRadius = 15.f;

	/**
	 * Straight pieces the bend is made of. Five reads as a curve at speed and costs five draws; the
	 * pool is fixed in the constructor like AGetawayVortex's bars, and spares are hidden.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Hook", meta = (ClampMin = "1", ClampMax = "8"))
	int32 BendSegments = 5;

	/**
	 * What the hook is made of, shank and bend alike.
	 *
	 * A property rather than a per-component slot because the bend is pooled, so the Details panel
	 * cannot reach the segments - the same reason AGetawayVortex has BarMaterial. Cyan by the colour
	 * rule: this is a thing that helps you. See docs/look.md.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Hook")
	TObjectPtr<UMaterialInterface> HookMaterial;

	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayGrappleTakenSignature OnTaken;

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsAvailable() const { return bAvailable; }

	/** Make it available again now, whatever the timer says. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void Respawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	/** Hands the hook over if this player can take it. */
	bool TryGive(AActor* Other);

	/** Catches a player who arrived with a full hand, or who is standing here when it respawns. */
	void CheckWaitingPlayers();

	void SetAvailable(bool bNew);

	/** Shank and bend, from the four Hook numbers. Called on construction and on begin play. */
	void LayOutHook();

	/** The bend. Chords of a circle, so the shape is the numbers rather than a mesh. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Bend;

	/** Who took it, held weakly: the pointer going stale is how the pickup hears about a death. */
	TWeakObjectPtr<AGetawayCharacter> Taker;

	bool bAvailable = true;
	float Age = 0.f;
	float RecheckTimer = 0.f;
	float RespawnCountdown = 0.f;
};
