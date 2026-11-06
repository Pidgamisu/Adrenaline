#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Character.h"
#include "GetawayCharacter.generated.h"

class UCameraComponent;
class UGetawayMovementComponent;
class UStaticMeshComponent;
class AGetawayC4;
class UMaterialInterface;
class UMaterialInstanceDynamic;
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

	/**
	 * The grapple rope: one cylinder, stretched between your hand and the hook every frame it is
	 * out, and hidden the rest of the time.
	 *
	 * Real geometry rather than a debug line, for the same reason the placement ghost and the C4
	 * blast are: `DrawDebugLine` is compiled out of a Shipping build, so a rope made of one would
	 * be invisible in the thing people actually download. Unlike the body it is *not* OwnerNoSee -
	 * your own rope is the main thing you are looking at.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UStaticMeshComponent> Rope;

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

	/**
	 * Extra degrees of FOV while a speed ring's boost is running, on top of what the speed itself
	 * already earns. This is the only thing that tells you the boost is live and, as it eases back
	 * out over about a sixth of a second, that it has gone - there is no HUD message for it.
	 *
	 * It has to be a flat bump rather than more speed FOV, because the speed FOV is relative to
	 * RunSpeed and a boost that is merely *held* (coasting at the new ceiling with no strafing)
	 * would otherwise look like a slightly fast jog. 0 turns it off; `bCameraEffects` off takes it
	 * with everything else.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look", meta = (ClampMin = "0", ClampMax = "30"))
	float BoostFOV = 9.f;

	/** Eye height above your feet (cm), standing and crouched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look")
	float StandingEyeHeight = 162.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look")
	float CrouchedEyeHeightAboveFeet = 95.f;

	/* ---------------- Look and move on a pad ---------------- */

	/**
	 * Turn rate at full stick, in degrees per second, before the response curve bites.
	 *
	 * This is a rate and not a sensitivity: a stick says how far it is held, so it has to be turned
	 * into degrees per second and multiplied by the frame time, where a mouse already reports a
	 * delta. Vertical is slower than horizontal on purpose, which is what nearly every pad shooter
	 * does - you need the full 180 sideways and barely 90 up and down.
	 *
	 * These are the same unit as Apex Legends' Advanced Look Controls "Yaw Speed" and "Pitch
	 * Speed", so a value from there can be typed in here directly. The defaults aim at Apex's
	 * 4 Linear - see docs/controller.md for where the numbers come from and how to check them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)", meta = (ClampMin = "10"))
	float StickLookYawSpeed = 230.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)", meta = (ClampMin = "10"))
	float StickLookPitchSpeed = 180.f;

	/**
	 * How much of the stick's travel is fine aiming. 1 is linear - the whole stick is one straight
	 * line from nothing to full rate, which is what Apex calls Linear and what its players use.
	 * Above 1, the inner half is slow and precise and the outer edge is fast.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)", meta = (ClampMin = "1", ClampMax = "4"))
	float StickLookExponent = 1.f;

	/**
	 * Radial dead zone for the look stick. The travel past it is rescaled back to the full range.
	 * Small on purpose: a linear curve with a wide dead zone throws away the slow end of the stick,
	 * which is the part a linear player is aiming with. Raise it if your sticks drift at rest.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)", meta = (ClampMin = "0", ClampMax = "0.5"))
	float StickLookDeadZone = 0.05f;

	/**
	 * Turn spin-up, in seconds: a flick starts at StickLookRampStartScale of the full rate and
	 * reaches it after this long, so small corrections do not snap. 0 turns it off, which is what
	 * Apex's Linear players do with its Turning Extra Yaw and ramp-up settings - a ramp means the
	 * same stick position turns you at different rates depending on how long you have held it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)", meta = (ClampMin = "0", ClampMax = "1"))
	float StickLookRampTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)", meta = (ClampMin = "0.05", ClampMax = "1"))
	float StickLookRampStartScale = 0.5f;

	/** Separate from bInvertMouseY, because wanting one inverted says nothing about the other. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)")
	bool bInvertStickY = false;

	/** Radial dead zone for the move stick. Wider than the look stick's: walking slightly when you
	 *  meant to stand still matters less than an aim that creeps, and a movement game wants the
	 *  stick to read as "not pushed" the moment you let go. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Move (Pad)", meta = (ClampMin = "0", ClampMax = "0.5"))
	float MoveStickDeadZone = 0.12f;

	/**
	 * Push the stick past this and it counts as pushed all the way.
	 *
	 * Source air acceleration grows with how far sideways your input points, so a stick held most
	 * of the way air strafes worse than A or D, which are always 100%. This hands the outer fifth
	 * of the stick back. Set it to 1 for honest analogue input.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Move (Pad)", meta = (ClampMin = "0.5", ClampMax = "1"))
	float MoveStickFullTilt = 0.9f;

	/** True while the last thing you touched was the pad, so the HUD can name the right buttons. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsUsingGamepad() const { return bUsingGamepad; }

	/* ---------------- What you are carrying ---------------- */

	/**
	 * How many things you may hold at once, C4 charges and grapple hooks counted together.
	 *
	 * One, and that is the rule rather than a tuning value: your hand holds a single item, and no
	 * pickup will hand you another until you have spent the one you are holding. A C4 pad and a
	 * grapple pad in the same stretch of course is therefore a choice between them, not a shopping
	 * trip - and a hook in hand is a reason to go and use it.
	 *
	 * It replaces the old "carry up to two charges of C4" (decided 2026-10-15, retired 2026-11-05
	 * when the hook became a pickup too). Set this to 2 and raise `MaxCharges` back to 2 for the
	 * old behaviour; the hand is a cap across both items and never a floor, so `MaxCharges` still
	 * limits C4 on its own.
	 *
	 * Throwing counts as spending. A charge stuck to a wall waiting for F is out of your hand, so
	 * you may pick up a hook while it sits there.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Items", meta = (ClampMin = "1"))
	int32 ItemCapacity = 1;

	/** Everything in your hand: C4 charges plus grapple hooks. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Items")
	int32 GetItemsHeld() const { return GetChargesLeft() + GetGrappleShotsLeft(); }

	/** True while a pickup of any kind would give you something. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Items")
	bool HasRoomForItem() const { return GetItemsHeld() < FMath::Max(1, ItemCapacity); }

	/* ---------------- C4 ---------------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|C4")
	TSubclassOf<AGetawayC4> C4Class;

	/**
	 * How many charges of C4 you can carry at once. One since 2026-11-05: your hand holds a single
	 * item, and `ItemCapacity` caps the total anyway - this is the C4-only half of that.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|C4", meta = (ClampMin = "1"))
	int32 MaxCharges = 1;

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

	/**
	 * True while there is room for more charges, i.e. while a C4 pickup would do something.
	 *
	 * Both caps have to pass: `MaxCharges` for C4 itself, and `ItemCapacity` for your hand as a
	 * whole - so holding a grapple hook closes the C4 pads as well, which is the point of the hand.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway|C4")
	bool HasRoomForCharges() const { return ChargesHeld < MaxCharges && HasRoomForItem(); }

	/** Hand over charges, up to MaxCharges. Returns how many were actually taken, 0 if already full. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|C4")
	int32 AddCharges(int32 Count);

	/* ---------------- Grapple ---------------- */

	/*
	 * The grappling hook. The character aims it, spends the charge and draws the rope; the pull
	 * itself is UGetawayMovementComponent's, because the pull is movement.
	 *
	 * You only have one at all on a Grapple round - GrappleShots is 0 otherwise - and the shot is
	 * spent when the hook *catches*, so a click at open sky costs nothing.
	 */

	/** How far the hook reaches (cm). 3500 is 35 m, about four of the 1000 cm bridge end to end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple", meta = (ClampMin = "100"))
	float GrappleRange = 3500.f;

	/** How thick the rope draws (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple", meta = (ClampMin = "0.5"))
	float RopeThickness = 5.f;

	/**
	 * The rope's colour. Cyan by the game's own rule: pink is architecture, cyan is the things that
	 * help you, lime kills you. See *The colour rule* in docs/look.md.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple")
	FLinearColor RopeColour = FLinearColor(0.40f, 1.f, 0.95f);

	/** Multiplies the lot; above 1 is what blooms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple", meta = (ClampMin = "0"))
	float RopeBrightness = 1.8f;

	/** Where the rope leaves you, measured from the camera in its own axes: forward, right, up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple")
	FVector RopeHandOffset = FVector(30.f, 20.f, -22.f);

	/**
	 * What the rope is made of. Additive and unlit like everything else in this world, so the line
	 * glows rather than being shaded - and a dynamic instance of it is tinted RopeColour.
	 *
	 * A soft reference resolved on the first frame a rope is drawn, not a constructor
	 * `FObjectFinder`: the same reasoning as `AGetawayBlast::BlastMaterial`, which would otherwise
	 * warn on every editor start until `adrenaline_look.py` has been run.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple")
	TSoftObjectPtr<UMaterialInterface> RopeMaterial =
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Materials/M_VB_Pane.M_VB_Pane")));

	/**
	 * Left click: hook whatever you are looking at, inside GrappleRange.
	 *
	 * Spends the shot only if something is hit. Does nothing at all on a round that did not hand
	 * you a hook, which is why a spare mouse button is safe to put this on.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Grapple")
	void FireGrapple();

	/** Left click again: let go. Your speed is yours to keep, and the shot does not come back. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Grapple")
	void ReleaseGrapple();

	/** True while the rope is attached, on this machine or (for a rival) as last replicated. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Grapple")
	bool IsGrappling() const;

	/** True while there is a hook in your hand, so the HUD can name the key for it. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Grapple")
	bool HasGrappleHook() const { return GrappleShots > 0; }

	/** Hooks in hand. 1 from a pickup or a Grapple round, 0 once you have fired it. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Grapple")
	int32 GetGrappleShotsLeft() const { return FMath::Clamp(GrappleShots, 0, MaxGrappleShots); }

	/** How many hooks you can carry at once. `ItemCapacity` caps the hand as a whole on top of it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple", meta = (ClampMin = "1"))
	int32 MaxGrappleShots = 1;

	/** True while a grapple pickup would give you something: room in the hand, and room for a hook. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Grapple")
	bool HasRoomForGrappleShots() const { return GrappleShots < MaxGrappleShots && HasRoomForItem(); }

	/**
	 * Hand over hooks, up to MaxGrappleShots and up to what the hand has room for. Returns how many
	 * were actually taken, 0 if there was no room - the same shape as AddCharges, and read the same
	 * way by AGetawayGrapplePickup.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Grapple")
	int32 AddGrappleShots(int32 Count);

	/* ---------------- Round effects ---------------- */

	/*
	 * What each of the round's effects is worth. *Which* effect is on is chosen by the game mode and
	 * replicated on AGetawayGameState; these say what it means, and they live here because the
	 * character is the thing that applies it - on the machine that owns it, like the rest of the
	 * movement. A fresh character is spawned every round and on every respawn, so an effect is gone
	 * the moment its round is, and two of them can never stack.
	 */

	/**
	 * Air jumps on a Double Jump round. A floor rather than an addition: the tuned default for
	 * AirJumps is 0, so 1 here is one jump in the air.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Effects", meta = (ClampMin = "0"))
	int32 EffectAirJumps = 1;

	/** Charges you start a Loaded round carrying. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Effects", meta = (ClampMin = "0"))
	int32 EffectC4Charges = 1;

	/** How many you can hold on a Loaded round, so a pickup tops you back up and no further. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Effects", meta = (ClampMin = "1"))
	int32 EffectC4MaxCharges = 1;

	/** Gravity on a Low Gravity round, as a fraction of the tuned 2.04. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Effects", meta = (ClampMin = "0.05", ClampMax = "1"))
	float EffectGravityScale = 0.6f;

	/**
	 * Hooks you get on a Grapple round. One, deliberately: a single hook is a decision about where
	 * in the course you spend it, and two would just be a way of travelling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Effects", meta = (ClampMin = "0"))
	int32 EffectGrappleShots = 1;

	/** Give this character whatever this round's effect is. Called from BeginPlay. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Effects")
	void ApplyRoundEffect();

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
	 * Killed by a trap, or by falling. Call it on the machine that owns this character: it stops
	 * you where you stand and tells the server, which credits whoever placed the thing and then
	 * puts you back at the start - or ends your run for the round, if the game mode's
	 * `bRespawnOnDeath` is off. Safe to call twice; the second one does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void Die(AActor* KilledBy);

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsDead() const { return bDead; }

	/**
	 * Whether dying puts you back at the start - the game mode's `bRespawnOnDeath`. Works on a
	 * client, which has no game mode of its own: it reads the replicated game mode class default.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool WillRespawnOnDeath() const;

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

	/**
	 * Say something to the player holding this character, through AGetawayHUD::ShowMessage. Does
	 * nothing for a character we do not control, which is what keeps a listen-server host from
	 * drawing another player's pickups and deaths on its own screen.
	 *
	 * These lines used to go to GEngine->AddOnScreenDebugMessage, which is compiled out of Shipping
	 * and Test builds - see ShowMessage for the detail.
	 */
	void Notify(const FString& Text, float Seconds, const FLinearColor& Colour, int32 Key) const;

