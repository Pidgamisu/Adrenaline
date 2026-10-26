#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Character.h"
#include "GetawayCharacter.generated.h"

class UCameraComponent;
class UGetawayMovementComponent;
class UStaticMeshComponent;
class AGetawayC4;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * What a remote player is doing, in one byte. This is the whole of the custom movement state that
 * crosses the wire: the wall normal, the air-jump count and the mantle interpolation stay on the
 * machine that owns the character, because that machine is the authority on its own movement.
 */
UENUM(BlueprintType)
enum class EGetawayNetMove : uint8
{
	Ground,
	Crouch,
	Slide,
	Air,
	WallRun,
	Mantle,
};

/**
 * One client-authoritative movement update. Sent unreliably by the owning client at NetSendRate,
 * applied by the server to its own copy of the character, then replicated on to everyone else's
 * simulated proxies, which interpolate toward it.
 *
 * There is deliberately no input, no timestamp and no saved-move queue: nothing here is replayed or
 * predicted, so UGetawayMovementComponent never has to be taught how to rewind itself.
 */
USTRUCT()
struct FGetawayNetMove
{
	GENERATED_BODY()

	UPROPERTY() FVector_NetQuantize100 Location = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantize10 Velocity = FVector::ZeroVector;
	UPROPERTY() float Yaw = 0.f;
	UPROPERTY() float Pitch = 0.f;
	UPROPERTY() uint8 State = 0;
};

