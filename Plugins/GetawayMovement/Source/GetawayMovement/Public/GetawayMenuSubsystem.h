#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GetawayMenuSubsystem.generated.h"

class UNetDriver;

/**
 * One address this machine can be reached on, and what it is good for.
 *
 * There is normally more than one - a home LAN address, plus one for every mesh-VPN adapter - and
 * which of them a friend needs depends entirely on where that friend is sitting. The game cannot
 * know that, so it shows all of them and says what each one looks like. See *Reading out the right
 * address* in docs/multiplayer.md.
 */
USTRUCT(BlueprintType)
struct FGetawayLocalAddress
{
	GENERATED_BODY()

	/** The address itself, without the port. */
	UPROPERTY(BlueprintReadOnly, Category = "Getaway|Menu")
	FString Address;

	/** What it is for, in a few words, or empty when there is nothing useful to say about it. */
	UPROPERTY(BlueprintReadOnly, Category = "Getaway|Menu")
	FString Note;
};

/**
 * The two things the front end has to remember across a level change: why the last join failed,
 * and what you typed into the menu's boxes.
 *
 * It is a game instance subsystem because it has to outlive the travel itself. A failed join
 * destroys the world, the player controller and the menu widget, so a message stored on any of
 * those three is gone by the time there is a menu to show it on.
 *
 * It does not travel anywhere itself, and does not need to. The engine already does that:
 * UEngine::HandleNetworkFailure and HandleTravelFailure both end in HandleDisconnect, which sets
 * client travel to "?closed", and that browses back to GameDefaultMap - TestCourse, with no
 * options, which is the menu. Checked in UnrealEngine.cpp, 2026-11-02. All this has to do is make
 * sure the player is told why.
 */
UCLASS()
class GETAWAYMOVEMENT_API UGetawayMenuSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** The reason the last connection fell over, in words, or empty if there is nothing to say. */
	UFUNCTION(BlueprintPure, Category = "Getaway|Menu")
	const FString& PeekFailure() const { return LastFailure; }

	/** Read the reason and forget it, so it shows once rather than on every visit to the menu. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Menu")
	FString TakeFailure();

	/** Say something on the menu's status line without a network failure behind it. */
	UFUNCTION(BlueprintCallable, Category = "Getaway|Menu")
	void SetFailure(const FString& Reason) { LastFailure = Reason; }

	/**
	 * Every address somebody else could type into Join, best guess first, or empty if there isn't
	 * a sensible one at all. The menu lists them so the host can read the right one out - which is
	 * the job `Tools/Host-Adrenaline.bat` used to do by printing the lot from `ipconfig`, and
	 * nothing was doing once people stopped launching through the `.bat` file.
	 *
	 * **All of them, not just the best one.** This used to return a single address and prefer a
	 * private LAN one, which is exactly wrong once a mesh VPN is installed to play with somebody in
	 * another city: the VPN address is the one that friend needs, and the 192.168 address that got
	 * returned instead is of no use to them whatsoever. Only the person reading the address out
	 * knows where their friend is, so the choice belongs to them.
	 *
	 * Link-local 169.254.x addresses are skipped: Windows hands them to every unplugged or virtual
	 * adapter, and this machine has three of them to one real one. Loopback is skipped too.
	 */
	UFUNCTION(BlueprintPure, Category = "Getaway|Menu")
	static TArray<FGetawayLocalAddress> FindLocalAddresses();

	/* ---------------- What was typed into the menu ---------------- */

	/** Kept so leaving a game and coming back does not mean typing your name in again. */
	UPROPERTY(BlueprintReadWrite, Category = "Getaway|Menu")
	FString PlayerName;

	/** The last address joined or typed, for the same reason. */
	UPROPERTY(BlueprintReadWrite, Category = "Getaway|Menu")
	FString LastAddress;

private:
	void OnNetworkFailure(UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& Error);
	void OnTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);

	/** The engine's reason is a developer string. This turns the common ones into a sentence. */
	FString Explain(ENetworkFailure::Type FailureType, const FString& Error) const;

	FString LastFailure;

	FDelegateHandle NetworkFailureHandle;
	FDelegateHandle TravelFailureHandle;
};
