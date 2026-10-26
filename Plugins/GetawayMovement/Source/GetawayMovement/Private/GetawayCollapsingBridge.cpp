#include "GetawayCollapsingBridge.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/DamageType.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** How tall a slab of air above the deck counts as "standing on it" (cm). */
	constexpr float StandProbeHeight = 20.f;
	/** How often that probe runs (s). At 800 cm/s a player crosses 80 cm between checks. */
	constexpr float StandProbeInterval = 0.1f;
}

AGetawayCollapsingBridge::AGetawayCollapsingBridge()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Mesh->SetMobility(EComponentMobility::Movable); // it has to be able to fall
	Mesh->SetSimulatePhysics(false);
	Mesh->SetNotifyRigidBodyCollision(false);

	// Replicated so a bridge somebody places exists for everyone, and so the collapse reaches them.
	// Movement is deliberately *not* replicated - see StartFalling for why.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}
}

void AGetawayCollapsingBridge::BeginPlay()
{
	Super::BeginPlay();
	StartLocation = GetActorLocation();
	StartRotation = GetActorRotation();
}

void AGetawayCollapsingBridge::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGetawayCollapsingBridge, State);
}

void AGetawayCollapsingBridge::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// The rattle is cosmetic and runs wherever the bridge is about to go, so the warning reaches
	// both screens. Stepping off does not save it, as in the lab.
	if (State == EGetawayBridgeState::Warning && WarnShake > 0.f)
	{
		const FVector Jitter(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f));
		SetActorLocation(StartLocation + Jitter * WarnShake);
	}

	// Parking is each machine's own business, for the same reason the rattle is: movement is not
	// replicated, so every copy falls on its own and each one decides when its copy has dropped
	// far enough to be out of the way. Hence above the authority check, not below it.
	if (State == EGetawayBridgeState::Fallen && !bParked && CleanupDrop > 0.f
		&& GetActorLocation().Z < StartLocation.Z - CleanupDrop)
	{
		Park();
	}

	// Nothing below is a client's to decide. The server is the only machine that sees every player,
	// and two copies polling independently would give way at different moments.
	if (!HasAuthority()) return;

	switch (State)
	{
	case EGetawayBridgeState::Idle:
		StandCheck -= DeltaSeconds;
		if (StandCheck <= 0.f)
		{
			StandCheck = StandProbeInterval;
			if (IsStoodOn())
			{
				State = EGetawayBridgeState::Warning;
				Timer = CollapseDelay;
				if (Timer <= 0.f)
				{
					Collapse();
				}
			}
		}
		break;

	case EGetawayBridgeState::Warning:
		Timer -= DeltaSeconds;
		if (Timer <= 0.f)
		{
			Collapse();
		}
		break;

	case EGetawayBridgeState::Fallen:
		// Normally nothing happens here at all: a fallen bridge stays fallen until the next round
		// calls Restore on it. bRespawns is off by default, and is only for a bridge standing in a
		// level with no round behind it.
		if (bRespawns)
		{
			Timer -= DeltaSeconds;
			if (Timer <= 0.f)
			{
				Restore();
			}
		}
		break;
	}
}

void AGetawayCollapsingBridge::Collapse()
{
	// The decision is the server's; every machine then drops its own copy off the replicated state.
	if (!HasAuthority() || State == EGetawayBridgeState::Fallen || !Mesh) return;

	State = EGetawayBridgeState::Fallen;
	Timer = RespawnDelay;
	StartFalling();

	UE_LOG(LogTemp, Log, TEXT("Getaway: bridge %s gave way"), *GetName());
}

void AGetawayCollapsingBridge::Restore()
{
	if (!HasAuthority())
	{
		// Not expected - the game mode restores on the server - but harmless and better than a
		// client left looking at a bridge the rest of the session can walk on.
		StopFalling();
		return;
	}

	State = EGetawayBridgeState::Idle;
	StandCheck = StandProbeInterval;
	Timer = 0.f;
	StopFalling();
}

void AGetawayCollapsingBridge::OnRep_State()
{
	switch (State)
	{
	case EGetawayBridgeState::Fallen:
		StartFalling();
		break;
	case EGetawayBridgeState::Idle:
		StopFalling();
		break;
	case EGetawayBridgeState::Warning:
		// Nothing to do: Tick picks the rattle up from the state itself.
		break;
	}
}

void AGetawayCollapsingBridge::StartFalling()
{
	if (!Mesh) return;

	// Undo the rattle first, so it starts falling from where it belongs rather than from wherever
	// the last jitter frame left it - which is also what keeps the two copies starting together.
	SetActorLocation(StartLocation);
	Mesh->SetSimulatePhysics(true);

	if (TipSpeed > 0.f)
	{
		// A kick straight down at the far end, which is both a push and a torque: the nose drops and
		// the near end swings up, roughly the lab's hinged fall without a constraint to tune.
		const float Reach = GetActorScale3D().X * 50.f * 0.8f;
		const FVector FarEnd = GetActorLocation() + GetActorForwardVector() * Reach;
		Mesh->AddImpulseAtLocation(FVector(0.f, 0.f, -TipSpeed * Mesh->GetMass()), FarEnd);
	}

	OnCollapsed.Broadcast(this);
}

void AGetawayCollapsingBridge::Park()
{
	if (bParked || !Mesh) return;
	bParked = true;

	// Hidden and inert, but still here. Restore brings it back through StopFalling.
	Mesh->SetSimulatePhysics(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetVisibility(false);
}

void AGetawayCollapsingBridge::FellOutOfWorld(const UDamageType& DmgType)
{
	// A deck that stays down for the whole round has plenty of time to reach KillZ, and the engine
	// default there is Destroy() - which would take the bridge out of the level for good and leave
	// the next round with nothing to put back. Park it instead; the round rights it from
	// StartLocation either way.
	Park();
}

void AGetawayCollapsingBridge::StopFalling()
{
	if (!Mesh) return;

	if (bParked)
	{
		bParked = false;
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetVisibility(true);
	}
	Mesh->SetSimulatePhysics(false);
	SetActorLocation(StartLocation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(StartRotation, ETeleportType::TeleportPhysics);
}

bool AGetawayCollapsingBridge::IsStoodOn() const
{
	const UWorld* World = GetWorld();
	if (!World) return false;

	// The deck is the engine cube, so its half extent in cm is the actor scale x 50.
	const FVector Half = GetActorScale3D().GetAbs() * 50.f;
	if (Half.X < 1.f || Half.Y < 1.f) return false;

	// A thin slab sitting on the top face. Pulled in slightly so brushing past the side does not
	// count, and started just below the surface so a capsule resting on it is caught.
	const FVector Probe(FMath::Max(1.f, Half.X - 5.f), FMath::Max(1.f, Half.Y - 5.f), StandProbeHeight * 0.5f);
	const FVector Centre = GetActorLocation()
		+ GetActorUpVector() * (Half.Z + StandProbeHeight * 0.5f - 2.f);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GetawayBridgeStand), false, this);
	FCollisionObjectQueryParams Pawns(ECC_Pawn);
	return World->OverlapAnyTestByObjectType(Centre, GetActorQuat(), Pawns, FCollisionShape::MakeBox(Probe), Params);
}
