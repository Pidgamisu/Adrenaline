#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/**
 * Turning an analogue stick into something this game can use.
 *
 * A mouse reports "you moved N counts since the last frame", so mouse look is a delta and is
 * already frame-rate independent. A stick reports "I am being held this far", which is a rate:
 * multiply it by the frame time or your turn speed changes with your frame rate. That difference is
 * why the stick cannot be fed through the mouse look formula, and why the character and the
 * placement camera each get a second look handler rather than a second key on the mouse one.
 *
 * The shaping lives here so both sticks feel the same and there is one place to change it.
 */
namespace GetawayStick
{
	/**
	 * Radial dead zone, with the rest of the travel rescaled back up to 0..1.
	 *
	 * Rescaling is the part that matters: without it a 0.18 dead zone makes the stick jump straight
	 * to 18% speed the moment it leaves the middle, so there is no slow end to the range at all.
	 * Radial rather than per-axis, so a diagonal push is one direction and not two separate nudges
	 * that each have to clear the dead zone on their own.
	 */
	inline FVector2D ApplyDeadZone(const FVector2D& Raw, float DeadZone)
	{
		const float Mag = Raw.Size();
		if (Mag <= KINDA_SMALL_NUMBER) return FVector2D::ZeroVector;

		const float Dead = FMath::Clamp(DeadZone, 0.f, 0.95f);
		if (Mag <= Dead) return FVector2D::ZeroVector;

		const float Travel = FMath::Min((Mag - Dead) / (1.f - Dead), 1.f);
		return (Raw / Mag) * Travel;
	}

	/** The same for a single axis, which is what the placement camera's up/down triggers are. */
	inline float ApplyDeadZone(float Raw, float DeadZone)
	{
		const float Mag = FMath::Abs(Raw);
		if (Mag <= KINDA_SMALL_NUMBER) return 0.f;

		const float Dead = FMath::Clamp(DeadZone, 0.f, 0.95f);
		if (Mag <= Dead) return 0.f;

		const float Travel = FMath::Min((Mag - Dead) / (1.f - Dead), 1.f);
		return FMath::Sign(Raw) * Travel;
	}

	/**
	 * Response curve. An exponent of 1 is linear, which feels twitchy near the middle because the
	 * first millimetre of travel is already a real turn; above 1 the inner half of the stick is
	 * slow and fine and the outer edge is fast. Applied to the magnitude only, so the curve can
	 * never bend the direction you are pushing.
	 */
	inline FVector2D ApplyCurve(const FVector2D& In, float Exponent)
	{
		const float Mag = In.Size();
		if (Mag <= KINDA_SMALL_NUMBER) return FVector2D::ZeroVector;

		const float Curved = FMath::Pow(FMath::Min(Mag, 1.f), FMath::Max(Exponent, 1.f));
		return (In / Mag) * Curved;
	}

	/** The same for a single axis. */
	inline float ApplyCurve(float In, float Exponent)
	{
		const float Mag = FMath::Abs(In);
		if (Mag <= KINDA_SMALL_NUMBER) return 0.f;

		const float Curved = FMath::Pow(FMath::Min(Mag, 1.f), FMath::Max(Exponent, 1.f));
		return FMath::Sign(In) * Curved;
	}

	/**
	 * Past FullTilt, call it full tilt.
	 *
	 * This is here for air strafing. The Source air acceleration the movement component copies
	 * grows with how far sideways your input points, so a stick held 80% of the way accelerates you
	 * noticeably worse than a keyboard key, which is always 100%. Rounding the outer fifth of the
	 * stick up to 1 gives a pad the same air control as A and D without losing the analogue range
	 * underneath it. FullTilt of 1 switches this off and gives honest analogue input.
	 */
	inline FVector2D SnapToFullTilt(const FVector2D& In, float FullTilt)
	{
		const float Mag = In.Size();
		const float Threshold = FMath::Clamp(FullTilt, 0.1f, 1.f);
		if (Mag <= KINDA_SMALL_NUMBER || Mag < Threshold) return In;

		return (In / Mag);
	}

	/**
	 * Every pad key the game binds anywhere.
	 *
	 * One extra input action is mapped to all of them, for no reason other than noticing that the
	 * pad is being used at all. Most of the bindings share an action with their keyboard twin
	 * (space and A are both the jump action), and an Enhanced Input handler is not told which key
	 * fired it, so the shared actions cannot answer "pad or keyboard?" on their own. The HUD needs
	 * that answer to name the right buttons in its hints.
	 */
	inline const TArray<FKey>& ProbeKeys()
	{
		static const TArray<FKey> Keys = {
			EKeys::Gamepad_LeftStick_Up,       EKeys::Gamepad_LeftStick_Down,
			EKeys::Gamepad_LeftStick_Left,     EKeys::Gamepad_LeftStick_Right,
			EKeys::Gamepad_RightStick_Up,      EKeys::Gamepad_RightStick_Down,
			EKeys::Gamepad_RightStick_Left,    EKeys::Gamepad_RightStick_Right,
			EKeys::Gamepad_FaceButton_Bottom,  EKeys::Gamepad_FaceButton_Right,
			EKeys::Gamepad_FaceButton_Left,    EKeys::Gamepad_FaceButton_Top,
			EKeys::Gamepad_LeftShoulder,       EKeys::Gamepad_RightShoulder,
			EKeys::Gamepad_LeftTrigger,        EKeys::Gamepad_RightTrigger,
			EKeys::Gamepad_DPad_Up,            EKeys::Gamepad_DPad_Down,
			EKeys::Gamepad_DPad_Left,          EKeys::Gamepad_DPad_Right,
			EKeys::Gamepad_LeftThumbstick,     EKeys::Gamepad_RightThumbstick,
			EKeys::Gamepad_Special_Left,       EKeys::Gamepad_Special_Right,
		};
		return Keys;
	}
}
