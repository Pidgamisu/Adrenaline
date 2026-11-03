#include "GetawayWindmill.h"

#include "GetawayCharacter.h"
#include "GetawayGameState.h"
#include "GetawayMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Math/RotationMatrix.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** The engine cube and cylinder are both 100 units, centred - the assumption every piece makes. */
	constexpr float MeshLocal = 100.f;
	/** Plates and spokes are pooled in the constructor and the spares hidden, as the vortex pools bars. */
	constexpr int32 MaxBlades = 8;
	/** How far past the plate its contact box reaches, in cm. Enough to notice a sweep, not a ride. */
	constexpr float ContactSkin = 12.f;
	/**
	 * Slack on the "is this a rider or a victim" test, in cm. Feet this far below the top face still
	 * count as standing on it, which covers the frame a rider has left the plate but not the box.
	 */
	constexpr float RiderSkin = 8.f;
}

AGetawayWindmill::AGetawayWindmill()
{
	PrimaryActorTick.bCanEverTick = true;
	// Before physics, so the plates have moved by the time a character standing on one asks its
	// base where it is. Characters tick in this group too; being in it is what keeps the ride to at
	// most one frame behind rather than a frame behind and a frame ahead by turns.
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// The root never moves. Everything the piece does is children moving inside it, which is also
	// why there is no movement replication to do: see the class comment on the derived spin.
	Mount = CreateDefaultSubobject<USceneComponent>(TEXT("Mount"));
	RootComponent = Mount;

	// Turns. The hub and the spokes are attached here, because they are what should look like it is
	// turning; the plates are not, because they must not turn - that is the whole decision.
	Wheel = CreateDefaultSubobject<USceneComponent>(TEXT("Wheel"));
	Wheel->SetupAttachment(Mount);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Hub = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Hub"));
	Hub->SetupAttachment(Wheel);
	Hub->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cylinder.Succeeded())
	{
		Hub->SetStaticMesh(Cylinder.Object);
	}

	for (int32 I = 0; I < MaxBlades; ++I)
	{
		// The spoke. Cosmetic: with collision on, the one place a spoke crosses is exactly where a
		// rider stands, and it would trip them on the plate they are riding.
		UStaticMeshComponent* Arm = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Arm%d"), I));
		Arm->SetupAttachment(Wheel);
		Arm->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Cylinder.Succeeded())
		{
			Arm->SetStaticMesh(Cylinder.Object);
		}
		Arms.Add(Arm);

		// The plate. Attached to the fixed root, not the wheel, so its orientation never changes
		// and it hands a rider a pure translation. Movable and blocking is the whole of what the
		// engine's based movement needs to carry somebody standing on it.
		UStaticMeshComponent* Blade = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Blade%d"), I));
		Blade->SetupAttachment(Mount);
		Blade->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
		Blade->SetMobility(EComponentMobility::Movable);
		if (Cube.Succeeded())
		{
			Blade->SetStaticMesh(Cube.Object);
		}
		Blades.Add(Blade);

		// The contact box rides alongside the plate rather than inside it, so its extent stays in
		// plain cm: a box parented to the plate would be multiplied by the plate's own scale.
		UBoxComponent* Contact = CreateDefaultSubobject<UBoxComponent>(*FString::Printf(TEXT("Contact%d"), I));
		Contact->SetupAttachment(Mount);
		Contact->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Contact->SetCollisionObjectType(ECC_WorldDynamic);
		Contact->SetCollisionResponseToAllChannels(ECR_Ignore);
		Contact->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		Contact->SetGenerateOverlapEvents(true);
		Contacts.Add(Contact);
	}

	// Replicated so a wheel somebody places exists for everyone - the lesson of 2026-10-18. The
	// actor itself never moves and the plates are worked out from the shared run clock on every
	// machine, so there is nothing for movement replication to send.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
}

void AGetawayWindmill::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Runs on every machine, because the transform travels in the spawn - the same trick
	// AGetawayVortex, AGetawayJumpPad and AGetawayRamp use to build themselves with no mesh going
	// over the wire.
	LayOutWheel();
	PositionBlades(StartAngle);
}

void AGetawayWindmill::BeginPlay()
{
	Super::BeginPlay();
	LayOutWheel();
	PositionBlades(GetWheelAngle());
}

/* ============================================================ the wheel */

FVector AGetawayWindmill::RestDirection(int32 Index, int32 Used) const
{
	// Measured from straight up, turning toward +Y, so plate 0 sits at the top of the stroke at
	// angle zero and the piece reads the same whichever way R has turned it.
	const float Radians = 2.f * PI * static_cast<float>(Index) / static_cast<float>(FMath::Max(1, Used));
	return FVector(0.f, FMath::Sin(Radians), FMath::Cos(Radians));
}

