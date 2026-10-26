#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawaySpikes.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class AGetawaySpikes;
class AGetawayCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGetawaySpikedSignature, AGetawaySpikes*, Spikes,
	AGetawayCharacter*, Victim);

/**
 * The first trap: a bed of spikes that kills whoever touches it.
 *
 * It is **surface mounted** (`bSurfaceMounted` on its catalog entry), so it lies flat against the
 * wall, floor or ceiling you aim at with its points facing out, and cannot be placed in mid-air.
 * Same footprint as a wall - 400 x 400 x 50 as a plate - which makes it big enough to be a real
 * obstacle rather than something you only die to by bad luck.
 *
 * It never blocks you. The meshes have collision off and the trigger is QueryOnly, overlapping
 * pawns only, exactly like AGetawayFinish: running into spikes should kill you, not stop you dead
 * and then kill you. You can wallrun straight past a spiked wall if you keep off it.
 *
 * Works with no assets (engine BasicShapes).
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawaySpikes : public AActor
{
	GENERATED_BODY()

public:
	AGetawaySpikes();

	/** Overlap volume. Fills the piece; the meshes inside it are decoration. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UBoxComponent> Trigger;

	/** The plate the points stand on. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Base;

	/**
	 * Roughly how far apart the points sit, in world cm. The count comes from this and the plate's
	 * actual size, so a plate trimmed back to a ledge gets fewer points rather than squashed ones.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Spikes", meta = (ClampMin = "20"))
	float SpikeSpacing = 133.f;

	/** World size of one point, held constant however the plate is scaled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Spikes", meta = (ClampMin = "5"))
	float SpikeWidth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Spikes", meta = (ClampMin = "5"))
	float SpikeHeight = 35.f;

	virtual void OnConstruction(const FTransform& Transform) override;

	/** Armed only during a run. Off between rounds so the placement camera cannot set them off. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsArmed() const { return bArmed; }

	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void SetArmed(bool bNew) { bArmed = bNew; }

	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawaySpikedSignature OnSpiked;

	/**
	 * Stick these spikes to whatever they were placed on. Server only; the host replicates so every
	 * machine can hook itself up the same way.
	 *
	 * They are attached to it, so a host that moves - a collapsing bridge - carries them down and
	 * brings them back up with it. And if the host is a breakable, smashing the panel takes the
	 * spikes with it: you can clear a spiked wall by breaking the wall.
	 */
	void AttachToHost(AActor* InHost);

	UFUNCTION(BlueprintPure, Category = "Getaway")
	AActor* GetHost() const { return Host; }

	/** False while the host is broken: hidden, disarmed, and waiting for the host to come back. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsPresent() const { return bPresent; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	/** Place and size the points for whatever the plate has actually come out as. */
	void LayOutPoints();

	/** Hook up to the host's break and restore. Runs on every machine, off the replicated Host. */
	void BindToHost();
	void SetPresent(bool bNew);

	UFUNCTION()
	void OnHostBroken(class AGetawayBreakable* Breakable);

	UFUNCTION()
	void OnHostRestored(class AGetawayBreakable* Breakable);

	UFUNCTION()
	void OnRep_Host() { BindToHost(); }

	/** What they are stuck to. Null for spikes dropped straight onto the level's own geometry. */
	UPROPERTY(ReplicatedUsing = OnRep_Host)
	TObjectPtr<AActor> Host;

	bool bPresent = true;

	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	/** The points. Built in the constructor, so the count is fixed at class default time. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> Points;

	bool bArmed = true;
};
