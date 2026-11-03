#include "GetawayMenuSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"

void UGetawayMenuSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GEngine)
	{
		NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(this, &UGetawayMenuSubsystem::OnNetworkFailure);
		TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(this, &UGetawayMenuSubsystem::OnTravelFailure);
	}
}

void UGetawayMenuSubsystem::Deinitialize()
{
	if (GEngine)
	{
		GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
		GEngine->OnTravelFailure().Remove(TravelFailureHandle);
	}
	Super::Deinitialize();
}

FString UGetawayMenuSubsystem::TakeFailure()
{
	FString Reason = MoveTemp(LastFailure);
	LastFailure.Empty();
	return Reason;
}

void UGetawayMenuSubsystem::OnNetworkFailure(UWorld* World, UNetDriver* NetDriver,
	ENetworkFailure::Type FailureType, const FString& Error)
{
	LastFailure = Explain(FailureType, Error);
	UE_LOG(LogTemp, Warning, TEXT("Getaway: network failure (%s): %s"),
		ENetworkFailure::ToString(FailureType), *Error);
}

void UGetawayMenuSubsystem::OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error)
{
	switch (FailureType)
	{
	case ETravelFailure::InvalidURL:
		LastFailure = TEXT("That is not an address the game can use.");
		break;
	case ETravelFailure::PackageVersion:
	case ETravelFailure::PackageMissing:
	case ETravelFailure::NoDownload:
		// Worth naming precisely: everyone has to be on the same package, and it is the first
		// thing that goes wrong when a build is shared round. See docs/multiplayer.md.
		LastFailure = TEXT("That host is running a different build of the game.");
		break;
	default:
		LastFailure = TEXT("Could not get into that game.");
		break;
	}

	UE_LOG(LogTemp, Warning, TEXT("Getaway: travel failure (%s): %s"),
		ETravelFailure::ToString(FailureType), *Error);
}

FString UGetawayMenuSubsystem::Explain(ENetworkFailure::Type FailureType, const FString& Error) const
{
	switch (FailureType)
	{
	case ENetworkFailure::ConnectionTimeout:
	case ENetworkFailure::PendingConnectionFailure:
		// Overwhelmingly the common one, and almost always one of three things. The firewall is
		// listed first because it is the usual culprit - see *Hosting and joining* in
		// docs/multiplayer.md.
		return TEXT("No answer. Check the address, that the host is in a game, and that they allowed the game through Windows Firewall.");
	case ENetworkFailure::ConnectionLost:
		return TEXT("Lost the connection to the host.");
	case ENetworkFailure::OutdatedClient:
	case ENetworkFailure::OutdatedServer:
	case ENetworkFailure::NetChecksumMismatch:
	case ENetworkFailure::NetGuidMismatch:
		return TEXT("That host is running a different build of the game.");
	case ENetworkFailure::NetDriverListenFailure:
		return TEXT("Could not open the port to host on. Is another copy of the game already hosting?");
	case ENetworkFailure::FailureReceived:
		return Error.IsEmpty() ? FString(TEXT("The host turned the connection down.")) : Error;
	default:
		return TEXT("The connection failed.");
	}
}