FVector AGetawayWindmill::GetSweptSize() const
{
	// Along the axle the wheel is just a plate deep. Across it, a level plate at the side of the
	// stroke reaches half its width past the hub; up and down, half its thickness.
	return FVector(
		BladeLength,
		2.f * (BladeReach + BladeWidth * 0.5f),
		2.f * (BladeReach + BladeThickness * 0.5f));
}

FVector AGetawayWindmill::GetAxleDirection() const
{
	// Placement lines a directional piece's +X up with the way you are looking, so the axle runs
	// away from you and you meet the wheel face on.
	return GetActorQuat().GetAxisX().GetSafeNormal();
}

float AGetawayWindmill::GetWheelAngle() const
{
	const UWorld* World = GetWorld();

	// The run clock, not world time and not an accumulated angle. It is computed from the
	// replicated run start time, so every machine lands on the same angle on the same frame, and a
	// client that joined late or hitched does not have to catch up.
	float Elapsed = 0.f;
	if (const AGetawayGameState* GS = World ? World->GetGameState<AGetawayGameState>() : nullptr)
	{
		Elapsed = GS->GetRunTime();
	}

	// Outside a run that is 0, which holds the wheel at StartAngle through placement and freezes it
	// when the round ends. A wheel dropped into a map by hand has no round to read, hence the flag.
	if (Elapsed <= 0.f && bSpinOutsideRun && World)
	{
		Elapsed = World->GetTimeSeconds();
	}

	return StartAngle + SpinRate * Elapsed;
}

