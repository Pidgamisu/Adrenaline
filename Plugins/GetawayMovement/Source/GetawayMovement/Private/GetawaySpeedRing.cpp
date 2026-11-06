#include "GetawaySpeedRing.h"

#include "GetawayCharacter.h"
#include "GetawayMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Half extent of the trigger before the catalog scales it, so the piece is 100 local units. */
	constexpr float HalfLocal = 50.f;
	/** The cylinder mesh is 100 units across and 100 tall, centred - the same assumption the vortex makes. */
	constexpr float CylinderLocal = 100.f;
	/** Segments are pooled in the constructor and the spares hidden, exactly as the vortex pools its bars. */
	constexpr int32 MaxSegments = 24;
	/** World gap between the outside of the rim and the edge of the piece's own box. */
	constexpr float EdgeGap = 2.f;
}

AGetawaySpeedRing::AGetawaySpeedRing()
{
	PrimaryActorTick.bCanEverTick = true;

	// Authored inside a 100 cm cube so the catalog can scale it like every other piece: at the
	// catalog's 100 x 300 x 300 the actor scale is (1, 3, 3), and everything below rides it.
	//
	// The box is resized in LayOutRing to the inside of the rim, so what you have to hit is the
	// hole rather than the piece. It starts life filling the cube in case anything reads it before
	// OnConstruction runs.
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(HalfLocal, HalfLocal, HalfLocal));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	// Everything you can see hangs off this, and this is what spins. The trigger has to stay put:
	// it is a box, so turning it would change the footprint the ring actually catches you in.
	Ring = CreateDefaultSubobject<USceneComponent>(TEXT("Ring"));
	Ring->SetupAttachment(Trigger);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// There is no torus in the engine's BasicShapes set and this plugin ships no content, so the
	// hoop is a ring of stretched cylinders - the same answer as the vortex's cage of bars and the
	// ramp's hand-built wedge. Dashed rather than solid, so the spin is visible.
	for (int32 I = 0; I < MaxSegments; ++I)
	{
		const FString Name = FString::Printf(TEXT("Segment%d"), I);
		UStaticMeshComponent* Segment = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
		Segment->SetupAttachment(Ring);
		// The rim is not solid. Clipping the edge of a hoop at 1200 cm/s and being stopped by it
		// would be the worst thing in the game, so the piece has no blocking collision anywhere.
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Cylinder.Succeeded())
		{
			Segment->SetStaticMesh(Cylinder.Object);
		}
		Segments.Add(Segment);
	}

	// Replicated so a ring somebody places exists for everyone - the lesson of 2026-10-18. It never
	// moves, like AGetawayPiece and AGetawayVortex, so movement replication stays off; the spin is
	// cosmetic and every machine works it out for itself.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
}

void AGetawaySpeedRing::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Runs on every machine, because the scale travels in the spawn transform - the same trick
	// AGetawayVortex, AGetawayJumpPad and AGetawayRamp use to build themselves with no mesh going
	// over the wire.
	LayOutRing();
}

void AGetawaySpeedRing::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGetawaySpeedRing::OnTriggerOverlap);
	LayOutRing();
}

