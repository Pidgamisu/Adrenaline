#include "GetawaySpikes.h"

#include "GetawayBreakable.h"
#include "GetawayCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Most points a plate can show along one side. The pool is this squared. */
	constexpr int32 MaxPointsPerSide = 4;
	/** Local half extent of the trigger, so the plate is 100 local units across before scaling. */
	constexpr float SpikePlateLocal = 100.f;
	/** Local Z of the top of the base plate; the points stand on it. */
	constexpr float SpikePlateTopLocal = -30.f;
}

AGetawaySpikes::AGetawaySpikes()
{
	PrimaryActorTick.bCanEverTick = false;

	// Authored inside a 100 cm cube so the catalog can scale it like every other piece: at the
	// catalog's 400 x 400 x 50 the actor scale is (4, 4, 0.5), and everything below rides it.
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(50.f, 50.f, 50.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cone(TEXT("/Engine/BasicShapes/Cone.Cone"));

	// The plate, lying on the bottom of the local cube - the face that ends up against the wall.
	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(Trigger);
	Base->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Base->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	Base->SetRelativeScale3D(FVector(1.f, 1.f, 0.2f));
	if (Cube.Succeeded())
	{
		Base->SetStaticMesh(Cube.Object);
	}

	// A fixed pool of points, laid out by OnConstruction once the plate's real size is known.
	// Components cannot be created there, so the most a plate can ever show is made here and the
	// spares are hidden. MaxPointsPerSide 4 covers the catalog's 400 cm at the default spacing.
	for (int32 I = 0; I < MaxPointsPerSide; ++I)
	{
		for (int32 J = 0; J < MaxPointsPerSide; ++J)
		{
			const FString Name = FString::Printf(TEXT("Spike_%d_%d"), I, J);
			UStaticMeshComponent* Spike = CreateDefaultSubobject<UStaticMeshComponent>(*Name);
			Spike->SetupAttachment(Trigger);
			Spike->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			if (Cone.Succeeded())
			{
				Spike->SetStaticMesh(Cone.Object);
			}
			Points.Add(Spike);
		}
	}

	// Replicated so a trap somebody places exists for everyone - the lesson of 2026-10-18, when the
	// bridge, the C4 and the pickup all turned out to be invisible to the other player.
	bReplicates = true;
	bAlwaysRelevant = true;
	// Movement replication on, unlike the other placed pieces, because an actor's attachment rides
	// on it: spikes stuck to a collapsing bridge have to come down on everyone's screen, not just
	// the server's. A plate that is not on something that moves costs nothing to replicate.
	SetReplicateMovement(true);
}

void AGetawaySpikes::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Runs on every machine, because the scale travels in the spawn transform - the same trick
	// AGetawayRamp uses to build its wedge without any of the mesh going over the wire.
	LayOutPoints();
}

void AGetawaySpikes::LayOutPoints()
{
	const FVector Scale = GetActorScale3D().GetAbs();
	const float WorldX = SpikePlateLocal * Scale.X;
	const float WorldY = SpikePlateLocal * Scale.Y;
	const float Spacing = FMath::Max(20.f, SpikeSpacing);

	// Count from the plate's real size, so a plate trimmed back to a ledge loses points rather
	// than ending up with the same nine squashed into a narrower strip.
	const int32 NX = FMath::Clamp(FMath::RoundToInt(WorldX / Spacing), 1, MaxPointsPerSide);
	const int32 NY = FMath::Clamp(FMath::RoundToInt(WorldY / Spacing), 1, MaxPointsPerSide);

	// Each point keeps the same world size whatever the plate is scaled to, by dividing the actor
	// scale back out. Without this a narrow plate would give flattened, wedge-shaped spikes.
	const FVector Rel(
		SpikeWidth / FMath::Max(1.f, SpikePlateLocal * Scale.X),
		SpikeWidth / FMath::Max(1.f, SpikePlateLocal * Scale.Y),
		SpikeHeight / FMath::Max(1.f, SpikePlateLocal * Scale.Z));
	const float LocalHeight = SpikeHeight / FMath::Max(KINDA_SMALL_NUMBER, Scale.Z);

	int32 Used = 0;
	for (int32 I = 0; I < NX; ++I)
	{
		for (int32 J = 0; J < NY; ++J)
		{
			if (!Points.IsValidIndex(Used)) break;
			UStaticMeshComponent* Spike = Points[Used++];
			if (!Spike) continue;

			const float LX = -SpikePlateLocal * 0.5f + (SpikePlateLocal / NX) * (I + 0.5f);
			const float LY = -SpikePlateLocal * 0.5f + (SpikePlateLocal / NY) * (J + 0.5f);
			Spike->SetRelativeScale3D(Rel);
			// The cone mesh is centred, so half its local height above the plate puts its base on it.
			Spike->SetRelativeLocation(FVector(LX, LY, SpikePlateTopLocal + LocalHeight * 0.5f));
			Spike->SetVisibility(true);
		}
	}
	for (; Used < Points.Num(); ++Used)
	{
		if (Points[Used]) Points[Used]->SetVisibility(false);
	}
}

void AGetawaySpikes::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGetawaySpikes::OnTriggerOverlap);
	// A client can get the actor before Host has arrived; OnRep_Host covers that case.
	BindToHost();
}

void AGetawaySpikes::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGetawaySpikes, Host);
}

/* ============================================================ stuck to something */

void AGetawaySpikes::AttachToHost(AActor* InHost)
{
	if (!HasAuthority()) return;

	Host = InHost;
	BindToHost();
	if (Host)
	{
		// Keeping the world transform matters: placement already worked out exactly where the plate
		// sits, and attaching must not move it. The attachment itself replicates, so a host that
		// moves carries the spikes on every machine without their transform going over the wire.
		AttachToActor(Host, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void AGetawaySpikes::BindToHost()
{
	AGetawayBreakable* Breakable = Cast<AGetawayBreakable>(Host);
	if (!Breakable) return;

	// AddUnique, because this can run twice on a client: once in BeginPlay and again when Host
	// replicates in. Both OnBroken and OnRestored fire on every machine, so the spikes keep step
	// with the panel without anything of their own going over the wire.
	Breakable->OnBroken.AddUniqueDynamic(this, &AGetawaySpikes::OnHostBroken);
	Breakable->OnRestored.AddUniqueDynamic(this, &AGetawaySpikes::OnHostRestored);
}

void AGetawaySpikes::OnHostBroken(AGetawayBreakable*)
{
	SetPresent(false);
}

void AGetawaySpikes::OnHostRestored(AGetawayBreakable*)
{
	SetPresent(true);
}

void AGetawaySpikes::SetPresent(bool bNew)
{
	if (bPresent == bNew) return;
	bPresent = bNew;

	SetActorHiddenInGame(!bPresent);
	Trigger->SetCollisionEnabled(bPresent ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void AGetawaySpikes::OnTriggerOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
{
	if (!bArmed || !bPresent) return;

	AGetawayCharacter* Victim = Cast<AGetawayCharacter>(OtherActor);
	if (!Victim) return;

	// Only the machine that owns the victim kills them, and it does it immediately. Dying is the
	// harshest thing that can happen to a run, so it has to answer to the player's own eyes rather
	// than arrive a round trip after they thought they had cleared it. The server is told, and is
	// what credits the kill and ends their round - the same bargain as AGetawayBreakable::Break.
	if (!Victim->IsLocallyControlled()) return;

	OnSpiked.Broadcast(this, Victim);
	Victim->Die(this);
}
