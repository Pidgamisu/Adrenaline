#include "GetawayVoid.h"

#include "GetawayCharacter.h"
#include "GetawayMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/*
	 * Prefixed "Void" because a unity build merges several .cpp files into one translation unit, so
	 * a name in an anonymous namespace still has to be unique across the module - the clash the
	 * speed ring hit when the build happened to put it in the same blob as the vortex.
	 */

	/** The field's radius before the catalog scales it, so the piece is 100 local units across. */
	constexpr float VoidLocalRadius = 50.f;
	/** The cylinder and sphere meshes are 100 units across, centred - the same assumption every other piece makes. */
	constexpr float VoidMeshLocal = 100.f;
	/** Pooled in the constructor and the spares hidden, exactly as the vortex pools its bars. */
	constexpr int32 VoidMaxRings = 5;
	constexpr int32 VoidMaxSegments = 16;
}

AGetawayVoid::AGetawayVoid()
{
	PrimaryActorTick.bCanEverTick = true;

	// Authored inside a 100 cm sphere so the catalog can scale it like every other piece: at the
	// catalog's 500 cubed the actor scale is (5, 5, 5), and everything below rides it.
	//
	// A sphere rather than the usual box, because this is the one piece whose shape *is* the rule:
	// the pull points at the centre and the slingshot is measured from it, so a box trigger would
	// switch the field on in the corners, over a metre outside anything you can see.
	Field = CreateDefaultSubobject<USphereComponent>(TEXT("Field"));
	Field->InitSphereRadius(VoidLocalRadius);
	Field->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Field->SetCollisionObjectType(ECC_WorldDynamic);
	Field->SetCollisionResponseToAllChannels(ECR_Ignore);
	Field->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Field->SetGenerateOverlapEvents(true);
	RootComponent = Field;

	// Everything you can see hangs off this, and this is what spins. The field has to stay put: it
	// is what decides who is inside, and turning it would be busywork with no effect anyway.
	Shell = CreateDefaultSubobject<USceneComponent>(TEXT("Shell"));
	Shell->SetupAttachment(Field);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));

	// The graze ball, before the core so the core draws inside it. Translucent lime: this is the
	// line you aim to brush and the one thing that makes the risk judgeable from outside.
	Danger = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Danger"));
	Danger->SetupAttachment(Field);
	Danger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Sphere.Succeeded())
	{
		Danger->SetStaticMesh(Sphere.Object);
	}

	// The lethal middle. Small, solid and visible through the cage: the shell tells you how far the
	// field reaches, the graze ball tells you where the risk starts and this tells you what kills.
	Core = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Core"));
	Core->SetupAttachment(Field);
	Core->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Sphere.Succeeded())
	{
		Core->SetStaticMesh(Sphere.Object);
	}

	// There is no sphere wireframe in the engine's BasicShapes set and this plugin ships no
	// content, so the shell is a cage of stretched cylinders laid out as latitude rings - the same
	// answer as the vortex's bars, the speed ring's rim and the ramp's hand-built wedge. Dashed
	// rather than solid, so the spin is visible, and open enough to see the course through.
	for (int32 Ring = 0; Ring < VoidMaxRings; ++Ring)
	{
		for (int32 I = 0; I < VoidMaxSegments; ++I)
		{
			const FString Name = FString::Printf(TEXT("Shell%d_%d"), Ring, I);
			UStaticMeshComponent* Bar = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
			Bar->SetupAttachment(Shell);
			// Nothing on this piece is solid. Being stopped dead by the edge of a hazard you were
			// trying to skim past would be worse than dying in it.
			Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (Cylinder.Succeeded())
			{
				Bar->SetStaticMesh(Cylinder.Object);
			}
			ShellBars.Add(Bar);
		}
	}

	// Replicated so a void somebody places exists for everyone - the lesson of 2026-10-18. It never
	// moves, like AGetawayPiece and AGetawayVortex, so movement replication stays off; the spin is
	// cosmetic and every machine works it out for itself.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
}

void AGetawayVoid::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Runs on every machine, because the scale travels in the spawn transform - the same trick
	// AGetawayVortex, AGetawaySpeedRing and AGetawayJumpPad use to build themselves with no mesh
	// going over the wire.
	LayOutShell();
}

void AGetawayVoid::BeginPlay()
{
	Super::BeginPlay();
	LayOutShell();
}

