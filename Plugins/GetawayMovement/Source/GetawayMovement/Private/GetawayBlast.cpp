#include "GetawayBlast.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** The basic shapes are 100 cm across at scale 1, so a radius or a side converts like this. */
	constexpr float BasicShapeSize = 100.f;
}

AGetawayBlast::AGetawayBlast()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// Cosmetic only: nothing here is ever asked about by a trace or an overlap.
	SetActorEnableCollision(false);
	SetCanBeDamaged(false);

	// Spawned independently on every machine by AGetawayC4::MulticastDetonate, so it must not
	// replicate - a replicated copy would arrive alongside the local one and draw the blast twice.
	bReplicates = false;

	// Found here rather than loaded by path on detonation, so that both meshes are hard references
	// the cooker can see. See SphereMesh.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFind(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFind.Succeeded())
	{
		SphereMesh = SphereFind.Object;
	}
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFind(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeFind.Succeeded())
	{
		CubeMesh = CubeFind.Object;
	}
}

void AGetawayBlast::SizeToBlast(float BlastRadius)
{
	if (BlastRadius > 0.f)
	{
		FlashRadius = BlastRadius * FlashRadiusFraction;
	}
}

void AGetawayBlast::BeginPlay()
{
	Super::BeginPlay();

	BuildPieces();

	// Outlive the longest-lived part of the effect and no longer. Belt and braces next to the
	// explicit Destroy in Tick, so a blast can never be left behind if a tick is missed.
	SetLifeSpan(FMath::Max(FlashTime, ShardLife) + 0.1f);
}

bool AGetawayBlast::EnsureMaterials()
{
	if (FlashMaterial && ShardMaterial) return true;

	UMaterialInterface* Base = BlastMaterial.LoadSynchronous();
	if (!Base)
	{
		// Noisy in the log, quiet on screen - the same choice as the placement ghost makes, and for
		// the same reason: the engine default material is *lit*, so in this unlit world the
		// fallback draws as a black blob rather than as something obviously missing.
		UE_LOG(LogTemp, Warning, TEXT("Getaway: no blast material at '%s'. Run "
			"Content/Python/adrenaline_look.py in the editor to build it. Falling back to the "
			"engine default material, which is lit, so the explosion will look black."),
			*BlastMaterial.ToString());
		Base = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	FlashMaterial = UMaterialInstanceDynamic::Create(Base, this);
	ShardMaterial = UMaterialInstanceDynamic::Create(Base, this);
	if (!FlashMaterial || !ShardMaterial) return false;

	for (UMaterialInstanceDynamic* Mat : { FlashMaterial.Get(), ShardMaterial.Get() })
	{
		Mat->SetVectorParameterValue(TEXT("Color"), FaceColour);
		Mat->SetVectorParameterValue(TEXT("EdgeColor"), EdgeColour);
		// The world grid belongs to the level, not to a flash going off inside it. Same call the
		// ghost's instance makes.
		Mat->SetScalarParameterValue(TEXT("GridStrength"), 0.f);
	}

	FlashMaterial->SetScalarParameterValue(TEXT("FaceOpacity"), FlashFaceOpacity);
	ShardMaterial->SetScalarParameterValue(TEXT("BorderWidth"), ShardBorderWidth);

	return true;
}

void AGetawayBlast::BuildPieces()
{
	if (!EnsureMaterials()) return;

	if (SphereMesh)
	{
		Flash = NewObject<UStaticMeshComponent>(this, TEXT("Flash"));
		Flash->SetStaticMesh(SphereMesh);
		Flash->SetMaterial(0, FlashMaterial);
		Flash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		// It is additive and it is meant to be seen through walls while it lasts, so it casts
		// nothing and receives nothing.
		Flash->SetCastShadow(false);
		Flash->SetupAttachment(GetRootComponent());
		Flash->RegisterComponent();
		Flash->SetRelativeScale3D(FVector(FlashStartRadius * 2.f / BasicShapeSize));
	}

	if (!CubeMesh || ShardCount <= 0) return;

	ShardMeshes.Reserve(ShardCount);
	ShardMotion.Reserve(ShardCount);

	for (int32 i = 0; i < ShardCount; ++i)
	{
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(this,
			*FString::Printf(TEXT("Shard%d"), i));
		if (!Mesh) continue;

		Mesh->SetStaticMesh(CubeMesh);
		Mesh->SetMaterial(0, ShardMaterial);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetCastShadow(false);
		Mesh->SetupAttachment(GetRootComponent());
		Mesh->RegisterComponent();
		Mesh->SetRelativeScale3D(FVector(ShardSize / BasicShapeSize));
		// Absolute, so a shard flies away from where the charge was rather than being carried
		// about by the actor it hangs off.
		Mesh->SetUsingAbsoluteLocation(true);
		Mesh->SetUsingAbsoluteRotation(true);

		FShardMotion Motion;
		Motion.Position = GetActorLocation();

		// An even direction on the sphere, then tilted upward. VRand is uniform over the sphere,
		// which a per-axis random would not be - it would bunch the shards towards the corners.
		FVector Direction = FMath::VRand();
		Direction.Z = FMath::Abs(Direction.Z) * ShardUpwardBias + Direction.Z * (1.f - ShardUpwardBias);
		Direction = Direction.GetSafeNormal(SMALL_NUMBER, FVector::UpVector);

		const float Speed = FMath::Max(0.f,
			ShardSpeed + FMath::FRandRange(-ShardSpeedVariance, ShardSpeedVariance));
		Motion.Velocity = Direction * Speed;
		Motion.SpinAxis = FMath::VRand();
		Motion.Rotation = FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f),
			FMath::FRandRange(0.f, 360.f)).Quaternion();

		Mesh->SetWorldLocationAndRotation(Motion.Position, Motion.Rotation);

		ShardMeshes.Add(Mesh);
		ShardMotion.Add(Motion);
	}
}