void AGetawaySpeedRing::LayOutRing()
{
	const FVector Scale = GetActorScale3D().GetAbs();
	const float SafeX = FMath::Max(KINDA_SMALL_NUMBER, Scale.X);
	const float SafeY = FMath::Max(KINDA_SMALL_NUMBER, Scale.Y);
	const float SafeZ = FMath::Max(KINDA_SMALL_NUMBER, Scale.Z);

	// The hoop is circular in world space, laid out in the piece's own YZ plane - its +X is the
	// axis of the hole. The centreline radius is pulled in far enough that the outside of the rim
	// still sits inside the box the catalog gave the piece, so a 300 cm ring really is 300 cm
	// across and its lowest point rests on the floor it is standing on.
	const float Outer = FMath::Min(HalfLocal * SafeY, HalfLocal * SafeZ);
	const float Radius = FMath::Max(10.f, Outer - RingThickness * 0.5f - EdgeGap);

	// The hole, which is the rest of the piece. Inset to the inside of the rim: a box's inscribed
	// circle is the hole exactly, and its corners reach a little past the rim, which errs on the
	// forgiving side - the same bargain as the wallrun's generous attach.
	const float Inner = FMath::Max(10.f, Radius - RingThickness * 0.5f);
	if (Trigger)
	{
		Trigger->SetBoxExtent(FVector(HalfLocal, Inner / SafeY, Inner / SafeZ));
	}

	// Thickness and dash length are the same in the world however the piece is scaled, by dividing
	// the actor scale back out - the same trick as the vortex's bars and the jump pad's disc. A
	// ring whose Y and Z sizes differ gets a slightly uneven rim, because one scale has to stand
	// for both directions in the ring plane; the hoop itself stays circular either way.
	const float SPlane = FMath::Min(SafeY, SafeZ);
	const int32 Used = FMath::Clamp(SegmentCount, 3, MaxSegments);
	const float Arc = 2.f * PI * Radius / static_cast<float>(Used);
	const float DashLength = FMath::Max(1.f, Arc * FMath::Clamp(SegmentFill, 0.2f, 1.f));

	for (int32 I = 0; I < Segments.Num(); ++I)
	{
		UStaticMeshComponent* Segment = Segments[I];
		if (!Segment) continue;

		// Every segment, hidden spares included, and unconditionally, so clearing the property in
		// the Details panel puts the engine default back.
		Segment->SetMaterial(0, RingMaterial);

		if (I >= Used)
		{
			Segment->SetVisibility(false);
			continue;
		}

		const float Angle = 2.f * PI * static_cast<float>(I) / static_cast<float>(Used);
		Segment->SetVisibility(true);

		// Rolled about the piece's +X by the same angle that placed it, which points the cylinder's
		// own long axis along the tangent of the circle at that point. Roll only, so the mesh's X
		// stays along the piece's X and the non-uniform catalog scale cannot shear it.
		Segment->SetRelativeRotation(FRotator(0.f, 0.f, FMath::RadiansToDegrees(Angle)));
		Segment->SetRelativeScale3D(FVector(
			RingThickness / (CylinderLocal * SafeX),
			RingThickness / (CylinderLocal * SPlane),
			DashLength / (CylinderLocal * SPlane)));
		Segment->SetRelativeLocation(FVector(
			0.f,
			Radius * FMath::Cos(Angle) / SafeY,
			Radius * FMath::Sin(Angle) / SafeZ));
	}
}

void AGetawaySpeedRing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The spin is the only thing that says the piece is live, so it stops when the round does. It
	// turns about the axis of the hole, which is what the gaps between the segments make readable.
	if (bArmed && Ring && !FMath::IsNearlyZero(SpinRate))
	{
		SpinAngle = FMath::Fmod(SpinAngle + SpinRate * DeltaSeconds, 360.f);
		Ring->SetRelativeRotation(FRotator(0.f, 0.f, SpinAngle));
	}
}

FVector AGetawaySpeedRing::GetGateAxis() const
{
	// Placement lined the piece's +X up with the way you were looking, so this is the way the hoop
	// faces. It is the gap you have to fly through, not a direction the ring pushes you in.
	return GetActorQuat().GetAxisX().GetSafeNormal();
}

void AGetawaySpeedRing::SetArmed(bool bNew)
{
	bArmed = bNew;
	// Each round starts clean, so a ring that boosted you at the end of the last one can boost you
	// again on the first frame of this one.
	LastBoostTime.Reset();
}

/* ============================================================ the boost */

void AGetawaySpeedRing::OnTriggerOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
{
	if (!bArmed) return;

	AGetawayCharacter* Who = Cast<AGetawayCharacter>(OtherActor);
	if (!Who) return;

	// Only the machine that owns this character boosts it. Under client authority no other machine
	// is allowed to move it anyway, and it has to happen on the frame you went through rather than
	// a round trip later - the same bargain as the jump pad, dying on spikes and the rocket jump.
	if (!Who->IsLocallyControlled()) return;

	const UWorld* World = GetWorld();
	if (!World) return;

	const float Now = World->GetTimeSeconds();
	if (const float* Last = LastBoostTime.Find(Who))
	{
		if (Now - *Last < Retrigger) return;
	}
	LastBoostTime.Add(Who, Now);

	UGetawayMovementComponent* Move = Cast<UGetawayMovementComponent>(Who->GetCharacterMovement());
	if (!Move) return;

	// Handed over once and then the character's own clock runs it, because a ring is a gate: a
	// frame later there is nothing overlapping you to keep offering it, which is exactly what makes
	// the boost outlive the piece and carry into the jump or the slide that comes next.
	Move->AddSpeedBoost(BoostScale, BoostTime, bSnapToSpeed ? SnapMinSpeed : 0.f);

	Who->AddViewPunch(BoostViewPunch);
	Who->AddCameraShake(BoostShake);
	OnBoosted.Broadcast(this, Who);
}
