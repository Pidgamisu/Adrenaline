#include "GetawayPlayerState.h"

#include "Net/UnrealNetwork.h"

AGetawayPlayerState::AGetawayPlayerState()
{
	// The hand changes a few times a round and the score once; neither wants the default 100 ms.
	SetNetUpdateFrequency(10.f);
}

void AGetawayPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Your hand is yours: a PlayerState is owned by its controller, so COND_OwnerOnly sends it to
	// that player alone. Everything else is public, because the scoreboard shows it.
	DOREPLIFETIME_CONDITION(AGetawayPlayerState, Hand, COND_OwnerOnly);
	DOREPLIFETIME(AGetawayPlayerState, RunState);
	DOREPLIFETIME(AGetawayPlayerState, bDonePlacing);
	DOREPLIFETIME(AGetawayPlayerState, FinishTime);
	DOREPLIFETIME(AGetawayPlayerState, FinishPlace);
	DOREPLIFETIME(AGetawayPlayerState, BestTime);
	DOREPLIFETIME(AGetawayPlayerState, Points);
	DOREPLIFETIME(AGetawayPlayerState, LastRoundPoints);
	DOREPLIFETIME(AGetawayPlayerState, TrapKills);
}

void AGetawayPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
	Super::CopyProperties(NewPlayerState);
	// Seamless travel and reconnects rebuild the PlayerState, and losing the score to that would be
	// worse than any of it. The hand is deliberately not carried: a new round deals a fresh one.
	if (AGetawayPlayerState* New = Cast<AGetawayPlayerState>(NewPlayerState))
	{
		New->Points = Points;
		New->BestTime = BestTime;
	}
}

/* ============================================================ the hand */

bool AGetawayPlayerState::GetHandPiece(int32 Slot, FGetawayPieceDef& OutDef) const
{
	if (!Hand.IsValidIndex(Slot)) return false;
	OutDef = Hand[Slot];
	return OutDef.IsValid();
}

void AGetawayPlayerState::SetHand(const TArray<FGetawayPieceDef>& InHand)
{
	if (!HasAuthority()) return;
	Hand = InHand;
}

bool AGetawayPlayerState::TakeFromHand(int32 Slot)
{
	if (!HasAuthority() || !Hand.IsValidIndex(Slot)) return false;
	Hand.RemoveAt(Slot);
	return Hand.Num() == 0;
}

/* ============================================================ the round */

void AGetawayPlayerState::BeginRound()
{
	if (!HasAuthority()) return;
	RunState = EGetawayRunState::Building;
	bDonePlacing = false;
	FinishTime = -1.f;
	FinishPlace = 0;
	TrapKills = 0;
}

void AGetawayPlayerState::SetRunState(EGetawayRunState InState)
{
	if (!HasAuthority()) return;
	RunState = InState;
}

void AGetawayPlayerState::SetDonePlacing(bool bValue)
{
	if (!HasAuthority()) return;
	bDonePlacing = bValue;
}

void AGetawayPlayerState::MarkFinished(float Seconds, int32 Place)
{
	if (!HasAuthority()) return;
	RunState = EGetawayRunState::Finished;
	FinishTime = FMath::Max(0.f, Seconds);
	FinishPlace = Place;
	if (BestTime < 0.f || FinishTime < BestTime)
	{
		BestTime = FinishTime;
	}
}

void AGetawayPlayerState::AwardPoints(int32 Delta)
{
	if (!HasAuthority()) return;
	Points = FMath::Max(0, Points + Delta);
	// APlayerState keeps its own Score, which engine scoreboards and Blueprints read. Mirror it so
	// the two cannot disagree.
	SetScore(static_cast<float>(Points));
}
