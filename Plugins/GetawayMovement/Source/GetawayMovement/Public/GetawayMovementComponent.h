#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GetawayMovementComponent.generated.h"

class UAudioComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EGetawayCustomMove : uint8
{
	None    UMETA(Hidden),
	WallRun UMETA(DisplayName = "Wall Run"),
	Mantle  UMETA(DisplayName = "Mantle"),
};

/**
 * A ledge the mantle probe found: what it is, and where a climb would put you.
 *
 * Plain struct, not a USTRUCT: it lives for the length of one test and never leaves the component.
 */
struct FGetawayMantleFit
{
	/** The face you would climb, and the thing you would stand on. Usually the same actor. */
	const AActor* Wall = nullptr;
	const AActor* Ledge = nullptr;
	/** Flat direction into the wall, i.e. the way you go over the top. */
	FVector Dir = FVector::ZeroVector;
	/** Where the capsule ends up, X and Y only. */
	FVector LandXY = FVector::ZeroVector;
	/** World Z of the surface up there, and how much a slope lifts the landing capsule. */
	float Top = 0.f;
	float SlopeLift = 0.f;
	/** False when there is only room to crouch on top, so the climb has to duck. */
	bool bStandFits = false;
};

/**
 * Source-engine style movement for a first-person character, ported from the Getaway Movement Lab prototype.
 *
 * - Ground: Source friction (sv_friction / sv_stopspeed) and acceleration (sv_accelerate).
 * - Air: Source air acceleration with a tiny wish-speed cap, so air strafing gains speed and bunny hops keep it.
 * - Slide: crouch at speed for a boost and very low friction; slopes speed you up. A slide can't
 *   start again until SlideCooldown seconds after the last one ended, so it's a committed choice
 *   rather than something you tap over and over.
 * - Wallrun: a wall beside you (not in front) catches you; reduced gravity that ramps up late; Space kicks off.
 * - Mantle: jump into a ledge holding forward; you keep most of your speed going over the top.
 * - Grapple: a hook the round effect hands you. Not a movement mode - a pull added to whatever you
 *   are already doing, so you keep air strafing and steer it with the camera.
 * - Double jump, auto-hop, jump buffering, smashing through AGetawayBreakable at speed, pushing loose scraps.
 *
 * All distances are centimetres. The lab displayed "u/s" where 1 u = 2.5 cm, so 320 u/s = 800 cm/s.
 * Single-player only: there are no custom saved moves, so this is not network-predicted.
 */
