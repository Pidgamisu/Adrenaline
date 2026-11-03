#include "GetawayMenuCamera.h"

#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetMathLibrary.h"

AGetawayMenuCamera::AGetawayMenuCamera()
{
	PrimaryActorTick.bCanEverTick = true;

	// A bare scene component, not a capsule: nothing should ever collide with the menu camera, and
	// it is never simulated.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Root);
	Camera->bUsePawnControlRotation = false;

	bCollideWhenPlacing = false;
	SetCanBeDamaged(false);
	// Nobody else needs to see where somebody's menu camera is pointing, and in a menu session
	// there is nobody else anyway.
	bReplicates = false;
}

void AGetawayMenuCamera::BeginPlay()
{
	Super::BeginPlay();

	Camera->SetFieldOfView(FieldOfView);
	Angle = StartAngle;

	if (bAutoFrameLevel)
	{
		FrameLevel();
	}
	ApplyOrbit(Angle);
}

void AGetawayMenuCamera::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	Angle += OrbitSpeed * DeltaSeconds;
	ApplyOrbit(Angle);
}

void AGetawayMenuCamera::FrameLevel()
{
	UWorld* World = GetWorld();
	if (!World) return;

	FBox Bounds(ForceInit);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		const AActor* Actor = *It;
		if (!Actor || Actor == this) continue;

		// Only things with actual geometry. Everything else in a level - PlayerStarts, the game
		// mode's managers, lights, the fog - reports a zero or nominal box and would drag the
		// middle of the level towards wherever it happens to sit.
		if (!Actor->GetRootComponent() || !Actor->GetRootComponent()->IsA<UPrimitiveComponent>()) continue;

		FVector Origin, Extent;
		Actor->GetActorBounds(/*bOnlyCollidingComponents=*/true, Origin, Extent);
		if (Extent.IsNearlyZero()) continue;

		// Anything absurdly large is not part of the course, and one of them is always there:
		// AGameplayDebuggerCategoryReplicator carries a rendering component whose bounds are a
		// 10 km cube centred on the origin, so that the debugger draw is never culled. It exists
		// in every non-Shipping build, which is every build we hand to anybody. Left in, it framed
		// TestCourse at a radius of 15.5 km and the menu looked at empty space.
		//
		// A sky sphere, an unbound post-process volume or a kill plane would do the same, so this
		// is a cap rather than a check for that one class. Found by running the game headless,
		// 2026-11-02.
		if (Extent.GetMax() > IgnoreActorsBiggerThan)
		{
			UE_LOG(LogTemp, Verbose, TEXT("Getaway: the menu camera ignored %s (%s), extent %s"),
				*Actor->GetName(), *Actor->GetClass()->GetName(), *Extent.ToCompactString());
			continue;
		}

		Bounds += FBox(Origin - Extent, Origin + Extent);
	}

	if (!Bounds.IsValid)
	{
		// An empty map. Leave whatever is typed in the Circle category alone rather than orbiting
		// a radius of zero.
		UE_LOG(LogTemp, Warning, TEXT("Getaway: the menu camera found nothing to frame; using the values on the pawn."));
		return;
	}

	const FVector Centre = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();

	OrbitCentre = Centre;
	OrbitRadius = FMath::Max(Extent.X, Extent.Y) * FramePadding;
	// Off the radius, not off Extent.Z - see HeightFraction for why.
	OrbitHeight = OrbitRadius * HeightFraction;
	LookAtHeight = Extent.Z * LookAtHeightFraction;

	UE_LOG(LogTemp, Log, TEXT("Getaway: the menu camera framed the level at %s, radius %.0f, height %.0f"),
		*OrbitCentre.ToCompactString(), OrbitRadius, OrbitHeight);
}

void AGetawayMenuCamera::ApplyOrbit(float AngleDegrees)
{
	const float Radians = FMath::DegreesToRadians(AngleDegrees);
	const FVector Where = OrbitCentre + FVector(
		FMath::Cos(Radians) * OrbitRadius,
		FMath::Sin(Radians) * OrbitRadius,
		OrbitHeight);

	const FVector LookAt = OrbitCentre + FVector(0.f, 0.f, LookAtHeight);

	SetActorLocation(Where);
	SetActorRotation(UKismetMathLibrary::FindLookAtRotation(Where, LookAt));
}
