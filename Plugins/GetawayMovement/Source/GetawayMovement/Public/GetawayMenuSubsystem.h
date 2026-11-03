#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GetawayMenuSubsystem.generated.h"

class UNetDriver;

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
