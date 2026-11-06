#include "GetawayGrapplePickup.h"

#include "GetawayCharacter.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** The cylinder mesh is 100 cm across and 100 tall at scale 1, centred on its middle. */
	constexpr float HookCylinderLocal = 100.f;
	/** The pool, fixed in the constructor. BendSegments picks how many of them are used. */
	constexpr int32 MaxBendSegments = 8;
	/**
	 * How far round the bend goes, in degrees, starting from the bottom of the shank. Past 180 is
	 * what curls the tip back up, which is what makes the silhouette read as a hook and not a J.
	 */
	constexpr float HookBendSweep = 210.f;
}

AGetawayGrapplePickup::AGetawayGrapplePickup()
{
	PrimaryActorTick.bCanEverTick = true;

	// Pawns only, and query only: running through it must never slow you down.
	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(110.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	// Replicated so a pickup somebody places exists for everyone. Only its existence: who has taken
	// it, and when it comes back, is each machine's own - see TryGive.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// The same 110 cm pad the C4 pickup stands on, so the two read as the same kind of thing.
	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	Pad->SetupAttachment(Trigger);
	Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pad->SetRelativeLocation(FVector(0.f, 0.f, 3.f));
	Pad->SetRelativeScale3D(FVector(1.1f, 1.1f, 0.06f));
	if (Cylinder.Succeeded())
	{
		Pad->SetStaticMesh(Cylinder.Object);
	}

	// Everything the hook is made of hangs off this, so one component bobs and spins and the shape
	// itself never has to know about either.
	Hook = CreateDefaultSubobject<USceneComponent>(TEXT("Hook"));
	Hook->SetupAttachment(Trigger);

	Shank = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Shank"));
	Shank->SetupAttachment(Hook);
	Shank->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cylinder.Succeeded())
	{
		Shank->SetStaticMesh(Cylinder.Object);
	}

	// Pooled in the constructor and the spares hidden, exactly as the vortex pools its bars: how
	// many components a Blueprint has is fixed when its class default object is made, so it cannot
	// depend on a property somebody might change in the Details panel.
	Bend.Reserve(MaxBendSegments);
	for (int32 I = 0; I < MaxBendSegments; ++I)
	{
		UStaticMeshComponent* Segment = CreateDefaultSubobject<UStaticMeshComponent>(
			FName(*FString::Printf(TEXT("Bend%d"), I)));
		Segment->SetupAttachment(Hook);
		Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Cylinder.Succeeded())
		{
			Segment->SetStaticMesh(Cylinder.Object);
		}
		Bend.Add(Segment);
	}

	LayOutHook();

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGetawayGrapplePickup::OnTriggerBeginOverlap);
}

void AGetawayGrapplePickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	LayOutHook();
}

void AGetawayGrapplePickup::LayOutHook()
{
	if (!Hook) return;

	// The frame: the shank spans Z from -ShankLength/2 to +ShankLength/2 about the Hook component,
	// so the whole thing bobs and spins about its own middle rather than about the top of the shank.
	const float Thick = FMath::Max(0.5f, HookThickness) / HookCylinderLocal;
	const float Shaft = FMath::Max(1.f, ShankLength);
	const float R = FMath::Max(1.f, BendRadius);
	const FVector Bottom(0.f, 0.f, -Shaft * 0.5f);

	Hook->SetRelativeLocation(FVector(0.f, 0.f, HookHeight));

	if (Shank)
	{
		Shank->SetMaterial(0, HookMaterial);
		Shank->SetRelativeLocation(FVector::ZeroVector);
		Shank->SetRelativeRotation(FRotator::ZeroRotator);
		Shank->SetRelativeScale3D(FVector(Thick, Thick, Shaft / HookCylinderLocal));
	}

	// The bend is chords of a circle whose centre sits one radius to the +X side of the bottom of
	// the shank, swept from straight down the shank round under it and back up - so the first point
	// is exactly where the shank ends and no join has to be fudged.
	const int32 Used = FMath::Clamp(BendSegments, 1, MaxBendSegments);
	const FVector Centre = Bottom + FVector(R, 0.f, 0.f);
	auto PointAt = [&Centre, R](float Degrees)
	{
		const float Rad = FMath::DegreesToRadians(Degrees);
		return Centre + FVector(FMath::Cos(Rad) * R, 0.f, -FMath::Sin(Rad) * R);
	};

	for (int32 I = 0; I < Bend.Num(); ++I)
	{
		UStaticMeshComponent* Segment = Bend[I];
		if (!Segment) continue;

		// Every segment, hidden spares included, and unconditionally, so clearing the material in
		// the Details panel puts the engine default back.
		Segment->SetMaterial(0, HookMaterial);

		if (I >= Used)
		{
			Segment->SetVisibility(false);
			continue;
		}

		const float Step = HookBendSweep / static_cast<float>(Used);
		const FVector From = PointAt(180.f - Step * static_cast<float>(I));
		const FVector To = PointAt(180.f - Step * static_cast<float>(I + 1));
		const FVector Span = To - From;
		const float Length = FMath::Max(1.f, Span.Size());

		Segment->SetVisibility(true);
		Segment->SetRelativeLocation(From + Span * 0.5f);
		Segment->SetRelativeRotation(FRotationMatrix::MakeFromZ(Span).Rotator());
		Segment->SetRelativeScale3D(FVector(Thick, Thick, Length / HookCylinderLocal));
	}
}