UCLASS(ClassGroup = (Getaway), meta = (BlueprintSpawnableComponent))
class GETAWAYMOVEMENT_API UGetawayMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UGetawayMovementComponent();

	/* ---------------- Ground ---------------- */

	/** Top running speed (cm/s). Lab default 320 u/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground", meta = (ClampMin = "0"))
	float RunSpeed = 800.f;

	/** sv_accelerate: how fast you reach RunSpeed on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground", meta = (ClampMin = "0"))
	float GroundAccelerate = 10.f;

	/** sv_friction: how fast you slow down on the ground with no input. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground", meta = (ClampMin = "0"))
	float GroundFrictionSource = 5.f;

	/** sv_stopspeed (cm/s): below this, friction acts as if you were going this fast, so stops are crisp. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground", meta = (ClampMin = "0"))
	float StopSpeed = 250.f;

	/**
	 * **Can turning the camera while you run make you faster?** Off by default since 2026-11-06,
	 * which fixes a real bug rather than changing a feel.
	 *
	 * The ground cap is on your speed *projected onto the direction you are asking for*, never on
	 * your actual speed, so holding forward and turning keeps that projection under `RunSpeed`
	 * while your real speed climbs. The allowance is the full 800 (the air's is clamped to 35), so
	 * the push is 66.7 cm/s a tick, and the component of it along your own momentum outruns
	 * friction. It settles at about **1030 cm/s from nothing but walking and turning** - which made
	 * `RunSpeed` not a ceiling at all, and any smash threshold meaningless, since you were over it
	 * constantly.
	 *
	 * Off, acceleration may still steer you freely but cannot raise your speed past what you are
	 * asking for, so walking and turning tops out at 800. It is faithful Source behaviour either
	 * way - the same formulas with Source's own constants give a 1.30x overspeed against our 1.29x
	 * - so this is inherited, not a porting slip. It is simply far easier to exploit with a mouse
	 * here than it ever was there.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground")
	bool bGroundStrafeGain = false;

	/** Crouch-walk speed as a fraction of RunSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Ground", meta = (ClampMin = "0", ClampMax = "1"))
	float CrouchSpeedScale = 0.34f;

	/* ---------------- Air ---------------- */

	/** sv_airaccelerate: air control and how much speed air strafing can gain. 100+ feels like a surf server. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	float AirAccelerate = 12.f;

	/**
	 * Air wish-speed cap (cm/s). Source uses 30 u. Keep it small: this is what makes strafing work.
	 *
	 * Only read while `bAirStrafeGain` is on. With gain off the cap is what was *holding air control
	 * back*, so redirect-only mode ignores it and uses the full wish speed instead - see
	 * `AirControlAccelerate`.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	float AirSpeedCap = 75.f;

	/**
	 * **Can air input make you faster, or only point you somewhere else?**
	 *
	 * Off (the default since 2026-11-06): air input **steers your momentum without lengthening
	 * it**. You can still accelerate up to the wish speed from slower than it - a standing jump
	 * still gets you moving - but strafing can never take you past `RunSpeed`, so a bunny hop
	 * preserves speed instead of building it. Top speed then comes only from the pieces: jump pads,
	 * speed rings, slides down slopes, the C4 jump, the grapple.
	 *
	 * On: classic Source air strafing, where holding a direction near perpendicular to your
	 * momentum and turning the camera gains speed indefinitely. That is how this played until
	 * 2026-11-06 and it is one switch away.
	 *
	 * The two halves go together. `AirSpeedCap` 35 is tiny *because* a wide cap would let you
	 * accelerate straight forward and gain for free - which also means you can only influence your
	 * flight within about 2.5 degrees of perpendicular, and air control feels like nothing the rest
	 * of the time. Take the gain away and the cap stops being load-bearing, so redirect-only mode
	 * opens it to the full wish speed and air control becomes real.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air")
	bool bAirStrafeGain = false;

	/**
	 * How hard air input swings your momentum while `bAirStrafeGain` is off. Separate from
	 * `AirAccelerate` so flipping the switch back gives you the old feel exactly, with no retuning,
	 * and because `AirAccelerate` 12 is far too strong once the gate is open at every angle.
	 *
	 * It converts to a turn rate: at full speed you rotate about **`AirControlAccelerate` x 57.3
	 * degrees a second**, so 3 is roughly 170 deg/s - a 45 degree correction in a quarter second,
	 * and most of a reversal inside one jump. Raise it for Titanfall-style authority, drop it for
	 * something floatier. It cannot affect your speed whatever you set it to; the ratchet sees to
	 * that.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	float AirControlAccelerate = 3.f;

	/** Upward speed of a jump (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	float JumpSpeed = 750.f;

	/** Extra jumps in the air. Refilled on landing and when a wallrun starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	int32 AirJumps = 1;

	/** Holding jump re-jumps the moment you land. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air")
	bool bAutoHop = true;

	/** A jump pressed this long before landing still counts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Air", meta = (ClampMin = "0"))
	float JumpBufferTime = 0.15f;

	/* ---------------- Slide ---------------- */

	/** Speed added when a slide starts (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideBoost = 250.f;

	/** Friction while sliding (compare GroundFrictionSource). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideFriction = 0.3f;

	/** Crouching above this speed starts a slide (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideMinSpeed = 550.f;

	/** A slide ends below this speed (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideStopSpeed = 280.f;

	/** Seconds before the slide boost can trigger again. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideBoostCooldown = 1.f;

	/**
	 * Seconds after a slide ends before another one can start, however it ended: crouch released,
	 * slowed below SlideStopSpeed, jumped out of it, or left the ground. 0 lets you slide whenever
	 * you like, which is how it worked before this was added.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Slide", meta = (ClampMin = "0"))
	float SlideCooldown = 1.5f;

	/* ---------------- Wallrun ---------------- */

	/** Gravity multiplier while wallrunning. It ramps up over the last 40% of the run. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0"))
	float WallRunGravityScale = 0.18f;

	/** Longest a single wallrun can last (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0"))
	float WallRunMaxTime = 1.6f;

	/** Push away from the wall when you jump off (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0"))
	float WallKickSpeed = 550.f;

	/** How far from a wall you can be and still grab it (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0"))
	float WallAttachRange = 60.f;

	/** Minimum horizontal speed to start or keep a wallrun (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0"))
	float WallRunMinSpeed = 250.f;

	/** How "beside you" the wall must be: 0.65 means within about 40 degrees of straight left or right of your view. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Wallrun", meta = (ClampMin = "0", ClampMax = "1"))
	float WallSideTolerance = 0.65f;

	/* ---------------- Mantle ---------------- */

	/** Fraction of your run-up speed you keep after a mantle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Mantle", meta = (ClampMin = "0", ClampMax = "1"))
	float MantleSpeedKept = 0.75f;

	/** Seconds spent pulling yourself up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Mantle", meta = (ClampMin = "0.01"))
	float MantleDuration = 0.22f;

	/** Ledges this far above your feet can be mantled (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Mantle", meta = (ClampMin = "0"))
	float MantleMinHeight = 30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Mantle", meta = (ClampMin = "0"))
	float MantleMaxHeight = 170.f;

	/* ---------------- Grapple ---------------- */

	/*
	 * The grappling hook, which is a round effect rather than something you always have: the
	 * character fires it (AGetawayCharacter::FireGrapple traces for the anchor) and this is the
	 * pull, because the pull is movement and movement lives here.
	 *
	 * It is deliberately not a movement mode of its own. You stay in falling (or on the ground),
	 * which means Source air strafing, the slide, a wallrun you happen to catch and everything
	 * else carry on working while the rope is attached - so "you can still move while grappled"
	 * costs nothing and the hook reads as a force added to the movement rather than as a ride.
	 */

	/** How hard the rope reels you in (cm/s^2). The same shape as GroundAccelerate: it only ever adds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple", meta = (ClampMin = "0"))
	float GrapplePull = 4500.f;

	/** The fastest the rope itself will drag you along its own line (cm/s). Speed you earned
	 *  elsewhere is never taken off you - this caps what the pull adds, not how fast you may go. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple", meta = (ClampMin = "0"))
	float GrappleMaxSpeed = 1800.f;

	/**
	 * How far the pull leans toward where you are looking, instead of straight at the anchor.
	 *
	 * This is the steering. 0 winches you at the hook on a straight line whatever you do; 0.35
	 * bends the line enough that looking off to one side swings you round the anchor in an arc.
	 * The lean can never pass 90 degrees, so you can never pull yourself *away* from the anchor.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple", meta = (ClampMin = "0", ClampMax = "1"))
	float GrappleViewSteer = 0.35f;

	/**
	 * How much of gravity the rope holds up while you are in the air, 1 being all of it.
	 *
	 * At 1 the line never sags: a level grapple carries you level, like a zip line. Lower it for a
	 * rope that loses the fight with gravity, which swings more and reaches less.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple", meta = (ClampMin = "0", ClampMax = "1"))
	float GrappleGravityPayback = 1.f;

	/** Get this close to the anchor and the rope lets go, because you have arrived (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple", meta = (ClampMin = "20"))
	float GrappleReleaseDistance = 150.f;

	/**
	 * How much short of the anchor something has to block the line before it counts as cut (cm).
	 *
	 * The slack is what stops the anchor's own surface, and anything sitting flush against it,
	 * reading as the thing in the way. Swing behind a wall and the rope does cut, which is the
	 * answer you want: a hook through solid geometry would be stranger than losing it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Grapple", meta = (ClampMin = "0"))
	float GrappleCutSlack = 60.f;

	/* ---------------- Breakables ---------------- */

	/**
	 * Hitting an AGetawayBreakable at this speed or above bursts it (cm/s). **One rule, whatever you
	 * are doing** - running, sliding, falling, flung off a jump pad or boosted through a ring.
	 *
	 * **1100 is deliberately out of reach on foot.** Nothing you can do with the keyboard buys it:
	 * a full sprint is `RunSpeed` 800, a slide out of that run is 800 + `SlideBoost` = 1050, a
	 * wallrun is capped at `RunSpeed * 1.15` = 920, and since `bAirStrafeGain` went off there is no
	 * strafing your way past any of them. So breaking is **granted by the level, not by running
	 * hard**: a jump pad (`LaunchSpeed` 1400), a speed ring (1.5x, so 1200), a slide down a ramp
	 * where gravity carries you past it, or C4. Chosen 2026-11-06 for exactly that - you have to be
	 * going *exceptionally* fast, and only the pieces make you that fast.
	 *
	 * It is also the climb-or-break line, since `SmashAhead` returns below it - which means on foot
	 * a breakable is always a step, and `bMantleOverSmash` only decides anything when you arrive by
	 * piece.
	 *
	 * Two older rules died here, both on 2026-11-06. Sliding used to have a **second, much lower
	 * threshold** of its own (`SlideMinSpeed * 0.9`, so 450), so almost any slide burst almost
	 * anything it brushed; that branch is gone rather than retuned. And "breaking things is done by
	 * hitting them at speed" no longer holds on the flat: sliding into a panel on level ground does
	 * nothing at all now.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Breakables", meta = (ClampMin = "0"))
	float SmashSpeed = 1100.f;

	/**
	 * On: a breakable you are about to mantle is climbed rather than burst, even at smashing speed.
	 * Off (the default since 2026-11-06): **hitting it fast enough to break it breaks it**, and
	 * climbing is what happens below `SmashSpeed` - which is the one test at the top of SmashAhead,
	 * so the two never overlap. Below a full sprint you climb; at one you go through.
	 *
	 * Panels and anything too tall to climb break either way; what this decides is the crate whose
	 * top you could have pulled yourself onto. Arrive under the threshold and you still climb it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Breakables")
	bool bMantleOverSmash = false;

	/* ---------------- Sound ---------------- */

	/**
	 * Every sound below is played 2D, on the machine that owns this character and nowhere else.
	 *
	 * 2D rather than placed in the world because the listener is this character's own camera, so a
	 * positioned sound would sit on top of the listener anyway - and a 2D sound needs no Attenuation
	 * asset, which means a slot works the moment you point it at an imported wave. The cost is that
	 * nobody hears anybody else move: a remote character's movement component is tick-disabled by
	 * AGetawayCharacter::SetProxySimulation, so none of this runs for it. Rivals' footsteps would
	 * have to be driven off the replicated FGetawayNetMove instead. See docs/sound.md.
	 */

	/** Master switch for every movement sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	bool bMovementSounds = true;

	/** Volume every movement sound is multiplied by. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound", meta = (ClampMin = "0", ClampMax = "4"))
	float MovementSoundVolume = 1.f;

	/** Leaving the ground under your own power. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	TObjectPtr<USoundBase> JumpSound;

	/** The air jump. Worth making airier than JumpSound, so you can hear that you spent it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	TObjectPtr<USoundBase> DoubleJumpSound;

	/** Hitting the ground. Louder the harder you land, between LandMinSpeed and LandFullSpeed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	TObjectPtr<USoundBase> LandSound;

	/**
	 * The footstep pool. One is picked per step, never the one just played, so a handful of cuts
	 * from a real run cycle is enough to stop walking sounding mechanical.
	 *
	 * An array rather than one slot pointed at a Sound Cue with a Random node in it. The Cue is the
	 * usual way to do this and it is a dozen clicks of graph wiring per sound, where this is
	 * dragging waves into a list - and the Cue graph cannot be scripted, so there is no way to hand
	 * it over as a one-click setup either: USoundCue::FirstNode is BlueprintReadOnly and AllNodes
	 * is editor-only with no accessor. A Cue still works here; make it the only entry.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	TArray<TObjectPtr<USoundBase>> FootstepSounds;

	/** The scrape as a slide begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	TObjectPtr<USoundBase> SlideStartSound;

	/** Held for as long as the slide lasts, pitched by speed. The asset itself must loop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	TObjectPtr<USoundBase> SlideLoopSound;

	/** Catching a wall. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	TObjectPtr<USoundBase> WallRunStartSound;

	/** Held for as long as the wallrun lasts. The asset itself must loop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	TObjectPtr<USoundBase> WallRunLoopSound;

	/** Kicking off a wall with Space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	TObjectPtr<USoundBase> WallKickSound;

	/** Pulling yourself over a ledge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound")
	TObjectPtr<USoundBase> MantleSound;

	/* ---------------- Sound tuning ---------------- */

	/**
	 * Centimetres of ground covered between footsteps. Steps come faster as you speed up.
	 *
	 * 340 is 0.43 s between steps at RunSpeed 800 - a weighty jog. It was 220 to start with, which
	 * is 0.28 s and almost exactly the real cadence of the recording the footsteps were cut from,
	 * and it played far too quick: a true sprint cadence out of a first-person camera reads as
	 * frantic, and it gave the echoing samples no room to breathe between hits. Lower it for a
	 * faster patter, raise it for a heavier tread.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound Tuning", meta = (ClampMin = "20"))
	float FootstepStride = 340.f;

	/**
	 * A floor on how often a footstep may fire, in seconds, whatever the speed. 0 removes the cap.
	 *
	 * FootstepStride alone ties the rate to speed, and Source movement does not stop at RunSpeed:
	 * a few lossless bunny hops put you well past it, where distance-per-step turns into a machine
	 * gun. This is the backstop. A step waiting on the cap is not lost - the distance stays banked
	 * and it fires the moment the cap lets it, so the cap sets a maximum rate rather than dropping
	 * steps.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound Tuning", meta = (ClampMin = "0", ClampMax = "2"))
	float FootstepMinInterval = 0.3f;

	/** Below this ground speed you move silently (cm/s), so creeping about makes no noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound Tuning", meta = (ClampMin = "0"))
	float FootstepMinSpeed = 150.f;

	/** Pitch spread on each footstep, so one wave played over and over does not sound mechanical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound Tuning", meta = (ClampMin = "0", ClampMax = "0.5"))
	float FootstepPitchJitter = 0.08f;

	/** Landing slower than this is silent (cm/s downward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound Tuning", meta = (ClampMin = "0"))
	float LandMinSpeed = 200.f;

	/** Landing this fast is full volume (cm/s downward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound Tuning", meta = (ClampMin = "1"))
	float LandFullSpeed = 1600.f;

	/** How quiet the gentlest audible landing is, so a step off a kerb is still heard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound Tuning", meta = (ClampMin = "0", ClampMax = "1"))
	float LandMinVolume = 0.35f;

	/** Pitch of the slide loop at SlideStopSpeed, and at full tilt. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound Tuning", meta = (ClampMin = "0.1", ClampMax = "4"))
	float SlideLoopPitchMin = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound Tuning", meta = (ClampMin = "0.1", ClampMax = "4"))
	float SlideLoopPitchMax = 1.3f;

	/** Seconds a held loop takes to fade when it ends, so slides and wallruns do not click off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Sound Tuning", meta = (ClampMin = "0", ClampMax = "1"))
	float LoopFadeTime = 0.12f;

	/** Play the landing thump, scaled by the fall. Called from AGetawayCharacter::Landed. */
	void PlayLandSound(float DownwardSpeed);

	/* ---------------- Input from the character ---------------- */

	void QueueJump();
	void SetJumpHeld(bool bHeld) { bJumpHeld = bHeld; }
	void SetCrouchHeld(bool bHeld) { bCrouchHeld = bHeld; }

	/**
	 * Set how many air jumps this character gets, and hand them over right away.
	 *
	 * It exists for the round effect, which is applied in AGetawayCharacter::BeginPlay - by which
	 * time this component has already begun play and taken its own copy of AirJumps into
	 * AirJumpsLeft, so writing the property alone would leave you with the jumps you had a frame
	 * ago (none, at the tuned default) until the next time you landed.
	 */
	void SetAirJumps(int32 Count);

	/** Add a velocity kick, e.g. from a C4 blast. Lifts you off the ground if it points upward. */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void ApplyBlast(const FVector& DeltaVelocity);

	/**
	 * A sustained lift, from an AGetawayVortex you are inside. Unlike ApplyBlast this is not a kick:
	 * the vortex hands it over again every frame it holds you, and it expires by itself one frame
	 * after it stops doing so, so nothing has to tell the character it has left.
	 *
	 * UpSpeed is the climb it holds you at (cm/s) and UpAccel how fast you get there (cm/s^2). The
	 * steer pair is Source-style - SteerSpeed is a target in cm/s and SteerAccel a coefficient like
	 * GroundAccelerate - and it only ever adds speed toward the keys you are holding, so the lift
	 * can never slow down a player who arrived fast. The strongest UpSpeed wins if two overlap.
	 */
	void AddVortexLift(float UpSpeed, float UpAccel, float SteerSpeed, float SteerAccel, bool bBlockWallrun);

	/** True on a frame a vortex is holding you up. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsInVortex() const { return bVortexActive; }

	/**
	 * A timed speed boost, from an AGetawaySpeedRing you ran through.
	 *
	 * Unlike AddVortexLift this is handed over **once** and then runs on its own clock, because a
	 * ring is a gate you pass through rather than a volume that holds you: a frame later there is
	 * nothing overlapping you to keep offering it.
	 *
	 * Scale multiplies RunSpeed, which is the whole trick. Source friction always drags you back
	 * toward StopSpeed, so a bare velocity kick is eaten in about a second and "for three seconds"
	 * would mean nothing; raising the ceiling instead means holding W keeps you up there for as
	 * long as the boost lasts.
	 *
	 * SnapMinSpeed is the least speed you must already be going for the boost to lift you to that
	 * new ceiling on the spot. Below it you keep what you have and accelerate into the boost
	 * yourself, so stepping gingerly into a ring does not fling you at full speed in whatever
	 * direction you happened to be nudging. Pass 0 or less to never snap.
	 *
	 * Boosts do not stack: the stronger Scale wins and the longer clock wins, so running two rings
	 * back to back refreshes the three seconds rather than making you faster and faster.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void AddSpeedBoost(float Scale, float Duration, float SnapMinSpeed);

	/** True while a speed boost is running. The camera widens its FOV off this. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsSpeedBoosted() const { return BoostLeft > 0.f; }

	/** Seconds of boost left, 0 when there is none. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetSpeedBoostLeft() const { return FMath::Max(0.f, BoostLeft); }

	/** The multiplier a live boost is putting on RunSpeed, 1 when there is none. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetSpeedBoostScale() const { return BoostLeft > 0.f ? BoostScale : 1.f; }

	/**
	 * A timed low-friction window, from an AGetawayJumpPad that threw you.
	 *
	 * Handed over **once** and then run on the character's own clock, exactly like AddSpeedBoost
	 * and for the same reason: a pad is one kick on the frame you touch it, so there is nothing
	 * overlapping you a frame later to keep offering it.
	 *
	 * It is the **opposite lever to a speed boost**, and the two are worth keeping straight. A
	 * speed ring raises the ceiling (`RunSpeed`) and lets you climb to a speed you could not
	 * otherwise hold; a window lowers the drag and lets you **keep the speed you were already
	 * given**. That is what a jump pad wants: it throws you at `LaunchSpeed`, which is far above
	 * anything you can run at, and without this Source friction eats the landing in about a second.
	 * Neither one pushes you, and a window can never make you faster than you arrived.
	 *
	 * Scale multiplies `GroundFrictionSource` and `SlideFriction` together, so it covers running
	 * and sliding alike. 0 is frictionless ice; 1 or more is not a window and is ignored.
	 *
	 * **There is no air friction in this movement model to reduce.** `FallingLateralFriction` is 0
	 * and CalcVelocity applies no drag at all while you are airborne, so your speed through the air
	 * is already lossless and a window has nothing to do there. It bites the moment you land.
	 *
	 * Windows do not stack: the lower Scale wins and the longer clock wins, so two pads in a row
	 * refresh the window rather than compounding into ice.
	 */
	UFUNCTION(BlueprintCallable, Category = "Getaway")
	void AddLowFriction(float Scale, float Duration);

	/** True while a low-friction window is running. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsLowFriction() const { return LowFrictionLeft > 0.f; }

	/** Seconds of low friction left, 0 when there is none. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetLowFrictionLeft() const { return FMath::Max(0.f, LowFrictionLeft); }

	/** The multiplier a live window is putting on friction, 1 when there is none. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetFrictionScale() const { return LowFrictionLeft > 0.f ? LowFrictionScale : 1.f; }

	/**
	 * GroundFrictionSource and SlideFriction as the movement code should actually use them: the
	 * tuned value times any live window. Everything that drags you back toward StopSpeed goes
	 * through one of these, so one window reaches running and sliding at once.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetEffectiveGroundFriction() const { return GroundFrictionSource * GetFrictionScale(); }

	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetEffectiveSlideFriction() const { return SlideFriction * GetFrictionScale(); }

	/**
	 * RunSpeed as the movement code should actually use it: the tuned value times any live boost.
	 * Everything that reads RunSpeed as "how fast you are allowed to go" goes through this, so one
	 * boost reaches running, crouch-walking, air strafing and a wallrun at once.
	 *
	 * The camera's speed FOV deliberately still reads the raw RunSpeed, so a boosted run keeps the
	 * widened view it has earned instead of looking like a normal jog again.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetBoostedRunSpeed() const { return RunSpeed * GetSpeedBoostScale(); }

	/**
	 * Hook on, at this world point. Called by AGetawayCharacter::FireGrapple once its trace has
	 * found something; the character owns the charge, this owns the pull.
	 *
	 * AnchorActor is what was hit, kept weakly so that smashing the panel you hooked (or blowing
	 * up the crate) drops the rope by itself. Pass null for a hook into something that cannot go
	 * away, which is treated as a hook into the level.
	 *
	 * Like a vortex, the hook wins over a wallrun or a mantle: it is a route, and the thing you
	 * were doing a moment ago is not.
	 */
	void StartGrapple(const FVector& Anchor, AActor* AnchorActor);

	/** Let go. Your velocity is left exactly as it is: breaking a grapple never slows you down. */
	void StopGrapple();

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsGrappling() const { return bGrappling; }

	/** Where the hook is stuck, for the rope the character draws. Meaningless unless IsGrappling. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FVector GetGrappleAnchor() const { return GrappleAnchor; }

	/* ---------------- State ---------------- */

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsSliding() const { return bSliding; }

	/** Seconds left before a slide can start again. 0 means sliding is ready. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	float GetSlideCooldownLeft() const { return FMath::Max(0.f, SlideCooldownLeft); }

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsWallRunning() const;

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsMantling() const;

	UFUNCTION(BlueprintPure, Category = "Getaway")
	FVector GetWallNormal() const { return WallNormal; }

	/** World time of the last mantle / double jump, for camera effects. */
	float GetLastMantleTime() const { return LastMantleTime; }
	float GetLastDoubleJumpTime() const { return LastDoubleJumpTime; }

	/** GROUND, CROUCH, SLIDE, AIR, WALLRUN or MANTLE. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FString GetStateName() const;

	virtual float GetMaxSpeed() const override;
	virtual bool CanCrouchInCurrentState() const override;

protected:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;
	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

private:
	// Source movement primitives (PM_Friction, PM_Accelerate, PM_AirAccelerate)
	void SourceFriction(float Friction, float DeltaTime);
	void SourceAccelerate(const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime);
	void SourceAirAccelerate(const FVector& WishDir, float WishSpeed, float Accel, float DeltaTime);

	/**
	 * Stop an acceleration step from having made you *faster*, while leaving the direction it chose
	 * alone. Call it straight after one with the horizontal speed you had before.
	 *
	 * The ceiling is `FMath::Max(Before, Ceiling)`, and the Max is the whole subtlety: it only ever
	 * refuses a **gain**, never trims speed you already had. So a jump pad landing at 1400 or a
	 * slide out of a run at 1050 is untouched and decays by friction as it should, while input
	 * cannot push you past the speed input is asking for. Pass the wish speed as the ceiling, which
	 * is already boosted, so a speed ring still lets you reach 1200 under your own feet.
	 */
	void RatchetSpeed(float Before, float Ceiling);

	// Helpers
	FVector ViewForward() const;
	FVector ViewRight() const;
	FVector FeetLocation() const;
	float CapsuleRadius() const;
	float CapsuleHalfHeight() const;
	float WishAnalog() const;
	bool JumpPending() const;
	bool NearGround(float Distance) const;
	FCollisionQueryParams MoveQueryParams() const;

	// Wallrun
	bool TraceWall(const FVector& Dir, float Reach, FHitResult& OutHit, float& OutGap) const;
	bool FindSideWall(float Reach, FHitResult& OutHit, float& OutGap) const;
	bool IsSameWall(const FVector& Normal, const FVector& Point) const;
	bool TryStartWallRun();
	void PhysWallRun(float DeltaTime, int32 Iterations);
	void EndWallRun(float PushOff);
	void WallKick();

	// Mantle
	/**
	 * The whole geometric question - is there a ledge ahead I could pull myself onto, and where
	 * would it put me - with nothing changed. ExtraReach looks further ahead than a climb would
	 * really start from, which is how SmashAhead can ask the same question its break sweep is
	 * about to answer for itself. TryStartMantle passes 0 and then commits to the answer.
	 */
	bool FindMantle(float ExtraReach, FGetawayMantleFit& Out) const;
	bool TryStartMantle();
	void PhysMantle(float DeltaTime, int32 Iterations);

	// Slide. Always stop a slide through EndSlide, so the cooldown starts no matter what ended it.
	void EndSlide();

	// Breakables and loose scraps
	void SmashAhead(float DeltaTime);
	void PushScraps();

	// Vortex lift
	void ApplyVortexLift(float DeltaSeconds);

	// Grapple. The pull, and the three things that end it short of a second click.
	void ApplyGrapplePull(float DeltaSeconds);
	/** The full look direction, pitch included - ViewForward is flat, and a hook is aimed upward. */
	FVector ViewDirection() const;
	/** True when something solid has come between you and the anchor. */
	bool IsGrappleLineCut(const FVector& From, const FVector& ToAnchorDir, float Distance) const;

	// Sound. All of it no-ops unless this character is the one this machine is playing.
	bool SoundsAudible() const;
	void PlayMovementSound(USoundBase* Sound, float VolumeScale = 1.f, float Pitch = 1.f) const;
	void StartSoundLoop(TObjectPtr<UAudioComponent>& Slot, USoundBase* Sound);
	void StopSoundLoop(TObjectPtr<UAudioComponent>& Slot);
	void UpdateFootsteps(float DeltaSeconds);
	void UpdateSoundLoops();
	USoundBase* PickFootstep();

	// Runtime state
	bool bJumpHeld = false;
	bool bCrouchHeld = false;
	bool bSliding = false;
	float JumpQueuedAt = -100.f;
	float SlideBoostCooldownLeft = 0.f;
	float SlideCooldownLeft = 0.f;
	int32 AirJumpsLeft = 0;
	float LastDoubleJumpTime = -100.f;

	/**
	 * The live speed boost: what it multiplies RunSpeed by, and how long it has left. Counted down
	 * in UpdateCharacterStateBeforeMovement alongside the slide and wall cooldowns, and put back to
	 * 1 the instant it runs out, so GetBoostedRunSpeed needs no branch of its own.
	 */
	float BoostScale = 1.f;
	float BoostLeft = 0.f;

	/**
	 * The live low-friction window: what it multiplies ground and slide friction by, and how long
	 * it has left. Counted down next to the boost above and put back to 1 the instant it runs out,
	 * so GetFrictionScale needs no branch of its own.
	 */
	float LowFrictionScale = 1.f;
	float LowFrictionLeft = 0.f;

	FVector WallNormal = FVector::ZeroVector;
	/**
	 * What the current wallrun is actually on. Kept so a breakable can hold you up while you run
	 * it and give way when you push off: SmashAhead leaves this one alone, and WallKick breaks it.
	 */
	TWeakObjectPtr<AActor> WallActor;
	float WallRunTime = 0.f;
	float WallCooldown = 0.f;
	bool bHasLastWall = false;
	FVector LastWallNormal = FVector::ZeroVector;
	float LastWallPlane = 0.f;

	/**
	 * The lift a vortex has offered for the next update, and whether one is holding you now. Set by
	 * AddVortexLift and consumed once per tick, so a vortex that stops calling stops lifting - no
	 * end-overlap bookkeeping, and two overlapping columns resolve to the stronger one by themselves.
	 */
	float VortexUpSpeed = 0.f;
	float VortexUpAccel = 0.f;
	float VortexSteerSpeed = 0.f;
	float VortexSteerAccel = 0.f;
	bool bVortexBlocksWallrun = false;
	bool bVortexOffered = false;
	bool bVortexActive = false;

	/**
	 * The live hook: where it is stuck and what it is stuck to. The actor is weak on purpose - a
	 * breakable you hooked can be smashed by somebody else while you are hanging off it, and the
	 * pointer going stale is how the rope finds out.
	 */
	bool bGrappling = false;
	FVector GrappleAnchor = FVector::ZeroVector;
	TWeakObjectPtr<AActor> GrappleActor;
	bool bGrappleHadActor = false;

	FVector MantleStart = FVector::ZeroVector;
	FVector MantleEnd = FVector::ZeroVector;
	FVector MantleDir = FVector::ZeroVector;
	FVector MantleSavedVelocity = FVector::ZeroVector;
	float MantleTopCenterZ = 0.f;
	float MantleElapsed = 0.f;
	float LastMantleTime = -100.f;

	/**
	 * The two held loops, and how far you have walked since the last footstep.
	 *
	 * The loops are spawned attached to the capsule with bStopWhenAttachedToDestroyed, so dying
	 * mid-slide takes the scrape with it rather than leaving it playing in an empty level.
	 */
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SlideLoop;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WallRunLoop;

	float FootstepDistance = 0.f;
	float LastFootstepTime = -100.f;
	int32 LastFootstepIndex = -1;
};
