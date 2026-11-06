#include "GetawayJumpPad.h"

#include "GetawayBreakable.h"
#include "GetawayCharacter.h"
#include "GetawayMovementComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	/** Local half extent of the trigger, so the plate is 100 local units across before scaling. */
	constexpr float PlateLocal = 100.f;
	/** Local Z of the top of the base plate; the disc stands on it. */
	constexpr float PlateTopLocal = -30.f;
	/**
	 * Steeper than this far below horizontal and the ground lift is left alone: a pad on a ceiling
	 * or under a ramp is pointing down on purpose, and lifting it would fire you the other way.
	 */
	constexpr float MinLiftableZ = -0.5f;
}

AGetawayJumpPad::AGetawayJumpPad()
{
	PrimaryActorTick.bCanEverTick = false;

	// Authored inside a 100 cm cube so the catalog can scale it like every other piece: at the
	// catalog's 200 x 200 x 50 the actor scale is (2, 2, 0.5), and everything below rides it.
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->InitBoxExtent(FVector(50.f, 50.f, 50.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionObjectType(ECC_WorldDynamic);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);
	RootComponent = Trigger;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	// The plate, lying on the bottom of the local cube - the face that ends up against the surface.
	Base = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Base"));
	Base->SetupAttachment(Trigger);
	Base->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Base->SetRelativeLocation(FVector(0.f, 0.f, -40.f));
	Base->SetRelativeScale3D(FVector(1.f, 1.f, 0.2f));
	if (Cube.Succeeded())
	{
		Base->SetStaticMesh(Cube.Object);
	}

	// A disc, not a point: a plate with points on it is the thing that kills you, and the two
	// pieces would otherwise read as the same shape and size at a glance.
	Piston = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Piston"));
	Piston->SetupAttachment(Trigger);
	Piston->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Cylinder.Succeeded())
	{
		Piston->SetStaticMesh(Cylinder.Object);
	}

	// Replicated so a pad somebody places exists for everyone - the lesson of 2026-10-18. Movement
	// replication on for the same reason the spikes have it: an actor's attachment rides on it, so
	// a pad stuck to a collapsing bridge has to come down on every machine, not just the server's.
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(true);
}

void AGetawayJumpPad::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Runs on every machine, because the scale travels in the spawn transform - the same trick
	// AGetawaySpikes and AGetawayRamp use to build themselves with no mesh going over the wire.
	LayOutPiston();
}

void AGetawayJumpPad::LayOutPiston()
{
	if (!Piston) return;

	const FVector Scale = GetActorScale3D().GetAbs();
	const float WorldX = PlateLocal * Scale.X;
	const float WorldY = PlateLocal * Scale.Y;

	// Keep the disc the same size in the world however the plate is scaled, by dividing the actor
	// scale back out - otherwise a pad trimmed back to a ledge gets a squashed oval. It is also
	// held inside the plate it stands on, so a trimmed pad gets a smaller disc rather than one
	// hanging over the edge.
	const float Width = FMath::Min3(PistonWidth, WorldX * 0.7f, WorldY * 0.7f);
	const FVector Rel(
		Width / FMath::Max(1.f, PlateLocal * Scale.X),
		Width / FMath::Max(1.f, PlateLocal * Scale.Y),
		PistonHeight / FMath::Max(1.f, PlateLocal * Scale.Z));
	const float LocalHeight = PistonHeight / FMath::Max(KINDA_SMALL_NUMBER, Scale.Z);

	Piston->SetRelativeScale3D(Rel);
	// The cylinder mesh is centred, so half its local height above the plate puts its base on it.
	Piston->SetRelativeLocation(FVector(0.f, 0.f, PlateTopLocal + LocalHeight * 0.5f));
}

void AGetawayJumpPad::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGetawayJumpPad::OnTriggerOverlap);
	// A client can get the actor before Host has arrived; OnRep_Host covers that case.
	BindToHost();
}

void AGetawayJumpPad::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGetawayJumpPad, Host);
}

FVector AGetawayJumpPad::GetLaunchDirection() const
{
	// Placement turned the pad's +Z out of the surface, so this is the normal of whatever it is on.
	return GetActorQuat().GetAxisZ().GetSafeNormal();
}

void AGetawayJumpPad::SetArmed(bool bNew)
{
	bArmed = bNew;
	// Each round starts clean, so a pad that threw you at the end of the last one can throw you
	// again on the first frame of this one.
	LastLaunchTime.Reset();
}

/* ============================================================ the launch */