void AGetawayGrapplePickup::BeginPlay()
{
	Super::BeginPlay();
	LayOutHook();
	SetAvailable(true);
}

void AGetawayGrapplePickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (bAvailable)
	{
		if (Hook)
		{
			Hook->SetRelativeLocation(FVector(0.f, 0.f, HookHeight + FMath::Sin(Age * 2.5f) * BobHeight));
			Hook->AddRelativeRotation(FRotator(0.f, SpinSpeed * DeltaSeconds, 0.f));
		}

		// Cheap backstop for a player who arrived with a full hand, or who is standing here as it
		// comes back. The same 10 Hz recheck the C4 pickup runs.
		RecheckTimer -= DeltaSeconds;
		if (RecheckTimer <= 0.f)
		{
			RecheckTimer = 0.1f;
			CheckWaitingPlayers();
		}
	}
	else
	{
		// Back on death rather than on a clock, exactly as AGetawayC4Pickup does it - see the note
		// there for why the weak pointer is the whole test.
		if (bReturnsOnDeath && (!Taker.IsValid() || Taker->IsDead()))
		{
			Respawn();
		}
		else if (RespawnDelay > 0.f)
		{
			RespawnCountdown -= DeltaSeconds;
			if (RespawnCountdown <= 0.f)
			{
				Respawn();
			}
		}
	}
}

void AGetawayGrapplePickup::OnTriggerBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	TryGive(OtherActor);
}

bool AGetawayGrapplePickup::TryGive(AActor* Other)
{
	if (!bAvailable) return false;

	AGetawayCharacter* Player = Cast<AGetawayCharacter>(Other);
	if (!Player || !Player->HasRoomForGrappleShots()) return false;

	// Only the machine that owns the character hands anything out, because what you are carrying is
	// that machine's own count - the same bargain as the rest of client authority, and the same
	// consequence: a pickup is per player. Each of you can take it once per respawn and each of you
	// sees your own pad empty. Decided for C4 on 2026-10-18; see CLAUDE.md.
	if (!Player->IsLocallyControlled()) return false;

	const int32 Taken = Player->AddGrappleShots(ShotsGranted);
	if (Taken <= 0) return false;

	Taker = Player;
	SetAvailable(false);
	RespawnCountdown = FMath::Max(RespawnDelay, 0.f);
	OnTaken.Broadcast(this, Player);
	return true;
}

void AGetawayGrapplePickup::CheckWaitingPlayers()
{
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, AGetawayCharacter::StaticClass());
	for (AActor* A : Overlapping)
	{
		if (TryGive(A)) return;
	}
}

void AGetawayGrapplePickup::Respawn()
{
	Taker = nullptr;
	SetAvailable(true);
	CheckWaitingPlayers(); // someone may be standing on it
}

void AGetawayGrapplePickup::SetAvailable(bool bNew)
{
	bAvailable = bNew;
	if (Hook)
	{
		// Propagated, so the shank and every segment of the bend go with it.
		Hook->SetVisibility(bNew, true);
	}
	// The pad stays, so a depleted pickup still reads as "a hook comes back here".
}
