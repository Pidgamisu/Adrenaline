#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayC4Pickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class AGetawayCharacter;
class AGetawayC4Pickup;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGetawayPickupTakenSignature, AGetawayC4Pickup*, Pickup, AGetawayCharacter*, Taker);

/**
 * Restocks the player's C4. A thrown charge is spent for good (see AGetawayCharacter::StartingCharges), so this is
 * where C4 comes from: run through it and it hands over ChargesGranted charges, up to the player's MaxCharges.
 *
 * It never blocks movement - the trigger only overlaps pawns and the meshes have no collision, so you can sprint,
 * slide or wallrun straight through it. While depleted the floating brick and its light go away and only the pad is
 * left, then it comes back after RespawnDelay. Walking onto a pad while full takes nothing: it waits until you have
 * room, and a pickup that respawns under your feet is taken on the spot.
 *
 * Works with no assets (engine BasicShapes). Drop one in a level, or make a Blueprint child to restyle it.
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayC4Pickup : public AActor
{
	GENERATED_BODY()

public:
	AGetawayC4Pickup();

	/** Overlap volume. Pawns only; nothing else notices it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<USphereComponent> Trigger;

	/** Marker on the ground. Stays visible while the pickup is depleted so you know it will come back. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Pad;

	/** The floating charge. Hidden while depleted. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Charge;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UPointLightComponent> Light;

	/**
	 * Charges handed over, clamped by the player's MaxCharges. One each since 2026-10-19: a full
	 * load of two now costs two pickups, so carrying both is something you went out of your way
	 * for rather than the default state.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Pickup", meta = (ClampMin = "1"))
	int32 ChargesGranted = 1;

	/** Seconds before it comes back after being taken. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Pickup", meta = (ClampMin = "0"))
	float RespawnDelay = 8.f;

	/** Off means one use only: it stays depleted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Pickup")
	bool bRespawns = true;

	/** How far the charge bobs up and down (cm) and how fast it spins (degrees/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Pickup", meta = (ClampMin = "0"))
	float BobHeight = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Pickup")
	float SpinSpeed = 90.f;

	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayPickupTakenSignature OnTaken;

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsAvailable() const { return bAvailable; }

	/** Make it available again now, whatever the timer says. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void Respawn();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);

	/** Hands the charges over if this one can use them. */
	bool TryGive(AActor* Other);

	/** Catches a player who was full when they arrived, or who is standing here when it respawns. */
	void CheckWaitingPlayers();

	void SetAvailable(bool bNew);

	bool bAvailable = true;
	float Age = 0.f;
	float RecheckTimer = 0.f;
	float RespawnCountdown = 0.f;
	float ChargeBaseZ = 45.f;
};
