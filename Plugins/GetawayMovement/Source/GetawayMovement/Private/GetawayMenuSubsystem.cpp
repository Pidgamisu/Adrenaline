#include "GetawayMenuSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "IPAddress.h"
#include "SocketSubsystem.h"

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

FString UGetawayMenuSubsystem::FindLanAddress()
{
	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Sockets) return FString();

	TArray<TSharedPtr<FInternetAddr>> Addresses;
	if (!Sockets->GetLocalAdapterAddresses(Addresses)) return FString();

	FString Fallback;
	for (const TSharedPtr<FInternetAddr>& Addr : Addresses)
	{
		if (!Addr.IsValid()) continue;

		// Without the port, which is always 7777 and is not something to read down a phone.
		const FString Text = Addr->ToString(/*bAppendPort=*/false);

		// IPv4 only: an IPv6 address is not something anybody is going to type into a box, and
		// the launchers in Tools/ have always printed IPv4. Parsed into octets rather than matched
		// on prefixes, so that 172.2.x.x is not mistaken for the private 172.16-172.31 block.
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT("."), /*InCullEmpty=*/false);
		if (Parts.Num() != 4) continue;

		int32 Octet[4] = { 0, 0, 0, 0 };
		bool bNumeric = true;
		for (int32 i = 0; i < 4; ++i)
		{
			if (Parts[i].IsEmpty() || !Parts[i].IsNumeric()) { bNumeric = false; break; }
			Octet[i] = FCString::Atoi(*Parts[i]);
			if (Octet[i] < 0 || Octet[i] > 255) { bNumeric = false; break; }
		}
		if (!bNumeric) continue;

		if (Octet[0] == 127) continue;
		// Windows gives one of these to every adapter with nothing plugged into it.
		if (Octet[0] == 169 && Octet[1] == 254) continue;

		const bool bPrivate = (Octet[0] == 10)
			|| (Octet[0] == 192 && Octet[1] == 168)
			|| (Octet[0] == 172 && Octet[1] >= 16 && Octet[1] <= 31);

		if (bPrivate) return Text;
		if (Fallback.IsEmpty()) Fallback = Text;
	}

	return Fallback;
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
