#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GetawayFinish.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class AGetawayCharacter;
class AGetawayFinish;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGetawayFinishedSignature, AGetawayFinish*, Finish, AGetawayCharacter*, Runner);

/**
 * The objective. Run into it and the round ends: AGetawayGameMode stops the clock, shows your time
 * and drops you back into the placement phase to build the next one.
 *
 * It never blocks movement - the trigger overlaps pawns only and the meshes have no collision, so
 * you can slide, wallrun or rocket-jump straight through it at speed without being slowed or
 * snagged. That matters: an endpoint you can overshoot because it stopped you dead would be worse
 * than no endpoint at all.
 *
 * It only fires once per run; the game mode arms it again when the next run starts.
 *
 * Works with no assets (engine BasicShapes). Drop one in a level and it is the goal.
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayFinish : public AActor
{
	GENERATED_BODY()

public:
	AGetawayFinish();

	/** Overlap volume. Pawns only. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UBoxComponent> Trigger;

	/** Pad on the ground marking the spot. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Pad;

	/** The post, so it reads from across the level. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Post;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UPointLightComponent> Light;

	/** Full extent of the trigger in cm. Generous by default: you arrive here fast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Finish")
	FVector TriggerSize = FVector(300.f, 300.f, 400.f);

	/** Fired when someone reaches it. The game mode listens; hook it for scoring later. */
	UPROPERTY(BlueprintAssignable, Category = "Getaway")
	FGetawayFinishedSignature OnFinished;

	/** Show it again. Called by the game mode at the start of each run. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void Arm();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& Sweep);

	void Reach(AGetawayCharacter* Runner);

	float Spin = 0.f;
};
