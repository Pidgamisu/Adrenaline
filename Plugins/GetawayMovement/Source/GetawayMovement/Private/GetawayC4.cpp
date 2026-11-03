#include "GetawayC4.h"

#include "GetawayBlast.h"
#include "GetawayBreakable.h"
#include "GetawayCharacter.h"
#include "GetawayMovementComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "UObject/ConstructorHelpers.h"

AGetawayC4::AGetawayC4()
{
	PrimaryActorTick.bCanEverTick = true;

	// Sweeps against level geometry only: ignores the player and loose scraps.
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(6.f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	RootComponent = Collision;

	// 10 x 28 x 20 cm brick; the thin side faces the surface it sticks to (actor X axis).
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Collision);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.10f, 0.28f, 0.20f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Body->SetStaticMesh(Cube.Object);
	}

	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Collision);
	Light->SetRelativeLocation(FVector(8.f, 0.f, 6.f));
	Light->SetLightColor(FLinearColor(1.f, 0.15f, 0.1f));
	Light->SetIntensity(1500.f);
	Light->SetAttenuationRadius(120.f);
	Light->SetCastShadows(false);

	BlastEffectClass = AGetawayBlast::StaticClass();

	// A thrown charge is spawned by the server and streamed out, rather than simulated separately
	// on each machine: the arc is short, and two machines integrating it independently would put
	// the brick in two different places by the time it sticks.
	bReplicates = true;
	SetReplicateMovement(true);
}

void AGetawayC4::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGetawayC4, BlastRadius);
	DOREPLIFETIME(AGetawayC4, BlastBoost);
}

void AGetawayC4::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* MyOwner = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(MyOwner, true);
	}
}

void AGetawayC4::Launch(const FVector& InVelocity, float InBlastRadius, float InBlastBoost, float InGravityScale)
{
	Velocity = InVelocity;
	BlastRadius = FMath::Max(InBlastRadius, 130.f);
	BlastBoost = InBlastBoost;
	GravityScale = InGravityScale;
}

void AGetawayC4::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += DeltaSeconds;
	Light->SetVisibility(FMath::FloorToInt(Age * (bStuck ? 3.f : 8.f)) % 2 == 0);

	// The blink is cosmetic and runs everywhere; the flight and the stick are the server's, and
	// everyone else just receives where it ended up.
	if (!HasAuthority()) return;

	if (bStuck)
	{
		// Fall again if the surface we're stuck to breaks or goes away.
		if (StuckTo.IsValid() && StuckTo->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
		{
			return;
		}
		Unstick();
	}

	Velocity.Z += GetWorld()->GetGravityZ() * GravityScale * DeltaSeconds;

	FHitResult Hit;
	AddActorWorldOffset(Velocity * DeltaSeconds, true, &Hit);
	if (Hit.bBlockingHit)
	{
		Stick(Hit);
		return;
	}

	AddActorLocalRotation(FRotator(540.f * DeltaSeconds, 360.f * DeltaSeconds, 0.f));

	if (GetActorLocation().Z < GetWorld()->GetWorldSettings()->KillZ)
	{
		Destroy();
	}
}

void AGetawayC4::Stick(const FHitResult& Hit)
{
	bStuck = true;
	Velocity = FVector::ZeroVector;
	SetActorLocation(Hit.Location + Hit.ImpactNormal * 2.f);
	SetActorRotation(Hit.ImpactNormal.Rotation());
	if (UPrimitiveComponent* Comp = Hit.GetComponent())
	{
		AttachToComponent(Comp, FAttachmentTransformRules::KeepWorldTransform);
		StuckTo = Comp;
	}
}

void AGetawayC4::Unstick()
{
	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bStuck = false;
	StuckTo = nullptr;
}

void AGetawayC4::Detonate()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority()) return;

	const FVector P = GetActorLocation();

	// Tell everybody first, then do the parts only the authority may do.
	MulticastDetonate(P);
	ApplyWorldBlast(P);

	// Not Destroy(): the reliable multicast above is queued on this actor's channel, and tearing
	// the channel down in the same frame can take the message with it. It is already hidden on
	// every machine by the multicast, so a short life span finishes the job unseen.
	SetActorEnableCollision(false);
	SetLifeSpan(0.5f);
}