float AGetawayVoid::GetFieldRadius() const
{
	// Straight off the component, so the distance the slingshot is measured over is exactly the
	// volume that decided you were inside it. A sphere component takes the smallest of the three
	// scale axes, which is also why a void scaled unevenly is still round.
	return Field ? Field->GetScaledSphereRadius() : VoidLocalRadius;
}

float AGetawayVoid::GetSlingFraction(float Closest) const
{
	// The risk curve, and the whole bargain of the piece: nothing for drifting through the outside,
	// everything for brushing the core. Linear on purpose - a curve here would be one more thing to
	// learn on top of judging the distance, and judging the distance is the skill.
	const float Reach = FMath::Max(1.f, KillRadius);
	const float Span = FMath::Max(1.f, GetFieldRadius() - Reach);
	return FMath::Clamp(1.f - (Closest - Reach) / Span, 0.f, 1.f);
}

void AGetawayVoid::LayOutShell()
{
	const FVector Scale = GetActorScale3D().GetAbs();
	// One number for the whole piece, matching what the sphere component itself does with scale.
	// A void is round whatever the catalog says, so there is no second axis to keep track of.
	const float Safe = FMath::Max(KINDA_SMALL_NUMBER, Scale.GetMin());

	if (Core)
	{
		Core->SetMaterial(0, CoreMaterial);
		// World radius KillRadius, so what you can see is exactly what kills you. The mesh is 100
		// units across, hence the half.
		Core->SetRelativeScale3D(FVector(KillRadius / (VoidMeshLocal * 0.5f * Safe)));
		Core->SetRelativeLocation(FVector::ZeroVector);
	}

	if (Danger)
	{
		Danger->SetMaterial(0, DangerMaterial);
		const bool bShow = bShowDangerLine && DangerRingScale > 1.f;
		Danger->SetVisibility(bShow);
		// Held inside the shell whatever the numbers say, so a big DangerRingScale cannot swallow
		// the cage and leave the piece unreadable from outside.
		const float World = FMath::Min(KillRadius * DangerRingScale, VoidLocalRadius * Safe * 0.9f);
		Danger->SetRelativeScale3D(FVector(World / (VoidMeshLocal * 0.5f * Safe)));
		Danger->SetRelativeLocation(FVector::ZeroVector);
	}

	const int32 Rings = FMath::Clamp(ShellRings, 1, VoidMaxRings);
	const int32 PerRing = FMath::Clamp(ShellSegments, 4, VoidMaxSegments);
	const float Fill = FMath::Clamp(SegmentFill, 0.2f, 1.f);

	for (int32 Ring = 0; Ring < VoidMaxRings; ++Ring)
	{
		// Latitudes spread evenly over the sphere, poles excluded: with five rings that is 30, 60,
		// 90, 120 and 150 degrees, so the widest sits on the equator and the outer two read as the
		// curve closing toward the top and bottom. Any fewer and it stops looking spherical, which
		// is why ShellRings clamps where it does.
		const float Lat = PI * static_cast<float>(Ring + 1) / static_cast<float>(Rings + 1);
		// Local units, so these grow with the piece - unlike the thickness below, which must not.
		const float RingRadius = VoidLocalRadius * FMath::Sin(Lat);
		const float RingZ = VoidLocalRadius * FMath::Cos(Lat);
		const float DashLocal = FMath::Max(1.f,
			2.f * PI * RingRadius * Fill / static_cast<float>(PerRing));

		for (int32 I = 0; I < VoidMaxSegments; ++I)
		{
			UStaticMeshComponent* Bar = ShellBars[Ring * VoidMaxSegments + I];
			if (!Bar) continue;

			// Every dash, hidden spares included, and unconditionally, so clearing the property in
			// the Details panel puts the engine default back.
			Bar->SetMaterial(0, ShellMaterial);

			if (Ring >= Rings || I >= PerRing)
			{
				Bar->SetVisibility(false);
				continue;
			}

			const float Angle = 2.f * PI * static_cast<float>(I) / static_cast<float>(PerRing);
			Bar->SetVisibility(true);

			// The cylinder's long axis is its own Z, so point that along the tangent of the ring at
			// this angle and the dash lies along the circle. A void is always uniformly scaled in
			// practice, which is what lets this be a plain rotation with no shear to worry about.
			const FVector Tangent(-FMath::Sin(Angle), FMath::Cos(Angle), 0.f);
			Bar->SetRelativeRotation(FRotationMatrix::MakeFromZ(Tangent).Rotator());
			// Thickness stays the same in the world however the piece is scaled, by dividing the
			// actor scale back out; the dash length does not, because a bigger sphere wants longer
			// dashes rather than more of them.
			Bar->SetRelativeScale3D(FVector(
				RingThickness / (VoidMeshLocal * Safe),
				RingThickness / (VoidMeshLocal * Safe),
				DashLocal / VoidMeshLocal));
			Bar->SetRelativeLocation(FVector(
				RingRadius * FMath::Cos(Angle),
				RingRadius * FMath::Sin(Angle),
				RingZ));
		}
	}
}

