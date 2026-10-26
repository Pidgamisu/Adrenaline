#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayBreakable.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;
class AGetawayBreakable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGetawayBrokenSignature, AGetawayBreakable*, Breakable);

/**
 * A panel or crate that bursts into loose physics scraps when the player hits it fast enough
 * (see UGetawayMovementComponent::SmashSpeed) or a C4 blast reaches it. Breakables stacked on top go too.
 * Walls and floors should just be ordinary static meshes.
 *
 * Scale the actor to size it. Defaults to the engine cube (100 cm); set a material on the mesh.
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayBreakable : public AActor
{
	GENERATED_BODY()

public:
	AGetawayBreakable();

	/** Component tag on every scrap, used by the movement component to push them around. */
	static const FName ScrapTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** Rough size of each scrap (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Break", meta = (ClampMin = "5"))
	float PieceSize = 50.f;

	/** Most scraps one break can make. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Break", meta = (ClampMin = "1"))
	int32 MaxPieces = 48;

	/** Scraps are removed after this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Break", meta = (ClampMin = "1"))
	float ScrapLifetime = 30.f;

	/** Delay before breakables resting on top of this one break too. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Break", meta = (ClampMin = "0"))
	float ChainDelay = 0.08f;

	/** Optional material for the scraps. Uses the mesh's material if empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Break")
	TObjectPtr<UMaterialInterface> ScrapMaterial;

	/** How close the viewer has to be to feel the break. 0 shakes them wherever they are. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Break", meta = (ClampMin = "0"))
	float ShakeRadius = 1500.f;

	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayBrokenSignature OnBroken;

	/**
	 * Fired when it comes back, on every machine, so anything that reacted to OnBroken can undo
	 * itself without having to be told separately. Spikes stuck to a panel use the pair.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayBrokenSignature OnRestored;

	/**
	 * Burst into scraps. Velocity is in cm/s and is passed on to the scraps.
	 *
	 * Safe to call from any machine. A client breaks it locally straight away - a smash at 900 cm/s
	 * cannot wait for a round trip - and tells the server, which repeats it to everybody else. The
	 * server breaks it everywhere at once. Either way every machine ends up running BreakNow.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void Break(FVector Velocity);

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsBroken() const { return bBroken; }

	/**
	 * Put it back as it was: scraps cleared, mesh solid and visible again. The game mode calls this
	 * when a round starts, which is what stops the course wearing away run after run.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void Restore();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void ClearScraps();

	/** The actual work, run on every machine. Everything above is about getting it there. */
	void BreakNow(FVector Velocity);
	void RestoreNow();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastBreak(FVector_NetQuantize100 Velocity);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRestore();

	/**
	 * Catches a client that was not here when it happened: the multicasts only reach whoever is
	 * connected at the time, so somebody joining later needs the state rather than the event. They
	 * get the panel hidden or solid again, without the scrap shower for a break they missed.
	 */
	UFUNCTION()
	void OnRep_Broken();

	/** The loose pieces, so they can be cleared without taking the actor with them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<class UStaticMeshComponent>> Scraps;

	FTimerHandle ScrapTimer;

	UPROPERTY(ReplicatedUsing = OnRep_Broken)
	bool bBroken = false;
};
