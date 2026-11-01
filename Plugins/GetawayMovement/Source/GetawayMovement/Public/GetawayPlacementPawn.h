#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Pawn.h"
#include "GetawayPiece.h"
#include "GetawayPlacementPawn.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** Where the piece in your hand would land, and whether it may. */
USTRUCT(BlueprintType)
struct FGetawayPlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Getaway") bool bHasTarget = false;
	UPROPERTY(BlueprintReadOnly, Category = "Getaway") bool bBlocked = false;
	UPROPERTY(BlueprintReadOnly, Category = "Getaway") FVector Centre = FVector::ZeroVector;
	/** World-space extent after rotation, so it is the box you see. */
	UPROPERTY(BlueprintReadOnly, Category = "Getaway") FVector Size = FVector::ZeroVector;
	UPROPERTY(BlueprintReadOnly, Category = "Getaway") FRotator Rotation = FRotator::ZeroRotator;
	/** Copied from the piece: which way round it is matters, so the ghost shows the facing. */
	UPROPERTY(BlueprintReadOnly, Category = "Getaway") bool bDirectional = false;
	/** Copied from the piece: it lies on a surface and cannot be placed without one. */
	UPROPERTY(BlueprintReadOnly, Category = "Getaway") bool bSurfaceMounted = false;
	/**
	 * The piece's size in its own axes, which for a surface-mounted piece is the catalog size
	 * trimmed back to the part of it that is actually held up. This, not the catalog, is what the
	 * spawned actor is scaled to.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Getaway") FVector LocalSize = FVector::ZeroVector;
	/** True when the trim took something off, so the HUD can say the plate was cut down. */
	UPROPERTY(BlueprintReadOnly, Category = "Getaway") bool bTrimmed = false;
	/**
	 * Nothing to hang off: the piece would float with clear air on every side. A level is a
	 * structure you extend, not a cloud of blocks, so this refuses the placement.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Getaway") bool bFloating = false;

	bool CanPlace() const { return bHasTarget && !bBlocked && !bFloating; }
};

/**
 * The placement phase camera. You fly around, aim at a surface, and drop the piece you drafted.
 *
 * Placement follows the browser lab's placeBox(): the piece sits flush on the face you are aiming
 * at, and is snapped to a GridSize grid on the other two axes. Rotation is in 90 degree steps and
 * starts facing whichever way you are looking, so a wall you place reads the way you expect.
 *
 * Input is built in code, like AGetawayCharacter, so no input assets are needed:
 * WASD fly, Space up, C down, Shift faster, mouse wheel cycles your hand, R turns the piece,
 * left click places it, Enter starts the run.
 */
UCLASS(Blueprintable)
class GETAWAYMOVEMENT_API AGetawayPlacementPawn : public APawn
{
	GENERATED_BODY()

public:
	AGetawayPlacementPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Getaway")
	TObjectPtr<UCameraComponent> Camera;

	/* ---------------- Flying ---------------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Fly", meta = (ClampMin = "0"))
	float FlySpeed = 1200.f;

	/** Multiplier while Shift is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Fly", meta = (ClampMin = "1"))
	float FastMultiplier = 3.f;

	/** Higher snaps to the new speed sooner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Fly", meta = (ClampMin = "0.1"))
	float FlySmoothing = 12.f;

	/* ---------------- Look ---------------- */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look", meta = (ClampMin = "0.01"))
	float MouseSensitivity = 2.2f;

	/** Divides out Unreal's axis scaling, exactly as AGetawayCharacter does. Keep the two in step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look", meta = (ClampMin = "0.0001"))
	float MouseInputScale = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look", meta = (ClampMin = "60", ClampMax = "140"))
	float BaseFOV = 100.f;

	/* ---------------- Look on a pad ---------------- */