/**
 * First-person player for the Getaway movement. Works with no assets: input actions and key bindings are created
 * in code (WASD, mouse, Space / wheel jump, C / Shift / Ctrl crouch, G throw C4, F detonate, R restart).
 * Make a Blueprint child to tweak values in the Details panel, or set them here.
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AGetawayCharacter(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UCameraComponent> Camera;

	/**
	 * Something for the other players to look at: a cylinder body and a nose block, hidden from the
	 * owner so first person stays first person. Spike scaffolding - the real game wants a skeletal
	 * mesh here, and this plugin ships no content.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Nose;

	UFUNCTION(BlueprintPure, Category = "Getaway")
	UGetawayMovementComponent* GetGetawayMovement() const;

	/* ---------------- Networking (client-authoritative) ---------------- */

	/** How many times a second the owning client tells the server where it is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Net", meta = (ClampMin = "5", ClampMax = "60"))
	float NetSendRate = 25.f;

	/** How hard a remote player is pulled toward the last state received. Higher is tighter and jerkier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Net", meta = (ClampMin = "1"))
	float ProxyInterpSpeed = 18.f;

	/** Seconds of dead reckoning allowed between updates, so a remote player keeps moving between packets. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Net", meta = (ClampMin = "0", ClampMax = "0.5"))
	float ProxyExtrapolationLimit = 0.2f;

	/** Further than this from where it should be and a remote player is teleported, not interpolated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Net", meta = (ClampMin = "100"))
	float ProxySnapDistance = 800.f;

	/** The server moves its own copy of a remote character to the state that client reported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Net")
	bool bServerFollowsClient = true;

	/** Print each remote player's movement state on screen, so a two-player test can be read at a glance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Net")
	bool bShowRemoteState = true;

	/** What this character is doing: from the movement component if it is ours, off the wire if it is not. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Net")
	EGetawayNetMove GetNetMoveState() const;

	/* ---------------- Look ---------------- */

	/** Source-style sensitivity: degrees per mouse count = Sensitivity x 0.022 (same as CS / the lab). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look", meta = (ClampMin = "0.01"))
	float MouseSensitivity = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look")
	bool bInvertMouseY = false;

	/**
	 * The mouse sensitivity in Project Settings > Engine > Input > Axis Config (MouseX / MouseY), 0.07 by default.
	 * It gets divided back out so MouseSensitivity means the same as in Source. Change this only if you changed that setting
	 * (or if turning feels far too fast or slow).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look", meta = (ClampMin = "0.0001"))
	float MouseInputScale = 0.07f;

	/** Horizontal field of view in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look", meta = (ClampMin = "60", ClampMax = "140"))
	float BaseFOV = 100.f;

	/** Wallrun tilt, speed FOV, shake, landing kick, mantle jiggle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look")
	bool bCameraEffects = true;

	/** Eye height above your feet (cm), standing and crouched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look")
	float StandingEyeHeight = 162.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look")
	float CrouchedEyeHeightAboveFeet = 95.f;

	/* ---------------- C4 ---------------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|C4")
	TSubclassOf<AGetawayC4> C4Class;

	/** How many charges you can carry at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|C4", meta = (ClampMin = "1"))
	int32 MaxCharges = 2;

	/**
	 * Charges you spawn with. C4 is a collectable resource: a thrown charge is spent for good, so you restock from an
	 * AGetawayC4Pickup. Set this to MaxCharges to always start with a full load.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|C4", meta = (ClampMin = "0"))
	int32 StartingCharges = 0;

	/** Throw speed (cm/s), added on top of your own velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|C4", meta = (ClampMin = "0"))
	float C4ThrowSpeed = 1500.f;

	/** How much of your velocity the charge inherits. 1 = all of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|C4", meta = (ClampMin = "0", ClampMax = "1.5"))
	float C4MomentumCarried = 1.f;

	/** Launch speed at point blank (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|C4", meta = (ClampMin = "0"))
	float C4BlastBoost = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|C4", meta = (ClampMin = "130"))
	float C4BlastRadius = 600.f;

	UFUNCTION(BlueprintCallable, Category = "Getaway|C4")
	void ThrowC4();

	UFUNCTION(BlueprintCallable, Category = "Getaway|C4")
	void DetonateC4();

	/** Charges in hand, ready to throw. */
	UFUNCTION(BlueprintPure, Category = "Getaway|C4")
	int32 GetChargesLeft() const;

	/** True while there is room for more charges, i.e. while a pickup would do something. */
	UFUNCTION(BlueprintPure, Category = "Getaway|C4")
	bool HasRoomForCharges() const { return ChargesHeld < MaxCharges; }

	/** Hand over charges, up to MaxCharges. Returns how many were actually taken, 0 if already full. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|C4")
	int32 AddCharges(int32 Count);

	/* ---------------- Camera effects ---------------- */

	/** Dip the view by this many degrees (negative kicks it up); it settles back quickly. */
	void AddViewPunch(float Degrees);
	/** 0..1 camera shake that fades out. */
	void AddCameraShake(float Amount);

	/**
	 * A client telling the server it smashed something, so the server can break it for everybody.
	 * It lives here rather than on the breakable because a server RPC is only routed from an actor
	 * the client owns, and nobody owns a panel sitting in the level. AGetawayBreakable::Break sends
	 * it; there is no reason to call it from anywhere else.
	 */
	UFUNCTION(Server, Reliable)
	void ServerReportBreak(class AGetawayBreakable* Breakable, FVector_NetQuantize100 Velocity);

	/**
	 * Killed by a trap. Call it on the machine that owns this character: it stops you where you
	 * stand and tells the server, which ends your round and credits whoever placed the thing.
	 * Dying does not respawn you - your run is over until the next round, as in Ultimate Chicken
	 * Horse. Safe to call twice; the second one does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void Die(AActor* KilledBy);

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsDead() const { return bDead; }

	/**
	 * Fall below this and you die - measured at your feet, in world Z, so -100 is one metre under
	 * a course whose floor sits at zero. This is what makes a missed jump cost you the round
	 * rather than a walk back, and it is deliberately well above the engine KillZ: a plane down
	 * at the bottom of the world only catches you after a long, dull drop.
	 *
	 * Raise it to make a course less forgiving, or take it down past everything the level has (the
	 * test course has a catch floor at -300 with stairs out of every pit) to go back to falling
	 * being survivable. FellOutOfWorld is still the backstop if you get past this somehow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Death")
	float FallDeathZ = -100.f;

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void FellOutOfWorld(const class UDamageType& DmgType) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	/** Ticked on the owning machine: below FallDeathZ your run is over. */
	void CheckFallDeath();

	void BuildDefaultInput();
	void UpdateCamera(float DeltaSeconds);