void AGetawayVoid::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The spin is the only thing that says the piece is live, so it stops when the round does. Slow
	// on purpose: a void is something you watch warily, not a machine advertising itself.
	if (bArmed && Shell && !FMath::IsNearlyZero(SpinRate))
	{
		SpinAngle = FMath::Fmod(SpinAngle + SpinRate * DeltaSeconds, 360.f);
		Shell->SetRelativeRotation(FRotator(0.f, SpinAngle, 0.f));
	}

	PullRiders(DeltaSeconds);
}

void AGetawayVoid::SetArmed(bool bNew)
{
	bArmed = bNew;
	// Each round starts clean, so the field takes hold again - camera kick and all - on the first
	// frame of the new run even if somebody was standing in it when the last one ended. It also
	// drops every visit, which stops a stale point from last round being used as the start of a
	// swept kill test and stops a slingshot being paid for a graze nobody made this round.
	Visits.Reset();
}

/* ============================================================ the field */

void AGetawayVoid::PullRiders(float)
{
	if (!bArmed)
	{
		Visits.Reset();
		return;
	}

	TArray<AActor*> Inside;
	Field->GetOverlappingActors(Inside, AGetawayCharacter::StaticClass());

	const FVector Centre = GetCoreLocation();
	const float Radius = FMath::Max(KINDA_SMALL_NUMBER, GetFieldRadius());
	const float Reach = FMath::Max(1.f, KillRadius);

	// Rebuilt from the engine's own overlap set every tick rather than kept up to date from begin
	// and end overlap, exactly as the vortex does: a player who dies, respawns or is teleported out
	// of the field then needs no bookkeeping, and neither does a void disarmed mid-round.
	TMap<TWeakObjectPtr<AGetawayCharacter>, FVisit> StillInside;
	StillInside.Reserve(Inside.Num());

	// Whoever the core ate this frame. They are out of the field, but they must not be paid a
	// slingshot on the way out - the whole point is that cutting too close costs you the run.
	TSet<TWeakObjectPtr<AGetawayCharacter>> Eaten;

	for (AActor* Actor : Inside)
	{
		AGetawayCharacter* Who = Cast<AGetawayCharacter>(Actor);
		if (!Who) continue;

		// Only the machine that owns this character pulls, kills or slings it. No other machine is
		// allowed to move it under client authority anyway, and it has to happen on the frame you
		// are in the field - the same bargain as dying on spikes, the jump pad and the rocket jump.
		if (!Who->IsLocallyControlled()) continue;

		const FVector Here = Who->GetActorLocation();
		const FVisit* Before = Visits.Find(Who);

		// The swept distance to the core: the closest the line from last frame to this one comes to
		// the middle. It does two jobs at once. It is the kill test, because a point sample would
		// let anything arriving at pad or ring speed cross the whole core between two frames and
		// live. And it is the slingshot's measure, because the fastest and best-paid passes are
		// exactly the ones that would skip over the core between frames.
		const float Closest = Before
			? FMath::PointDistToSegment(Centre, Before->Last, Here)
			: FVector::Dist(Centre, Here);

		if (Closest <= Reach)
		{
			OnConsumed.Broadcast(this, Who);
			// The same death the spikes deal, so respawning, the HUD and the score all behave as
			// they already do. Not recorded in StillInside: this character is on its way out, and
			// its replacement should be treated as somebody new arriving.
			Eaten.Add(Who);
			Who->Die(this);
			continue;
		}

		FVisit Visit;
		Visit.Last = Here;
		Visit.Closest = Before ? FMath::Min(Before->Closest, Closest) : Closest;
		StillInside.Add(Who, Visit);

		UGetawayMovementComponent* Move = Cast<UGetawayMovementComponent>(Who->GetCharacterMovement());
		if (!Move) continue;

		const FVector ToCentre = Centre - Here;
		const float Distance = ToCentre.Size();
		if (Distance <= KINDA_SMALL_NUMBER) continue;
		const FVector Dir = ToCentre / Distance;

		// EdgePullScale at the rim rising to full strength at the centre - and 1 by default since
		// the slingshot redesign, so the field is uniform. It used to be 0.25, which made the pull
		// strengthen the deeper you went: a funnel, where the further in you were the less you
		// could do about it. Uniform is the one you can learn to judge.
		const float Depth = FMath::Clamp(1.f - Distance / Radius, 0.f, 1.f);
		const float Strength = FMath::Lerp(FMath::Clamp(EdgePullScale, 0.f, 1.f), 1.f, Depth);

		FVector Accel = Dir * PullAccel * Strength;
		if (bFlatPullOnGround && Move->IsMovingOnGround())
		{
			/*
			 * Standing on something: the pull is turned along the floor, keeping its full strength
			 * but never lifting you. Peeling somebody off the ground takes their wallrun, their
			 * slide and their bunny hop all at once, and those are the game - so on the ground a
			 * void is a drag you lean against, and the curving is saved for the air.
			 *
			 * **Turned, not thrown away.** Zeroing Z was half of why the piece once played as dead:
			 * a void on the floor has its core well above you, so the pull on a runner at ground
			 * level points steeply upward and dropping that left a sliver of the field.
			 *
			 * Directly under or over the core there is no flat direction left, so the field does
			 * nothing - which is correct: straight up is exactly the part the floor is cancelling.
			 */
			const float Magnitude = Accel.Size();
			Accel = FVector(Accel.X, Accel.Y, 0.f).GetSafeNormal() * Magnitude;
		}

		// Offered, not applied: the movement component takes it at the top of its own update, where
		// the jump and the vortex lift already write velocity, and drops it a frame after the
		// offers stop coming - so leaving a void needs no end-overlap bookkeeping.
		Move->AddVoidPull(Accel, MaxPullSpeed);

		if (!Before)
		{
			Who->AddViewPunch(EnterViewPunch);
			Who->AddCameraShake(EnterShake);
			OnEntered.Broadcast(this, Who);
		}
	}

	// The slingshot, paid on the way out. Anybody in last frame's visits who is not in this
	// frame's has left the field, and the diff is the only "end overlap" this piece needs - which
	// also means a void disarmed or destroyed under somebody simply never pays, rather than paying
	// at a strange moment.
	if (bSlingshot)
	{
		for (const TPair<TWeakObjectPtr<AGetawayCharacter>, FVisit>& Gone : Visits)
		{
			if (StillInside.Contains(Gone.Key) || Eaten.Contains(Gone.Key)) continue;
			if (AGetawayCharacter* Who = Gone.Key.Get())
			{
				PaySlingshot(Who, Gone.Value.Closest);
			}
		}
	}

	Visits = MoveTemp(StillInside);
}

