#include "GetawayGameState.h"

#include "GetawayPlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

AGetawayGameState::AGetawayGameState()
{
	// The clocks are read off this, so it wants to be tighter than the 0.1 s default would give on
	// a laggy join. Cheap: it is one float on one actor.
	ServerWorldTimeSecondsUpdateFrequency = 0.2f;
}

void AGetawayGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AGetawayGameState, Phase);
	DOREPLIFETIME(AGetawayGameState, RoundNumber);
	DOREPLIFETIME(AGetawayGameState, bRoundOver);
	DOREPLIFETIME(AGetawayGameState, PlacementEndServerTime);
	DOREPLIFETIME(AGetawayGameState, RunStartServerTime);
	DOREPLIFETIME(AGetawayGameState, RunEndServerTime);
	DOREPLIFETIME(AGetawayGameState, bGraceRunning);
	DOREPLIFETIME(AGetawayGameState, RoundEndRunTime);
	DOREPLIFETIME(AGetawayGameState, BestTime);
}

float AGetawayGameState::Now() const
{
	// On the server this is just world time. On a client it is the server's clock, tracked locally
	// between updates, which is what keeps a countdown smooth instead of stepping once a packet.
	return static_cast<float>(GetServerWorldTimeSeconds());
}

/* ============================================================ phase */

void AGetawayGameState::SetPhase(EGetawayPhase InPhase)
{
	Phase = InPhase;
}

void AGetawayGameState::SetRoundOver(bool bOver)
{
	bRoundOver = bOver;
	if (bOver)
	{
		RoundEndRunTime = FMath::Max(0.f, Now() - RunStartServerTime);
	}
}

/* ============================================================ the build clock */

void AGetawayGameState::BeginPlacementClock(float Seconds)
{
	PlacementEndServerTime = (Seconds > 0.f) ? Now() + Seconds : -1.f;
}

bool AGetawayGameState::IsPlacementTimed() const
{
	return Phase == EGetawayPhase::Placement && PlacementEndServerTime >= 0.f;
}

float AGetawayGameState::GetPlacementTimeLeft() const
{
	if (!IsPlacementTimed()) return 0.f;
	return FMath::Max(0.f, PlacementEndServerTime - Now());
}

/* ============================================================ the run clock */

void AGetawayGameState::BeginRunClock()
{
	RunStartServerTime = Now();
	RunEndServerTime = -1.f;
	bGraceRunning = false;
	RoundEndRunTime = 0.f;
}

void AGetawayGameState::SetRunDeadline(float Seconds, bool bIsGrace)
{
	if (Seconds <= 0.f) return;
	const float When = Now() + Seconds;
	// Never push a deadline back: the grace after the first finish may only bring the end nearer.
	if (RunEndServerTime >= 0.f && When >= RunEndServerTime)
	{
		bGraceRunning = bGraceRunning || bIsGrace;
		return;
	}
	RunEndServerTime = When;
	bGraceRunning = bIsGrace;
}

void AGetawayGameState::ClearRunDeadline()
{
	RunEndServerTime = -1.f;
	bGraceRunning = false;
}

bool AGetawayGameState::IsRunTimed() const
{
	return Phase == EGetawayPhase::Run && !bRoundOver && RunEndServerTime >= 0.f;
}

float AGetawayGameState::GetRunTimeLeft() const
{
	if (!IsRunTimed()) return 0.f;
	return FMath::Max(0.f, RunEndServerTime - Now());
}

float AGetawayGameState::GetRunTime() const
{
	if (Phase != EGetawayPhase::Run) return 0.f;
	if (bRoundOver) return RoundEndRunTime;
	return FMath::Max(0.f, Now() - RunStartServerTime);
}

void AGetawayGameState::ReportFinishTime(float Seconds)
{
	if (Seconds < 0.f) return;
	if (BestTime < 0.f || Seconds < BestTime)
	{
		BestTime = Seconds;
	}
}

/* ============================================================ standings */

TArray<AGetawayPlayerState*> AGetawayGameState::GetStandings() const
{
	TArray<AGetawayPlayerState*> Out;
	Out.Reserve(PlayerArray.Num());
	for (APlayerState* PS : PlayerArray)
	{
		if (AGetawayPlayerState* GPS = Cast<AGetawayPlayerState>(PS))
		{
			Out.Add(GPS);
		}
	}
	// Name breaks the tie so the list does not reshuffle under you between frames.
	Out.Sort([](const AGetawayPlayerState& A, const AGetawayPlayerState& B)
	{
		if (A.GetPoints() != B.GetPoints()) return A.GetPoints() > B.GetPoints();
		return A.GetPlayerName().Compare(B.GetPlayerName()) < 0;
	});
	return Out;
}

int32 AGetawayGameState::GetLeadingScore() const
{
	int32 Best = 0;
	bool bAny = false;
	for (APlayerState* PS : PlayerArray)
	{
		if (const AGetawayPlayerState* GPS = Cast<AGetawayPlayerState>(PS))
		{
			if (!bAny || GPS->GetPoints() > Best)
			{
				Best = GPS->GetPoints();
				bAny = true;
			}
		}
	}
	return bAny ? Best : 0;
}