void AGetawayBlast::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Age += DeltaSeconds;

	// The flash: swell fast and fade faster. The radius eases out so the first frames are the
	// quick ones, which is what a bang looks like; the brightness falls off squared so it is gone
	// well before the shards are.
	if (Flash)
	{
		const float Alpha = FMath::Clamp(Age / FlashTime, 0.f, 1.f);
		const float Eased = 1.f - FMath::Square(1.f - Alpha);
		const float Radius = FMath::Lerp(FlashStartRadius, FlashRadius, Eased);
		Flash->SetRelativeScale3D(FVector(Radius * 2.f / BasicShapeSize));

		if (Alpha >= 1.f)
		{
			Flash->SetVisibility(false);
		}
		else if (FlashMaterial)
		{
			FlashMaterial->SetScalarParameterValue(TEXT("Brightness"),
				FlashBrightness * FMath::Square(1.f - Alpha));
		}
	}

	// The shards: a plain ballistic step and a tumble. One shared material, so they fade together.
	const float ShardAlpha = FMath::Clamp(Age / ShardLife, 0.f, 1.f);
	if (ShardMaterial)
	{
		ShardMaterial->SetScalarParameterValue(TEXT("Brightness"),
			ShardBrightness * (1.f - ShardAlpha));
	}

	for (int32 i = 0; i < ShardMeshes.Num() && i < ShardMotion.Num(); ++i)
	{
		UStaticMeshComponent* Mesh = ShardMeshes[i];
		if (!Mesh) continue;

		if (ShardAlpha >= 1.f)
		{
			Mesh->SetVisibility(false);
			continue;
		}

		FShardMotion& Motion = ShardMotion[i];
		Motion.Velocity.Z -= ShardGravity * DeltaSeconds;
		Motion.Position += Motion.Velocity * DeltaSeconds;
		const FQuat Spin(Motion.SpinAxis.GetSafeNormal(SMALL_NUMBER, FVector::UpVector),
			FMath::DegreesToRadians(ShardSpin * DeltaSeconds));
		Motion.Rotation = Spin * Motion.Rotation;

		Mesh->SetWorldLocationAndRotation(Motion.Position, Motion.Rotation);
	}

	if (Age >= FMath::Max(FlashTime, ShardLife))
	{
		Destroy();
	}
}