void AGetawayC4::MulticastDetonate_Implementation(FVector_NetQuantize100 P)
{
	PlayBlastEffects(P);
	LaunchLocalPlayer(P);
	SetActorHiddenInGame(true);
}

void AGetawayC4::PredictLocalBlast()
{
	LaunchLocalPlayer(GetActorLocation());
}

void AGetawayC4::LaunchLocalPlayer(const FVector& P)
{
	if (bLocalBlastApplied) return;

	// Only the thrower is pushed, exactly as before - this is a rocket jump, not a weapon. What is
	// new is *where* it happens: on the machine that controls that character, because under client
	// authority no other machine is allowed to move it.
	ACharacter* Thrower = Cast<ACharacter>(GetOwner());
	if (!Thrower || !Thrower->IsLocallyControlled()) return;
	bLocalBlastApplied = true;

	{
		FVector D = Thrower->GetActorLocation() - P; // capsule centre
		const float Dist = D.Size();
		if (Dist < BlastRadius)
		{
			const float K = FMath::Clamp(1.f - (Dist - 120.f) / (BlastRadius - 120.f), 0.f, 1.f);
			D = Dist > 1.f ? D / Dist : FVector::UpVector;

			UCharacterMovementComponent* Move = Thrower->GetCharacterMovement();
			if (Move && Move->IsMovingOnGround() && D.Z < 0.35f)
			{
				// Standing on the ground, a blast always has some lift.
				D.Z = 0.35f;
				D.Normalize();
			}
			if (UGetawayMovementComponent* GMove = Cast<UGetawayMovementComponent>(Move))
			{
				GMove->ApplyBlast(D * BlastBoost * K);
			}
			else
			{
				Thrower->LaunchCharacter(D * BlastBoost * K, false, false);
			}
			if (AGetawayCharacter* GC = Cast<AGetawayCharacter>(Thrower))
			{
				GC->AddViewPunch(3.4f * K);
				GC->AddCameraShake(0.7f / (1.f + Dist / 100.f * 0.12f));
			}
		}
		else if (AGetawayCharacter* GC = Cast<AGetawayCharacter>(Thrower))
		{
			GC->AddCameraShake(0.7f / (1.f + Dist / 100.f * 0.12f));
		}
	}
}

void AGetawayC4::ApplyWorldBlast(const FVector& P)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Burst breakables and shove loose physics objects. Authority only: AGetawayBreakable::Break
	// multicasts itself from here, and the scraps are each machine's own.
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	Objects.AddObjectTypesToQuery(ECC_PhysicsBody);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GetawayC4Blast), false, this);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, P, FQuat::Identity, Objects, FCollisionShape::MakeSphere(BlastRadius), Params);

	TSet<AGetawayBreakable*> Broken;
	for (const FOverlapResult& O : Overlaps)
	{
		AGetawayBreakable* B = Cast<AGetawayBreakable>(O.GetActor());
		if (B && !B->IsBroken())
		{
			if (!Broken.Contains(B))
			{
				Broken.Add(B);
				FVector Dir = B->GetActorLocation() - P;
				const float L = FMath::Max(Dir.Size(), 1.f);
				const float KK = FMath::Max(0.f, 1.f - L / BlastRadius);
				B->Break(Dir / L * (600.f + 1400.f * KK) + FVector(0.f, 0.f, 300.f));
			}
			continue;
		}
		UPrimitiveComponent* Comp = O.GetComponent();
		if (Comp && Comp->IsSimulatingPhysics())
		{
			Comp->AddRadialImpulse(P, BlastRadius, 1400.f, ERadialImpulseFalloff::RIF_Linear, true);
		}
	}
}

void AGetawayC4::PlayBlastEffects(const FVector& P)
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ExplosionEffect, P);
	}
	else if (UClass* Class = BlastEffectClass ? BlastEffectClass.Get() : nullptr)
	{
		// Real geometry rather than the DrawDebugSphere this used to be: debug draw is compiled
		// out of Shipping and Test, so a packaged build showed nothing at all. See AGetawayBlast.
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AGetawayBlast* Blast = World->SpawnActor<AGetawayBlast>(Class, P, FRotator::ZeroRotator, Params))
		{
			// So the flash matches this charge's reach rather than a number typed into the class.
			Blast->SizeToBlast(BlastRadius);
		}
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, P);
	}
}