#if !UE_BUILD_SHIPPING
	/**
	 * On-screen readout of what the remote players are doing. Still a debug message rather than a
	 * HUD line, and deliberately so: it is spike scaffolding keyed on the actor, and what the game
	 * actually wants here one day is a nameplate over each player, not a list in the corner.
	 */
	void ShowRemoteState() const;
#endif

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
	void OnMoveStick(const FInputActionValue& Value);
	/** Shared by both of the above: a 2D wish direction, turned into world movement. */
	void ApplyMoveInput(const FVector2D& Axis);
	void OnLook(const FInputActionValue& Value);
	void OnLookStick(const FInputActionValue& Value);
	void OnLookStickReleased(const FInputActionValue& Value);
	void OnGamepadUsed(const FInputActionValue& Value);
	void OnJumpPressed(const FInputActionValue& Value);
	void OnJumpReleased(const FInputActionValue& Value);
	void OnJumpTap(const FInputActionValue& Value);
	void OnCrouchPressed(const FInputActionValue& Value);
	void OnCrouchReleased(const FInputActionValue& Value);
	void OnThrow(const FInputActionValue& Value);
	void OnDetonate(const FInputActionValue& Value);
	/** One button for both halves of the hook: throw it if it is in, let go if it is out. */
	void OnGrapple(const FInputActionValue& Value);
	void OnRestart(const FInputActionValue& Value);
	void OnBackToPlacement(const FInputActionValue& Value);

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveStickAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookStickAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> GamepadProbeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpTapAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ThrowAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> DetonateAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> GrappleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RestartAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BackToPlacementAction;

	/** Charges in the air or stuck to something, waiting for F. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<AGetawayC4>> Charges;

	/** Unthrown charges in hand. */
	UPROPERTY(Transient)
	int32 ChargesHeld = 0;

	/* ---------------- Grapple ---------------- */

	/** Hooks in hand. One item at a time, so this and ChargesHeld are never both positive. */
	int32 GrappleShots = 0;

	/** Stretch the rope between hand and hook, or hide it. Every machine, every character. */
	void UpdateRope();

	/** The tinted dynamic instance, built on the first frame a rope is actually drawn. */
	bool EnsureRopeMaterial();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RopeDynamic;

	/**
	 * The rope as everybody else sees it. Cosmetic and nothing more: the pull stays on the machine
	 * that owns the character, like the rest of the movement, and these two only exist so a rival's
	 * hook is something you can watch rather than guess at.
	 *
	 * Pushed by the owning client through ServerSetGrapple whenever the rope goes out or comes in -
	 * including when it ends itself, by arriving or being cut - and skipped for the owner, who is
	 * the one doing the telling.
	 */
	UPROPERTY(Replicated)
	bool bRemoteGrappling = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize100 RemoteGrappleAnchor = FVector::ZeroVector;

	UFUNCTION(Server, Unreliable)
	void ServerSetGrapple(bool bOn, FVector_NetQuantize100 Anchor);

	/** What we last told the server, so the push happens on change rather than every frame. */
	bool bSentGrappling = false;

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

	/** How long the look stick has been off centre, for the turn spin-up. */
	float StickLookHeld = 0.f;

	/** Purely for the HUD's hint lines. Local to this machine; nothing about it replicates. */
	bool bUsingGamepad = false;
};
