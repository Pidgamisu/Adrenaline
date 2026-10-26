#include "GetawayBreakable.h"

#include "GetawayCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

const FName AGetawayBreakable::ScrapTag(TEXT("GetawayScrap"));

AGetawayBreakable::AGetawayBreakable()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName); // object type WorldDynamic
	Mesh->SetMobility(EComponentMobility::Movable); // it hides and comes back at runtime

	// Replicated so a panel someone places exists for everyone, and so breaking and restoring it
	// reach every machine - see Break(). bAlwaysRelevant because a breakable is scenery: a client
	// that is not near one still has to agree about whether it is standing.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded())
	{
		Mesh->SetStaticMesh(Cube.Object);
	}

	// Something that is going to shatter should not look identical to a wall that is not. The
	// plugin ships no content, so this is the engine's grid material rather than real art: next to
	// the plain grey BasicShapeMaterial every other piece wears, it reads as different at a glance.
	//
	// Set here in the constructor and *not* in OnConstruction, deliberately. A material assigned to
	// a breakable in a level - the test course colour-codes its panels orange - overrides the class
	// default and is left alone; only a breakable that has not been given one picks this up, which
	// is every breakable the party box deals. Swap it for a real texture when there is art.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Grid(
		TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	if (Grid.Succeeded())
	{
		Mesh->SetMaterial(0, Grid.Object);
	}
}

void AGetawayBreakable::BreakNow(FVector Velocity)
{
	if (bBroken) return;
	bBroken = true;

	UWorld* World = GetWorld();
	if (!World) return;

	const FBox Box = Mesh->Bounds.GetBox();
	const FVector Size = Box.GetSize();

	// Anything breakable resting on top goes a moment later, with half the push.
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GetawayBreakChain), false, this);
		const FVector Center(Box.GetCenter().X, Box.GetCenter().Y, Box.Max.Z + 4.f);
		World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity, FCollisionObjectQueryParams(ECC_WorldDynamic),
			FCollisionShape::MakeBox(FVector(Size.X * 0.45f, Size.Y * 0.45f, 3.f)), Params);

		TSet<AGetawayBreakable*> Queued;
		for (const FOverlapResult& O : Overlaps)
		{
			AGetawayBreakable* Other = Cast<AGetawayBreakable>(O.GetActor());
			if (!Other || Other == this || Other->IsBroken() || Queued.Contains(Other)) continue;
			Queued.Add(Other);

			TWeakObjectPtr<AGetawayBreakable> Weak(Other);
			const FVector Half = Velocity * 0.5f;
			FTimerHandle Handle;
			World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Half]()
			{
				if (Weak.IsValid()) Weak->Break(Half);
			}), FMath::Max(ChainDelay, 0.001f), false);
		}
	}

	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetVisibility(false);

	// Split the bounds into a grid of pieces, at most 7 per side and MaxPieces in total.
	int32 N[3];
	for (int32 A = 0; A < 3; ++A)
	{
		N[A] = FMath::Clamp(FMath::RoundToInt32(Size[A] / PieceSize), 1, 7);
	}
	while (N[0] * N[1] * N[2] > MaxPieces)
	{
		int32 Largest = 0;
		for (int32 A = 1; A < 3; ++A) if (N[A] > N[Largest]) Largest = A;
		if (N[Largest] <= 1) break;
		--N[Largest];
	}

	UStaticMesh* SM = Mesh->GetStaticMesh();
	if (!SM) return;
	UMaterialInterface* Mat = ScrapMaterial ? ScrapMaterial.Get() : Mesh->GetMaterial(0);
	const FVector MeshSize = SM->GetBounds().BoxExtent * 2.f;
	const FVector Piece(Size.X / N[0], Size.Y / N[1], Size.Z / N[2]);
	const FVector PieceScale(
		Piece.X * 0.92f / FMath::Max(MeshSize.X, 1.f),
		Piece.Y * 0.92f / FMath::Max(MeshSize.Y, 1.f),
		Piece.Z * 0.92f / FMath::Max(MeshSize.Z, 1.f));

	for (int32 I = 0; I < N[0]; ++I)
	for (int32 J = 0; J < N[1]; ++J)
	for (int32 K = 0; K < N[2]; ++K)
	{
		UStaticMeshComponent* Scrap = NewObject<UStaticMeshComponent>(this);
		Scrap->SetMobility(EComponentMobility::Movable);
		Scrap->SetStaticMesh(SM);
		if (Mat) Scrap->SetMaterial(0, Mat);
		Scrap->SetWorldScale3D(PieceScale);
		Scrap->SetWorldLocation(Box.Min + Piece * FVector(I + 0.5f, J + 0.5f, K + 0.5f));
		Scrap->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName); // object type PhysicsBody
		Scrap->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);                 // the player walks through and shoves them
		Scrap->ComponentTags.Add(ScrapTag);
		Scrap->RegisterComponent();
		Scrap->SetSimulatePhysics(true);

		FVector V = Velocity * FMath::FRandRange(0.4f, 0.9f);
		V += FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(50.f, 350.f));
		Scrap->SetPhysicsLinearVelocity(V);
		Scrap->SetPhysicsAngularVelocityInDegrees(FMath::VRand() * FMath::FRandRange(200.f, 700.f));
		Scraps.Add(Scrap);
	}

	// Keep the actor. It used to SetLifeSpan here and delete itself, which meant a breakable was
	// gone for the rest of the session after one run - the course wore away. Only the scraps go.
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().SetTimer(ScrapTimer, this, &AGetawayBreakable::ClearScraps,
			FMath::Max(0.1f, ScrapLifetime), false);
	}

	// Shake whoever is playing on this machine, and only if they are near enough to have seen it,
	// fading with distance. Breaking is still local (see the constructor), so today that is almost
	// always the player who caused it - but an unconditional shake becomes a phantom jolt for the
	// whole level the moment Break() is replicated, which is the plan.
	if (AGetawayCharacter* Local = Cast<AGetawayCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		const float Distance = FVector::Dist(Local->GetActorLocation(), Box.GetCenter());
		const float Falloff = (ShakeRadius > 0.f) ? 1.f - Distance / ShakeRadius : 1.f;
		if (Falloff > 0.f)
		{
			Local->AddCameraShake(0.12f * FMath::Min(1.f, Falloff));
		}
	}
	OnBroken.Broadcast(this);
}