#if !UE_BUILD_SHIPPING
	/** -GetawayFinishAfter=<seconds>: a headless client has no hands, so it jumps to the objective. */
	FTimerHandle NetTestFinishTimer;
#endif

	/* ---------------- Networking ---------------- */

	/** Where we are and what we are doing, right now, as it goes on the wire. */
	FGetawayNetMove SampleMove() const;
	/** Owning client: push SampleMove() at NetSendRate. Listen-server host: publish it for the others. */
	void SendNetMove();
	/** A character someone else owns: slide toward the last state received. */
	void ApplyProxyMove(float DeltaSeconds);
	/** Stop the movement component simulating a character we do not own, and start it again if we turn out to. */
	void SetProxySimulation(bool bProxy);
	/** Squash the body when the state byte says crouched or sliding. */
	void UpdateBody();
	/** On-screen readout of what the remote players are doing. */
	void ShowRemoteState() const;

	UFUNCTION(Server, Unreliable)
	void ServerUpdateMove(FGetawayNetMove Move);

	UFUNCTION(Server, Reliable)
	void ServerBackToPlacement();

	/**
	 * Only the server spawns a thrown charge, so there is one brick and everybody sees the same
	 * one. The client sends where it left the hand and how fast, because only it knows where it was
	 * aiming. It is not checked against the server's idea of how many charges you hold - that count
	 * is yours, like the rest of client authority.
	 */
	UFUNCTION(Server, Reliable)
	void ServerThrowC4(FVector_NetQuantize100 Start, FRotator View, FVector_NetQuantize100 LaunchVelocity);

	UFUNCTION(Server, Reliable)
	void ServerDetonateC4();

	UFUNCTION(Server, Reliable)
	void ServerReportDeath(AActor* KilledBy);

	/** Set by Die on the machine that owns this character; stops it dying twice. */
	bool bDead = false;

	/** Spawn and launch one charge. Authority only; both paths above end up here. */
	class AGetawayC4* SpawnThrownC4(const FVector& Start, const FRotator& View, const FVector& LaunchVelocity);

	UFUNCTION()
	void OnRep_NetMove();

	void OnMove(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJumpPressed(const FInputActionValue& Value);
	void OnJumpReleased(const FInputActionValue& Value);
	void OnJumpTap(const FInputActionValue& Value);
	void OnCrouchPressed(const FInputActionValue& Value);
	void OnCrouchReleased(const FInputActionValue& Value);
	void OnThrow(const FInputActionValue& Value);
	void OnDetonate(const FInputActionValue& Value);
	void OnRestart(const FInputActionValue& Value);
	void OnBackToPlacement(const FInputActionValue& Value);

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpTapAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ThrowAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DetonateAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RestartAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BackToPlacementAction;

	/** Charges in the air or stuck to something, waiting for F. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGetawayC4>> Charges;

	/** Unthrown charges in hand. */
	UPROPERTY(Transient)
	int32 ChargesHeld = 0;

	/** The last state this character's owner reported. Only simulated proxies are told about it. */
	UPROPERTY(ReplicatedUsing = OnRep_NetMove)
	FGetawayNetMove NetMove;

	float NextSendTime = 0.f;
	float LastNetMoveTime = -1.f;
	bool bProxySimulationOff = false;

	float EyeAboveFeet = 162.f;
	float Roll = 0.f;
	float CurrentFOV = 100.f;
	float Punch = 0.f;
	float Shake = 0.f;
	float SeenDoubleJumpTime = -100.f;
};