	/**
	 * Degrees per second at full stick. Slower than the character's, because this camera is for
	 * lining a piece up rather than fighting, and a fast pad turn here overshoots the grid square
	 * you were aiming at. See AGetawayCharacter for why the stick cannot share the mouse formula.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)", meta = (ClampMin = "10"))
	float StickLookYawSpeed = 150.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)", meta = (ClampMin = "10"))
	float StickLookPitchSpeed = 110.f;

	/** Linear, to match the character's stick. Only the speeds differ between the two. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)", meta = (ClampMin = "1", ClampMax = "4"))
	float StickLookExponent = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)", meta = (ClampMin = "0", ClampMax = "0.5"))
	float StickLookDeadZone = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Look (Pad)")
	bool bInvertStickY = false;

	/** Radial dead zone for the fly stick, and a flat one for the up/down triggers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Fly (Pad)", meta = (ClampMin = "0", ClampMax = "0.5"))
	float MoveStickDeadZone = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Fly (Pad)", meta = (ClampMin = "0", ClampMax = "0.5"))
	float TriggerDeadZone = 0.1f;

	/** True while the last thing you touched was the pad, so the HUD can name the right buttons. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool IsUsingGamepad() const { return bUsingGamepad; }

	/* ---------------- Placement ---------------- */

	/** Snap step in cm on the two axes that are not pinned to the surface. The lab uses 0.5 m. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Placement", meta = (ClampMin = "1"))
	float GridSize = 50.f;

	/** How far you can place from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Placement", meta = (ClampMin = "100"))
	float AimRange = 25000.f;

	/** Shrinks the overlap test so a piece resting flush on a surface is not called blocked. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Placement", meta = (ClampMin = "0"))
	float FitTolerance = 3.f;

	/**
	 * How far past its own faces a piece looks for something to touch. Everything you build has to
	 * meet something that is already there, so the level stays one connected structure instead of
	 * a scatter of blocks in mid-air. This has to be bigger than FitTolerance or the skin would
	 * sit inside the gap that test leaves.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Placement", meta = (ClampMin = "0.1"))
	float ContactReach = 6.f;

	/**
	 * How far off a world axis a face has to be before a piece lies down on it instead of sitting
	 * square. Flat floors, walls and ceilings are exactly axis-aligned and stay square whatever
	 * this is; a ramp face is 18.4 degrees off and tips the piece onto the slope. Raise it past
	 * that and ramps stop slanting what you put on them.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Placement", meta = (ClampMin = "0.5", ClampMax = "45"))
	float SlantTolerance = 5.f;

	/** Draw the ghost box. Off if you want to see the piece land unobstructed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Getaway|Placement")
	bool bDrawGhost = true;

	/* ---------------- State, for the HUD ---------------- */

	/** Which slot of the drafted hand is in your hand right now. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 GetHandSlot() const { return HandSlot; }

	/** Where the piece would land this frame. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FGetawayPlacement GetPlacement() const { return Planned; }

	/** 0..3, in 90 degree steps, including the step you get from facing. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 GetRotationSteps() const;

	/** Which way the piece in your hand faces, as the lab names it: north is +X, east is +Y. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	FString GetFacingName() const;

	/* ---------------- The hand, as this machine sees it ---------------- */

	/**
	 * The hand lives on this player's AGetawayPlayerState, dealt per player and replicated to its
	 * owner. These two read it from there, which is why the HUD can draw it on a client that has
	 * no game mode at all.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	int32 GetHandCount() const;

	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool GetHandPiece(int32 Slot, FGetawayPieceDef& OutDef) const;

	/** This player's own round state, or null in the frame or two before it has replicated. */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	class AGetawayPlayerState* GetGetawayPlayerState() const;

	/**
	 * Whether building is allowed at all this moment. Holding this camera is not enough: forfeiting
	 * a run with Backspace hands it back mid-round, with whatever is left of your hand still in it,
	 * and dropping pieces into a level the others are still running through is not yours to do.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway")
	bool CanBuildNow() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void FellOutOfWorld(const class UDamageType& DmgType) override;

protected:
	virtual void BeginPlay() override;

private:
	void BuildDefaultInput();
	bool CurrentPiece(FGetawayPieceDef& OutDef) const;
	FGetawayPlacement ComputePlacement(const FGetawayPieceDef& Def) const;
	void DrawGhost() const;

	/** The world-space extent of a piece of this own-axes size at this rotation: the ghost's box. */
	static FVector WorldSizeFor(const FVector& LocalSize, const FRotator& Rotation);
	/** Would a piece of this world-space size at this centre push into anything? */
	bool IsBlockedAt(const FVector& Centre, const FRotator& Rotation, const FVector& LocalSize) const;