void AGetawayBreakable::ClearScraps()
{
	for (UStaticMeshComponent* S : Scraps)
	{
		if (IsValid(S))
		{
			S->DestroyComponent();
		}
	}
	Scraps.Reset();
}

void AGetawayBreakable::RestoreNow()
{
	if (UWorld* W = GetWorld())
	{
		W->GetTimerManager().ClearTimer(ScrapTimer);
	}
	ClearScraps();

	if (!bBroken) return;
	bBroken = false;
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetVisibility(true);
	OnRestored.Broadcast(this);
}

/* ============================================================ getting it to every machine */

void AGetawayBreakable::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGetawayBreakable, bBroken);
}

void AGetawayBreakable::Break(FVector Velocity)
{
	if (bBroken) return;

	if (HasAuthority())
	{
		// Reaches this machine too: a multicast runs on the server as well as the clients.
		MulticastBreak(Velocity);
		return;
	}

	// A client breaks it on the spot rather than waiting to be told. Smashing a panel at 900 cm/s
	// is the one thing in this game that cannot afford a round trip, and it matches how the rest of
	// the movement works - the client is trusted, and the server repeats it to everybody else.
	BreakNow(Velocity);

	// The breakable is a level actor that nobody owns, and a server RPC from an unowned actor is
	// dropped, so the report goes through this player's own character, which they do own.
	if (AGetawayCharacter* Local = Cast<AGetawayCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		Local->ServerReportBreak(this, Velocity);
	}
}

void AGetawayBreakable::MulticastBreak_Implementation(FVector_NetQuantize100 Velocity)
{
	BreakNow(Velocity);
}

void AGetawayBreakable::Restore()
{
	if (HasAuthority())
	{
		MulticastRestore();
		return;
	}
	RestoreNow();
}

void AGetawayBreakable::MulticastRestore_Implementation()
{
	RestoreNow();
}

void AGetawayBreakable::OnRep_Broken()
{
	// State, not event: somebody who joined after the break never saw the multicast. Give them the
	// panel as it stands, with no scrap burst for something that happened before they arrived.
	if (bBroken)
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetVisibility(false);
	}
	else
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetVisibility(true);
	}
}
