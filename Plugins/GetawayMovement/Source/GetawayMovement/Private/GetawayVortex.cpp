#include "GetawayVortex.h"

#include "GetawayCharacter.h"
#include "GetawayMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Half extent of the trigger, so the piece is 100 local units across before the catalog scales it. */
	constexpr float HalfLocal = 50.f;
	/** The cylinder mesh is 100 units across and 100 tall, centred - the same assumption the jump pad makes. */
	constexpr float CylinderLocal = 100.f;
	/** Where round the edge the bars stand. 46 of 50 leaves the outer face of a bar flush with the column. */
	constexpr float BarRingLocal = 46.f;
	/** Bars are pooled in the constructor and the spares hidden, exactly as AGetawaySpikes pools its points. */
	constexpr int32 MaxBars = 12;
	/** World gap under the bottom disc, so it does not z-fight the floor the column stands on. */
	constexpr float SurfaceGap = 2.f;
}

AGetawayVortex::AGetawayVortex()
{
	PrimaryActorTick.bCanEverTick = true;

	// Authored inside a 100 cm cube so the catalog can scale it like every other piece: at the
	// catalog's 300 x 300 x 500 the actor scale is (3, 3, 5), and everything below rides it.
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(HalfLocal, HalfLocal, HalfLocal));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	// Everything you can see hangs off this, and this is what spins. The trigger has to stay put:
	// it is a box, so turning it would change the footprint the column actually catches you in.
	Column = CreateDefaultSubobject<USceneComponent>(TEXT("Column"));
	Column->SetupAttachment(Trigger);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(Column);
	Base->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cylinder.Succeeded())
	{
		Base->SetStaticMesh(Cylinder.Object);
	}

	Cap = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cap"));
	Cap->SetupAttachment(Column);
	Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cylinder.Succeeded())
	{
		Cap->SetStaticMesh(Cylinder.Object);
	}

	// A cage of thin bars rather than a solid column, because you have to be able to see the course
	// through it - and the plugin ships no content, so there is no translucent material to use.
	for (int32 I = 0; I < MaxBars; ++I)
	{
		const FString Name = FString::Printf(TEXT("Bar%d"), I);
		UStaticMeshComponent* Bar = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
		Bar->SetupAttachment(Column);
		Bar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Cylinder.Succeeded())
		{
			Bar->SetStaticMesh(Cylinder.Object);
		}
		Bars.Add(Bar);
	}

	// Replicated so a column somebody places exists for everyone - the lesson of 2026-10-18. It
	// never moves, like AGetawayPiece, so movement replication stays off; the spin is cosmetic and
	// every machine works it out for itself.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
}

void AGetawayVortex::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Runs on every machine, because the scale travels in the spawn transform - the same trick
	// AGetawayJumpPad, AGetawaySpikes and AGetawayRamp use to build themselves with no mesh going
	// over the wire.
	LayOutColumn();
}

void AGetawayVortex::BeginPlay()
{
	Super::BeginPlay();
	LayOutColumn();
}