void AGetawayWindmill::LayOutWheel()
{
	const int32 Used = FMath::Clamp(BladeCount, 1, MaxBlades);

	if (Hub)
	{
		// The cylinder mesh stands along its own Z, so turn it to lie along the axle instead.
		Hub->SetRelativeRotation(FRotationMatrix::MakeFromZ(FVector::XAxisVector).Rotator());
		Hub->SetRelativeScale3D(FVector(
			HubRadius * 2.f / MeshLocal,
			HubRadius * 2.f / MeshLocal,
			HubWidth / MeshLocal));
		Hub->SetRelativeLocation(FVector::ZeroVector);
	}

	for (int32 I = 0; I < MaxBlades; ++I)
	{
		UStaticMeshComponent* Arm = Arms.IsValidIndex(I) ? Arms[I].Get() : nullptr;
		UStaticMeshComponent* Blade = Blades.IsValidIndex(I) ? Blades[I].Get() : nullptr;
		UBoxComponent* Contact = Contacts.IsValidIndex(I) ? Contacts[I].Get() : nullptr;

		// Every one, hidden spares included, and unconditionally, so clearing the property in the
		// Details panel puts the engine default back - as AGetawayVortex does for its bars.
		if (Arm) Arm->SetMaterial(0, ArmMaterial);
		if (Blade) Blade->SetMaterial(0, BladeMaterial);

		const bool bLive = I < Used;
		if (Arm) Arm->SetVisibility(bLive);
		if (Blade)
		{
			Blade->SetVisibility(bLive);
			// A hidden spare must not be standable, or the wheel would have invisible floors
			// stacked at the hub.
			Blade->SetCollisionEnabled(bLive ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}
		if (Contact)
		{
			Contact->SetCollisionEnabled(bLive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		}
		if (!bLive) continue;

		if (Arm)
		{
			// Hub out to the plate centre: a cylinder turned so its own Z runs along the spoke, and
			// pushed half way out, because the mesh is centred.
			const FVector Dir = RestDirection(I, Used);
			Arm->SetRelativeRotation(FRotationMatrix::MakeFromZ(Dir).Rotator());
			Arm->SetRelativeScale3D(FVector(
				ArmThickness / MeshLocal,
				ArmThickness / MeshLocal,
				BladeReach / MeshLocal));
			Arm->SetRelativeLocation(Dir * BladeReach * 0.5f);
		}

		if (Blade)
		{
			Blade->SetRelativeScale3D(FVector(
				BladeLength / MeshLocal,
				BladeWidth / MeshLocal,
				BladeThickness / MeshLocal));
		}

		if (Contact)
		{
			Contact->SetBoxExtent(FVector(
				BladeLength * 0.5f + ContactSkin,
				BladeWidth * 0.5f + ContactSkin,
				BladeThickness * 0.5f + ContactSkin));
		}
	}
}

void AGetawayWindmill::PositionBlades(float Angle)
{
	const int32 Used = FMath::Clamp(BladeCount, 1, MaxBlades);
	const FQuat Spin(FVector::XAxisVector, FMath::DegreesToRadians(Angle));

	// The spokes and the hub turn; the plates only orbit. Both are driven from the same angle, so a
	// spoke always points at its own plate however the piece is tuned.
	if (Wheel)
	{
		Wheel->SetRelativeRotation(Spin);
	}

	for (int32 I = 0; I < Used; ++I)
	{
		const FVector Where = Spin.RotateVector(RestDirection(I, Used)) * BladeReach;

		// Relative rotation left alone, which is the piece: a plate is always level in the piece's
		// own frame, so whoever is standing on it is handed a translation and nothing else.
		if (UStaticMeshComponent* Blade = Blades.IsValidIndex(I) ? Blades[I].Get() : nullptr)
		{
			Blade->SetRelativeLocation(Where);
		}
		if (UBoxComponent* Contact = Contacts.IsValidIndex(I) ? Contacts[I].Get() : nullptr)
		{
			Contact->SetRelativeLocation(Where);
		}
	}
}

void AGetawayWindmill::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	PositionBlades(GetWheelAngle());
	ShoveStruck();
}

void AGetawayWindmill::SetArmed(bool bNew)
{
	bArmed = bNew;
	// Each round starts clean, so a plate that batted you at the end of the last one can bat you
	// again on the first frame of this one - the same reset as the jump pad's.
	LastShoveTime.Reset();
}

/* ============================================================ being hit by one */

void AGetawayWindmill::ShoveStruck()
{
	if (!bArmed || ShoveScale <= 0.f || FMath::IsNearlyZero(SpinRate))
	{
		LastShoveTime.Reset();
		return;
	}

	const UWorld* World = GetWorld();
	if (!World) return;

	const float Now = World->GetTimeSeconds();
	const FVector Omega = GetAxleDirection() * FMath::DegreesToRadians(SpinRate);
	const FVector Centre = GetActorLocation();
	const int32 Used = FMath::Clamp(BladeCount, 1, MaxBlades);

	for (int32 I = 0; I < Used; ++I)
	{
		UStaticMeshComponent* Blade = Blades.IsValidIndex(I) ? Blades[I].Get() : nullptr;
		UBoxComponent* Contact = Contacts.IsValidIndex(I) ? Contacts[I].Get() : nullptr;
		if (!Blade || !Contact) continue;

		TArray<AActor*> Inside;
		Contact->GetOverlappingActors(Inside, AGetawayCharacter::StaticClass());
		if (Inside.Num() == 0) continue;

		const FVector BladeAt = Blade->GetComponentLocation();
		const FVector Up = Blade->GetUpVector();
		// A plate never turns, so every point on it moves at the same velocity - its centre's.
		const FVector Sweep = FVector::CrossProduct(Omega, BladeAt - Centre);

		for (AActor* Actor : Inside)
		{
			AGetawayCharacter* Who = Cast<AGetawayCharacter>(Actor);
			if (!Who) continue;

			// Only the machine that owns this character throws it. No other machine is allowed to
			// move it under client authority anyway, and it has to happen on the frame the plate
			// caught you - the same bargain as the jump pad, the spikes and the C4 rocket jump.
			if (!Who->IsLocallyControlled()) continue;

			// A rider, not a victim. Two tests rather than one: the movement base is the honest
			// answer while you are standing, and the height test covers the frames around a jump,
			// when the base has been cleared but you are still inside the contact box.
			if (Who->GetMovementBaseObject() == Blade) continue;
			const float Feet = FVector::DotProduct(
				Who->GetActorLocation() - Up * Who->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - BladeAt,
				Up);
			if (Feet > BladeThickness * 0.5f - RiderSkin) continue;

			if (const float* Last = LastShoveTime.Find(Who))
			{
				if (Now - *Last < ShoveRetrigger) continue;
			}
			LastShoveTime.Add(Who, Now);

			UCharacterMovementComponent* Move = Who->GetCharacterMovement();
			UGetawayMovementComponent* GMove = Cast<UGetawayMovementComponent>(Move);
			if (GMove)
			{
				// ApplyBlast already knows how to drop you out of a wallrun or a mantle, and off
				// the ground when the kick points away from it, so the hit is expressed as the
				// change in velocity it wants - exactly as the jump pad and the C4 blast are.
				GMove->ApplyBlast(Sweep * ShoveScale);
			}
			else if (Move)
			{
				Move->Velocity += Sweep * ShoveScale;
			}

			Who->AddViewPunch(ShoveViewPunch);
			Who->AddCameraShake(ShoveShake);
			OnStruck.Broadcast(this, Who);
		}
	}
}
