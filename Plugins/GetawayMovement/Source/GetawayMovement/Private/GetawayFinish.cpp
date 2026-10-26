#include "GetawayFinish.h"

#include "GetawayCharacter.h"
#include "GetawayGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

AGetawayFinish::AGetawayFinish()
{
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	// Pawns only, query only: this must never push the player around or slow them down.
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Pad = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pad"));
	Pad->SetupAttachment(Trigger);
	Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cube.Succeeded()) Pad->SetStaticMesh(Cube.Object);

	Post = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Post"));
	Post->SetupAttachment(Trigger);
	Post->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cube.Succeeded()) Post->SetStaticMesh(Cube.Object);

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Trigger);
	Light->SetLightColor(FLinearColor(0.31f, 0.82f, 0.58f));
	Light->SetIntensity(6000.f);
	Light->SetAttenuationRadius(900.f);
	Light->SetCastShadows(false);
}

void AGetawayFinish::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	Trigger->SetBoxExtent(TriggerSize * 0.5f, false);

	// A flat pad on the floor of the trigger, and a tall thin post rising out of it.
	const float Pad_Z = -TriggerSize.Z * 0.5f;
	Pad->SetRelativeScale3D(FVector(TriggerSize.X / 100.f, TriggerSize.Y / 100.f, 0.1f));
	Pad->SetRelativeLocation(FVector(0.f, 0.f, Pad_Z + 5.f));

	Post->SetRelativeScale3D(FVector(0.5f, 0.5f, TriggerSize.Z / 100.f));
	Post->SetRelativeLocation(FVector(0.f, 0.f, 0.f));

	Light->SetRelativeLocation(FVector(0.f, 0.f, TriggerSize.Z * 0.5f));
}

void AGetawayFinish::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGetawayFinish::OnTriggerOverlap);

	// Someone may already be standing in it when the run starts.
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, AGetawayCharacter::StaticClass());
	for (AActor* A : Overlapping)
	{
		if (AGetawayCharacter* Runner = Cast<AGetawayCharacter>(A))
		{
			Reach(Runner);
		}
	}
}

void AGetawayFinish::Arm()
{
	// It stays lit all run now, because the first player through is not the last: the others are
	// still coming and they need to see where they are going.
	if (Post) Post->SetVisibility(true);
	if (Light) Light->SetVisibility(true);
}

void AGetawayFinish::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Slow spin so it catches the eye from across the level.
	Spin += DeltaSeconds * 60.f;
	if (Post)
	{
		Post->SetRelativeRotation(FRotator(0.f, Spin, 0.f));
	}
}

void AGetawayFinish::OnTriggerOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
{
	if (AGetawayCharacter* Runner = Cast<AGetawayCharacter>(OtherActor))
	{
		Reach(Runner);
	}
}

void AGetawayFinish::Reach(AGetawayCharacter* Runner)
{
	if (!Runner) return;

	// Each runner only counts once, but the objective itself is not spent by the first one through
	// it: with more than one player the rest of the field still has to get here. The game mode is
	// the one that knows who has already finished, and it is the only thing that keeps a score, so
	// it does the de-duplicating and this just reports.
	OnFinished.Broadcast(this, Runner);

	// Only the server's overlap counts. The actor exists on every machine, so without this a client
	// would try to score its own arrival off a game mode it does not have.
	if (!HasAuthority()) return;

	if (AGetawayGameMode* GM = Cast<AGetawayGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		GM->ReachedFinish(Runner);
	}
}