void AGetawayVortex::LayOutColumn()
{
	const FVector Scale = GetActorScale3D().GetAbs();
	const float SafeX = FMath::Max(KINDA_SMALL_NUMBER, Scale.X);
	const float SafeY = FMath::Max(KINDA_SMALL_NUMBER, Scale.Y);
	const float SafeZ = FMath::Max(KINDA_SMALL_NUMBER, Scale.Z);

	if (Column)
	{
		// Upright whatever the piece is sitting on. Placement lies a piece down on a sloped face,
		// and a vortex lifts straight up, so a tilted cage would be telling you the wrong thing.
		Column->SetWorldRotation(FRotator(0.f, GetActorRotation().Yaw + SpinAngle, 0.f));
	}

	// Keep the bars and the discs the same thickness in the world however the piece is scaled, by
	// dividing the actor scale back out - the same trick as the jump pad's disc.
	const float DiscLocalZ = DiscThickness / (CylinderLocal * SafeZ);
	const float DiscHalfLocal = DiscLocalZ * CylinderLocal * 0.5f;

	if (Base)
	{
		Base->SetRelativeScale3D(FVector(0.92f, 0.92f, DiscLocalZ));
		Base->SetRelativeLocation(FVector(0.f, 0.f, -HalfLocal + DiscHalfLocal + SurfaceGap / SafeZ));
	}
	if (Cap)
	{
		Cap->SetRelativeScale3D(FVector(0.92f, 0.92f, DiscLocalZ));
		Cap->SetRelativeLocation(FVector(0.f, 0.f, HalfLocal - DiscHalfLocal));
	}

	const int32 Used = FMath::Clamp(BarCount, 0, MaxBars);
	for (int32 I = 0; I < Bars.Num(); ++I)
	{
		UStaticMeshComponent* Bar = Bars[I];
		if (!Bar) continue;

		if (I >= Used)
		{
			Bar->SetVisibility(false);
			continue;
		}

		const float Angle = 2.f * PI * static_cast<float>(I) / static_cast<float>(FMath::Max(1, Used));
		Bar->SetVisibility(true);
		// Z scale 1 is exactly the height of the local cube, so a bar always spans the whole column.
		Bar->SetRelativeScale3D(FVector(
			BarThickness / (CylinderLocal * SafeX),
			BarThickness / (CylinderLocal * SafeY),
			1.f));
		Bar->SetRelativeLocation(FVector(
			BarRingLocal * FMath::Cos(Angle),
			BarRingLocal * FMath::Sin(Angle),
			0.f));
	}
}

void AGetawayVortex::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The spin is the only thing that says the piece is live, so it stops when the round does.
	if (bArmed && Column && !FMath::IsNearlyZero(SpinRate))
	{
		SpinAngle = FMath::Fmod(SpinAngle + SpinRate * DeltaSeconds, 360.f);
		Column->SetWorldRotation(FRotator(0.f, GetActorRotation().Yaw + SpinAngle, 0.f));
	}

	LiftRiders(DeltaSeconds);
}

FVector AGetawayVortex::GetLiftDirection() const
{
	// Straight up, always. The cage is kept upright to match, even on a piece that placement lay
	// down on a slope.
	return FVector::UpVector;
}

void AGetawayVortex::SetArmed(bool bNew)
{
	bArmed = bNew;
	// Each round starts clean, so the column catches you - camera kick and all - on the first frame
	// of the new run even if you were standing in it when the last one ended.
	Riding.Reset();
}

/* ============================================================ the lift */

void AGetawayVortex::LiftRiders(float)
{
	if (!bArmed)
	{
		Riding.Reset();
		return;
	}

	TArray<AActor*> Inside;
	Trigger->GetOverlappingActors(Inside, AGetawayCharacter::StaticClass());

	// Rebuilt from the engine's own overlap set every tick rather than kept up to date from begin
	// and end overlap: a character who dies, respawns or is teleported out of the column then needs
	// no bookkeeping, and neither does a column disarmed while somebody is riding it.
	TSet<TWeakObjectPtr<AGetawayCharacter>> StillInside;
	StillInside.Reserve(Inside.Num());

	for (AActor* Actor : Inside)
	{
		AGetawayCharacter* Who = Cast<AGetawayCharacter>(Actor);
		if (!Who) continue;

		// Only the machine that owns this character lifts it. No other machine is allowed to move it
		// under client authority anyway, and it has to happen on the frame you are in the column -
		// the same bargain as the jump pad, dying on spikes and the C4 rocket jump.
		if (!Who->IsLocallyControlled()) continue;

		UGetawayMovementComponent* Move = Cast<UGetawayMovementComponent>(Who->GetCharacterMovement());
		if (!Move) continue;

		// Offered, not applied: the movement component takes it at the top of its own update, where
		// the jump and the double jump already write vertical speed, and drops it a frame after the
		// offers stop coming.
		Move->AddVortexLift(LiftSpeed, LiftAccel, SteerSpeed, SteerAccel, bBlocksWallrun);
		StillInside.Add(Who);

		if (!Riding.Contains(Who))
		{
			Who->AddViewPunch(LiftViewPunch);
			Who->AddCameraShake(LiftShake);
			OnEntered.Broadcast(this, Who);
		}
	}

	Riding = MoveTemp(StillInside);
}
