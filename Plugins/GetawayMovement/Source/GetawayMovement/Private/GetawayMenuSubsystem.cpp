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

namespace
{
	/**
	 * "a.b.c.d" into four numbers. False for anything that is not a dotted IPv4 quad.
	 *
	 * IPv4 only: an IPv6 address is not something anybody is going to type into a box, and the
	 * launchers in Tools/ have always printed IPv4. Parsed into octets rather than matched on
	 * string prefixes, so that 172.2.x.x is not mistaken for the private 172.16-172.31 block.
	 */
	bool ParseIPv4(const FString& Text, int32(&OutOctet)[4])
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT("."), /*InCullEmpty=*/false);
		if (Parts.Num() != 4) return false;

		for (int32 i = 0; i < 4; ++i)
		{
			if (Parts[i].IsEmpty() || !Parts[i].IsNumeric()) return false;
			OutOctet[i] = FCString::Atoi(*Parts[i]);
			if (OutOctet[i] < 0 || OutOctet[i] > 255) return false;
		}
		return true;
	}

	/**
	 * What an address looks like it is for, from its range alone.
	 *
	 * The three mesh VPNs that docs/multiplayer.md recommends hand out addresses from fixed blocks,
	 * so they can be named with some confidence: Radmin is 26.x, Hamachi is 25.x, and Tailscale
	 * uses the carrier-grade NAT block 100.64.0.0/10. ZeroTier lets you choose your own range and
	 * so cannot be spotted this way - a ZeroTier address usually lands in 10.x and gets called a
	 * local one. That is a known soft spot and is why every address is listed rather than one
	 * being picked: a wrong label is survivable, a wrong choice made for you is not.
	 */
	FString NoteForAddress(const int32(&Octet)[4], bool& bOutIsVpn, bool& bOutIsLocal)
	{
		bOutIsVpn = false;
		bOutIsLocal = false;

		if (Octet[0] == 26)
		{
			bOutIsVpn = true;
			return TEXT("Radmin VPN - a friend somewhere else");
		}
		if (Octet[0] == 25)
		{
			bOutIsVpn = true;
			return TEXT("Hamachi - a friend somewhere else");
		}
		if (Octet[0] == 100 && Octet[1] >= 64 && Octet[1] <= 127)
		{
			bOutIsVpn = true;
			return TEXT("Tailscale - a friend somewhere else");
		}

		const bool bPrivate = (Octet[0] == 10)
			|| (Octet[0] == 192 && Octet[1] == 168)
			|| (Octet[0] == 172 && Octet[1] >= 16 && Octet[1] <= 31);

		if (bPrivate)
		{
			bOutIsLocal = true;
			return TEXT("this network - same house");
		}

		return FString();
	}
}

TArray<FGetawayLocalAddress> UGetawayMenuSubsystem::FindLocalAddresses()
{
	// Three buckets rather than a sort: there are never more than a handful of adapters, and the
	// order wanted is "VPN, then local, then whatever is left" with each bucket keeping the order
	// the OS gave it.
	TArray<FGetawayLocalAddress> Vpn;
	TArray<FGetawayLocalAddress> Local;
	TArray<FGetawayLocalAddress> Rest;

	ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!Sockets) return Vpn;

	TArray<TSharedPtr<FInternetAddr>> Addresses;
	if (!Sockets->GetLocalAdapterAddresses(Addresses)) return Vpn;

	TSet<FString> Seen;
	for (const TSharedPtr<FInternetAddr>& Addr : Addresses)
	{
		if (!Addr.IsValid()) continue;

		// Without the port, which is always 7777 and is not something to read down a phone.
		const FString Text = Addr->ToString(/*bAppendPort=*/false);

		int32 Octet[4] = { 0, 0, 0, 0 };
		if (!ParseIPv4(Text, Octet)) continue;

		if (Octet[0] == 127) continue;
		// Windows gives one of these to every adapter with nothing plugged into it.
		if (Octet[0] == 169 && Octet[1] == 254) continue;

		// An adapter can be reported twice; reading the same address out twice is just confusing.
		if (Seen.Contains(Text)) continue;
		Seen.Add(Text);

		bool bIsVpn = false;
		bool bIsLocal = false;
		FGetawayLocalAddress Found;
		Found.Address = Text;
		Found.Note = NoteForAddress(Octet, bIsVpn, bIsLocal);

		if (bIsVpn)        Vpn.Add(MoveTemp(Found));
		else if (bIsLocal) Local.Add(MoveTemp(Found));
		else               Rest.Add(MoveTemp(Found));
	}

	// A VPN address goes first because a machine only has one if somebody deliberately installed
	// it, and the reason to install it is a friend who is not in the house.
	Vpn.Append(MoveTemp(Local));
	Vpn.Append(MoveTemp(Rest));
	return Vpn;
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