	/**
	 * Is every corner of a plate of this size, at this spot, actually backed by something? Used to
	 * check a trim rather than to forbid one: the client works out how far the surface reaches and
	 * the server checks the answer with this. Corners are tested separately, so a plate bridging
	 * two surfaces that meet underneath is supported.
	 */
	bool IsFullySupported(const FVector& Centre, const FRotator& Rotation, const FVector& LocalSize) const;

	/** True if anything solid is within ContactReach of the piece's faces. See ContactReach. */
	bool IsTouchingSomething(const FVector& Centre, const FRotator& Rotation, const FVector& LocalSize) const;

	/**
	 * Shrink a surface-mounted piece to the part of it that is actually held up, instead of
	 * refusing it. Grows a rectangle outwards from the cell you aimed at for as long as whole rows
	 * and columns are backed, so spikes placed over the end of a ledge stop at the edge.
	 *
	 * False only if the spot you aimed at is not supported at all, which should not happen - you
	 * got the aim point by hitting something.
	 */
	bool TrimToSupport(const FVector& AimPoint, const FVector& UntrimmedCentre, const FRotator& Rotation,
		const FGetawayPieceDef& Def, FVector& OutCentre, FVector& OutLocalSize) const;

	/** What a surface-mounted piece at this spot would be stuck to, or null for bare level geometry. */
	AActor* FindHostAt(const FVector& Centre, const FRotator& Rotation, const FGetawayPieceDef& Def) const;

	/**
	 * Ask the server to place the piece. It looks the slot up in the hand it dealt and re-runs the
	 * overlap test itself, so a client cannot place a piece it was never given or drop one inside
	 * the level geometry.
	 */
	UFUNCTION(Server, Reliable)
	void ServerPlacePiece(int32 Slot, FVector_NetQuantize100 Centre, FRotator Rotation,
		FVector_NetQuantize100 LocalSize);

	UFUNCTION(Server, Reliable)
	void ServerStartRun();

#if !UE_BUILD_SHIPPING
	/**
	 * The headless two-player check, run with -GetawayNetTest=place on one client and
	 * -GetawayNetTest=watch on the others. "place" asks the server for a piece without needing a
	 * mouse; both then log every dynamically placed piece they can see, which is how piece
	 * replication was verified without a pair of hands on two keyboards. See CLAUDE.md.
	 */
	void NetSelfTestPlace(bool bThenTryAgain);
	void NetSelfTestCount();
#endif

	void OnMove(const FInputActionValue& Value);
	void OnUpDown(const FInputActionValue& Value);
	void OnMoveStick(const FInputActionValue& Value);
	void OnUpDownStick(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnLookStick(const FInputActionValue& Value);
	void OnGamepadUsed(const FInputActionValue& Value);
	void OnFast(const FInputActionValue& Value);
	void OnSlow(const FInputActionValue& Value);
	void OnPlace(const FInputActionValue& Value);
	void OnRotate(const FInputActionValue& Value);
	void OnNextPiece(const FInputActionValue& Value);
	void OnPrevPiece(const FInputActionValue& Value);
	void OnStartRun(const FInputActionValue& Value);

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> UpDownAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveStickAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> UpDownStickAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookStickAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> GamepadProbeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FastAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PlaceAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RotateAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> NextPieceAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PrevPieceAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> StartRunAction;

	FVector2D MoveInput = FVector2D::ZeroVector;
	float UpInput = 0.f;

	/**
	 * The pad's fly input, kept apart from the keyboard's rather than written into the same fields.
	 * Both are latched until the input is released, so sharing them would mean the stick going
	 * back to centre also wiping WASD for anyone using both at once. Tick adds the two together.
	 */
	FVector2D StickMoveInput = FVector2D::ZeroVector;
	float StickUpInput = 0.f;

	bool bFast = false;
	FVector FlyVelocity = FVector::ZeroVector;

	/** Purely for the HUD's hint lines. Local to this machine; nothing about it replicates. */
	bool bUsingGamepad = false;

	int32 HandSlot = 0;
	int32 ManualRotation = 0;
	FGetawayPlacement Planned;
};