void AGetawayVoid::PaySlingshot(AGetawayCharacter* Who, float Closest)
{
	if (!Who || !Who->IsLocallyControlled()) return;

	const float T = GetSlingFraction(Closest);
	if (T <= KINDA_SMALL_NUMBER) return;

	UGetawayMovementComponent* Move = Cast<UGetawayMovementComponent>(Who->GetCharacterMovement());
	if (!Move) return;

	/*
	 * Both of the project's own levers for handing somebody speed, and neither of them is a shove.
	 *
	 * The ceiling goes up (AddSpeedBoost), because Source friction eats a velocity kick in about a
	 * second - the decision recorded as *A speed boost raises your ceiling, it does not shove you*.
	 * And the drag comes down (AddLowFriction), because the well has already given you the velocity
	 * and the first touchdown would otherwise take it straight back - the jump pad's lesson.
	 *
	 * No snap, hence the 0: the pull spent your whole visit bending your line, so the speed is
	 * already yours and pointing where the orbit left you. Snapping would redirect a slingshot,
	 * which is the one thing it must not do.
	 */
	Move->AddSpeedBoost(FMath::Lerp(1.f, FMath::Max(1.f, SlingBoostScale), T), SlingTime, 0.f);
	Move->AddLowFriction(FMath::Lerp(1.f, FMath::Clamp(SlingFrictionScale, 0.f, 1.f), T), SlingTime);

	// Scaled by the graze, so the kick itself tells you how well you cut it before the speedometer
	// does. A rim-brush is almost nothing; a core graze is the biggest punch any piece gives.
	Who->AddViewPunch(SlingViewPunch * T);
	Who->AddCameraShake(SlingShake * T);
	OnSlung.Broadcast(this, Who);
}
