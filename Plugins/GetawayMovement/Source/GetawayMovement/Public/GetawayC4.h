#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayC4.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * A thrown charge. Flies on its own simple ballistic path, sticks to whatever it hits (and falls again if that thing
 * breaks), blinks while armed, and on Detonate():
 *  - launches its owner away from the blast, rocket-jump style (strongest within ~1.2 m),
 *  - bursts every AGetawayBreakable in the radius,
 *  - shoves loose physics objects.
 * It never damages walls or floors.
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayC4 : public AActor
{
	GENERATED_BODY()

public:
	AGetawayC4();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UPointLightComponent> Light;

	/** Optional explosion effect. With none set, a debug sphere flashes instead (editor and development builds only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Effects")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Effects")
	TObjectPtr<USoundBase> ExplosionSound;

	/** Called by the thrower right after spawning, on the server. Speeds in cm/s, radius in cm. */
	void Launch(const FVector& InVelocity, float InBlastRadius, float InBlastBoost, float InGravityScale);

	/**
	 * Set it off. Server only - a client asks through AGetawayCharacter::ServerDetonateC4. The
	 * server breaks things and shoves physics, and multicasts the bang so every machine can play it
	 * and launch its own player.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void Detonate();

	/**
	 * A client throwing itself before it asks. A rocket jump is a precision movement tech and
	 * cannot wait a round trip to be pushed, so the owning client applies its own launch the
	 * instant it presses the key; the multicast then skips it, having been beaten to it.
	 */
	void PredictLocalBlast();

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsStuck() const { return bStuck; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	void Stick(const FHitResult& Hit);
	void Unstick();

	/** Breakables and loose physics. Authority only: the break replicates itself from there. */
	void ApplyWorldBlast(const FVector& P);
	/** Flash and noise. Runs on every machine. */
	void PlayBlastEffects(const FVector& P);
	/**
	 * Push this machine's own player away from the blast, if they are close enough. Every machine
	 * launches only its own character, because under client authority that is the only one it is
	 * allowed to move - which between them covers everybody.
	 */
	void LaunchLocalPlayer(const FVector& P);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastDetonate(FVector_NetQuantize100 P);

	FVector Velocity = FVector::ZeroVector;

	/** Replicated because a client needs them for its own predicted launch, and they are set once. */
	UPROPERTY(Replicated)
	float BlastRadius = 600.f;

	UPROPERTY(Replicated)
	float BlastBoost = 1500.f;

	/** This machine has already pushed its player, so the multicast must not do it twice. */
	bool bLocalBlastApplied = false;
	float GravityScale = 2000.f / 980.f;
	float Age = 0.f;
	bool bStuck = false;
	TWeakObjectPtr<UPrimitiveComponent> StuckTo;
};
