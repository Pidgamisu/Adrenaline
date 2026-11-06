#include "GetawayC4Pickup.h"

#include "GetawayCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGetawayC4Pickup::AGetawayC4Pickup()
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
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	// 110 cm wide, 6 cm tall pad, sitting on the floor when the actor is placed at floor level.
	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	Pad->SetupAttachment(Trigger);
	Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pad->SetRelativeLocation(FVector(0.f, 0.f, 3.f));
	Pad->SetRelativeScale3D(FVector(1.1f, 1.1f, 0.06f));
	if (Cylinder.Succeeded())
	{
		Pad->SetStaticMesh(Cylinder.Object);
	}

	// A C4 brick like AGetawayC4's, a little larger so it reads from a distance, floating and spinning above the pad.
	Charge = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Charge"));
	Charge->SetupAttachment(Trigger);
	Charge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Charge->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	Charge->SetRelativeRotation(FRotator(20.f, 0.f, 0.f));
	Charge->SetRelativeScale3D(FVector(0.14f, 0.40f, 0.28f));
	if (Cube.Succeeded())
	{
		Charge->SetStaticMesh(Cube.Object);
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Trigger);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 45.f));
	Light->SetLightColor(FLinearColor(1.f, 0.55f, 0.15f)); // amber, like the lab's pickups
	Light->SetIntensity(2000.f);
	Light->SetAttenuationRadius(320.f);
	Light->SetCastShadows(false);

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGetawayC4Pickup::OnTriggerBeginOverlap);
}

void AGetawayC4Pickup::BeginPlay()
{
	Super::BeginPlay();
	ChargeBaseZ = Charge->GetRelativeLocation().Z;
	SetAvailable(true);
}

void AGetawayC4Pickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;

	if (bAvailable)
	{
		Charge->SetRelativeLocation(FVector(0.f, 0.f, ChargeBaseZ + FMath::Sin(Age * 2.5f) * BobHeight));
		Charge->AddRelativeRotation(FRotator(0.f, SpinSpeed * DeltaSeconds, 0.f));

		// Cheap backstop for a player who arrived full, or who is standing here as it comes back.
		RecheckTimer -= DeltaSeconds;
		if (RecheckTimer <= 0.f)
		{
			RecheckTimer = 0.1f;
			CheckWaitingPlayers();
		}
	}
	else
	{
		// Back on death rather than on a clock. A destroyed character makes the weak pointer stale,
		// and IsDead covers the beat between dying and being destroyed - so the pad is ready again
		// by the time you are back at the start rather than some seconds after it.
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

void AGetawayC4Pickup::OnTriggerBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	TryGive(OtherActor);
}

bool AGetawayC4Pickup::TryGive(AActor* Other)
{
	if (!bAvailable) return false;

	AGetawayCharacter* Player = Cast<AGetawayCharacter>(Other);
	if (!Player || !Player->HasRoomForCharges()) return false;

	// Only the machine that owns the character hands out charges, because the charge count is that
	// machine's own - the same bargain as the rest of client authority. The consequence is that a
	// pickup is **per player**: each of you can collect it once per respawn, and each of you sees
	// your own pad empty. That is a design choice and an easy one to reverse; see CLAUDE.md.
	if (!Player->IsLocallyControlled()) return false;

	const int32 Taken = Player->AddCharges(ChargesGranted);
	if (Taken <= 0) return false;

	Taker = Player;
	SetAvailable(false);
	RespawnCountdown = FMath::Max(RespawnDelay, 0.f);
	OnTaken.Broadcast(this, Player);
	return true;
}

void AGetawayC4Pickup::CheckWaitingPlayers()
{
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, AGetawayCharacter::StaticClass());
	for (AActor* A : Overlapping)
	{
		if (TryGive(A)) return;
	}
}

void AGetawayC4Pickup::Respawn()
{
	Taker = nullptr;
	SetAvailable(true);
	CheckWaitingPlayers(); // someone may be standing on it
}

void AGetawayC4Pickup::SetAvailable(bool bNew)
{
	bAvailable = bNew;
	Charge->SetVisibility(bNew);
	Light->SetVisibility(bNew);
	// The pad stays, so a depleted pickup still reads as "C4 comes back here".
}
