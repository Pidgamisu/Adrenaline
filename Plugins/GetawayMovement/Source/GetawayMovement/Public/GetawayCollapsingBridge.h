#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayCollapsingBridge.generated.h"

class UStaticMeshComponent;
class AGetawayCollapsingBridge;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGetawayBridgeCollapsedSignature, AGetawayCollapsingBridge*, Bridge);

/** Where a bridge is in its one short life. Replicated, so it has to be a UENUM. */
UENUM(BlueprintType)
enum class EGetawayBridgeState : uint8
{
	/** Solid and waiting. */
	Idle,
	/** Someone is on it. It rattles, then lets go. */
	Warning,
	/** Simulating physics on its way down. */
	Fallen,
};

/**
 * The lab's collapsing bridge: a long deck that holds just long enough to tempt you across, then
 * gives way. "It won't hold. Ride it down."
 *
 * Stand on it and CollapseDelay starts; it rattles for that long, then drops under physics with a
 * downward kick at its far end (its own +X, the way the placement arrow points) so it tips nose
 * first instead of sinking flat. It keeps its collision all the way down, so riding it is fair
 * game. Once it has gone it stays gone: the round puts it back between rounds, which is what
 * makes it a real obstacle rather than a four-second inconvenience. bRespawns is the old
 * behaviour, off by default since 2026-10-26.
 *
 * Deliberately simple physics rather than a hinge or a fracture: this project scrapped heavy
 * destruction on purpose.
 *
 * Scale the actor to size it, like AGetawayPiece - the deck is the engine's 100 cm cube.
 * Works with no assets (engine BasicShapes).
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayCollapsingBridge : public AActor
{
	GENERATED_BODY()

public:
	AGetawayCollapsingBridge();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Seconds it holds after someone first stands on it. The lab warns for 0.45 s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Bridge", meta = (ClampMin = "0"))
	float CollapseDelay = 0.45f;

	/**
	 * On, it rights itself after RespawnDelay. **Off since 2026-10-26, and that is the default**:
	 * a bridge that came back mid-run was something the player who dropped it could watch fall and
	 * then walk over anyway. It now stays down until the next round, which is what AGetawayGameMode
	 * restores it for. Turn it on for a bridge in a level that has no round driving it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Bridge")
	bool bRespawns = false;

	/** Seconds from giving way to standing again. Only used when bRespawns is on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Bridge", meta = (ClampMin = "0"))
	float RespawnDelay = 6.f;

	/** How far the deck rattles (cm) while it is about to go. 0 for no warning at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Bridge", meta = (ClampMin = "0"))
	float WarnShake = 3.f;

	/** Downward kick (cm/s) at the far end as it lets go, so it tips the way it faces. 0 drops it flat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Bridge", meta = (ClampMin = "0"))
	float TipSpeed = 220.f;

	/**
	 * How far the deck has to fall below where it started before it is cleaned up, when it is not
	 * coming back. 0 leaves the wreck lying wherever it lands.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Bridge", meta = (ClampMin = "0"))
	float CleanupDrop = 3000.f;

	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayBridgeCollapsedSignature OnCollapsed;

	/** True while it is still something you can run on. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsStanding() const { return State != EGetawayBridgeState::Fallen; }

	/** Let go now, whatever the delay says. Hook this up to a trap or a blast. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void Collapse();

	/** Put it back where it started, now. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void Restore();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	/** Is a pawn standing on the deck? Polled rather than using a trigger volume, so the probe can
	 *  be sized from the actor's scale and stay a fixed height whatever the deck is scaled to.
	 *  Server only: it is the only machine that sees every player. */
	bool IsStoodOn() const;

	/**
	 * The physical consequences of State, run on every machine. The server decides *when* the state
	 * changes and replicates that; each machine then drops or rights its own copy. The deck is not
	 * movement-replicated - a rider needs the platform under them to be local and immediate, not
	 * streamed a frame late - so the two copies fall independently from the same impulse. They will
	 * not land identically, which does not matter: by the time it has stopped moving, the ride is
	 * over. What matters is that both start falling at the same moment, and they do.
	 */
	void StartFalling();
	void StopFalling();

	/**
	 * Stop a deck that has fallen out of the level and put it out of sight, keeping the actor.
	 * It used to Destroy() itself here, which was fine while that only happened to a bridge set
	 * not to respawn - but every bridge is that now, and a destroyed one is not there for the
	 * round to restore. Runs on each machine for its own copy, since each falls independently.
	 */
	void Park();

	virtual void FellOutOfWorld(const class UDamageType& DmgType) override;

	UFUNCTION()
	void OnRep_State();

	UPROPERTY(ReplicatedUsing = OnRep_State)
	EGetawayBridgeState State = EGetawayBridgeState::Idle;

	/** Counts down CollapseDelay, then RespawnDelay. */
	float Timer = 0.f;
	float StandCheck = 0.f;
	/** Set by Park, cleared by StopFalling. Local to this machine's copy, never replicated. */
	bool bParked = false;

	/** Captured in BeginPlay. Location and rotation only - the catalog scales the actor *after*
	 *  BeginPlay, so a transform captured here would restore the wrong size. */
	FVector StartLocation = FVector::ZeroVector;
	FRotator StartRotation = FRotator::ZeroRotator;
};