void AGetawayJumpPad::OnTriggerOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*,
	int32, bool, const FHitResult&)
{
	if (!bArmed || !bPresent) return;

	AGetawayCharacter* Who = Cast<AGetawayCharacter>(OtherActor);
	if (!Who) return;

	// Only the machine that owns this character throws it. Under client authority no other machine
	// is allowed to move it anyway, and it has to happen on the frame you touched the pad rather
	// than a round trip later - the same bargain as dying on spikes and the C4 rocket jump.
	if (!Who->IsLocallyControlled()) return;

	const UWorld* World = GetWorld();
	if (!World) return;

	const float Now = World->GetTimeSeconds();
	if (const float* Last = LastLaunchTime.Find(Who))
	{
		if (Now - *Last < Retrigger) return;
	}
	LastLaunchTime.Add(Who, Now);

	LaunchLocal(Who);
	OnLaunched.Broadcast(this, Who);
}

void AGetawayJumpPad::LaunchLocal(AGetawayCharacter* Who)
{
	UCharacterMovementComponent* Move = Who->GetCharacterMovement();
	if (!Move) return;

	FVector Out = GetLaunchDirection();

	// A pad flat on a wall fires horizontally, and a horizontal shove handed to somebody standing
	// on the floor is eaten by ground friction before it takes them anywhere - so a flat launch is
	// tilted upward while you are on the ground, exactly as AGetawayC4 does for a blast at your
	// feet. Only while you are on the ground: in the air, off a wallrun or on a slope there is no
	// friction to beat and the pad should fire precisely where it points.
	const float Lift = FMath::Clamp(GroundLift, 0.f, 1.f);
	if (Lift > 0.f && Out.Z < Lift && Out.Z > MinLiftableZ && Move->IsMovingOnGround())
	{
		Out.Z = Lift;
		Out = Out.GetSafeNormal();
	}

	const FVector V = Move->Velocity;
	const float Along = FVector::DotProduct(V, Out);
	const FVector Across = V - Out * Along;

	// The speed out of the pad is the pad's business; the speed across it is yours. That is what
	// makes a pad the same height for everybody while still letting a fast player carry their
	// run-up through it. Max() rather than a straight assignment, so the pad can only ever throw
	// you - arriving faster than it fires is no reason to be slowed down.
	const float NewAlong = bAddToIncoming ? Along + LaunchSpeed : FMath::Max(Along, LaunchSpeed);
	const FVector Target = Out * NewAlong + Across * FMath::Clamp(SideSpeedKept, 0.f, 1.f);

	if (UGetawayMovementComponent* GMove = Cast<UGetawayMovementComponent>(Move))
	{
		// ApplyBlast already knows how to drop you out of a wallrun or a mantle, and off the ground
		// when the kick points away from it, so the pad is expressed as the change it wants.
		GMove->ApplyBlast(Target - V);

		// And the window, which is what makes the launch last past the landing. Handed over here
		// rather than ticked at you, because the pad is behind you a frame later; the character's
		// own clock runs it from now on. It is not a push and does nothing to your velocity - see
		// AddLowFriction.
		GMove->AddLowFriction(FrictionScale, FrictionTime);
	}
	else
	{
		Who->LaunchCharacter(Target, true, true);
	}

	Who->AddViewPunch(LaunchViewPunch);
	Who->AddCameraShake(LaunchShake);
}

/* ============================================================ stuck to something */

void AGetawayJumpPad::AttachToHost(UPrimitiveComponent* InHost)
{
	if (!HasAuthority()) return;

	// The actor is what the breakable events hang off; the component is what the pad rides.
	Host = InHost ? InHost->GetOwner() : nullptr;
	BindToHost();
	if (InHost)
	{
		// To the component that was hit rather than the host's root, so a pad on a windmill plate
		// goes round with the plate. See the note on AGetawaySpikes::AttachToHost.
		//
		// Keeping the world transform matters: placement already worked out exactly where the pad
		// sits, and attaching must not move it - the pad's rotation *is* its launch direction, and
		// the direction it launches you has to keep pointing out of the face it is stuck to.
		AttachToComponent(InHost, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void AGetawayJumpPad::BindToHost()
{
	AGetawayBreakable* Breakable = Cast<AGetawayBreakable>(Host);
	if (!Breakable) return;

	// AddUnique, because this can run twice on a client: once in BeginPlay and again when Host
	// replicates in. Both OnBroken and OnRestored fire on every machine, so the pad keeps step
	// with the panel without anything of its own going over the wire.
	Breakable->OnBroken.AddUniqueDynamic(this, &AGetawayJumpPad::OnHostBroken);
	Breakable->OnRestored.AddUniqueDynamic(this, &AGetawayJumpPad::OnHostRestored);
}

void AGetawayJumpPad::OnHostBroken(AGetawayBreakable*)
{
	SetPresent(false);
}

void AGetawayJumpPad::OnHostRestored(AGetawayBreakable*)
{
	SetPresent(true);
}

void AGetawayJumpPad::SetPresent(bool bNew)
{
	if (bPresent == bNew) return;
	bPresent = bNew;

	SetActorHiddenInGame(!bPresent);
	Trigger->SetCollisionEnabled(bPresent ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}
