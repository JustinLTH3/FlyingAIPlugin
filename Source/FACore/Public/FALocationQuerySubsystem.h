// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FALevelData.h"
#include "Subsystems/WorldSubsystem.h"
#include "FALocationQuerySubsystem.generated.h"

class UFAPathfindingSettings;
class UFAWorldSubsystem;
/**
 * 
 */
UCLASS(BlueprintType)
class FACORE_API UFALocationQuerySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	UFUNCTION(BlueprintCallable, Category = "FA|LocationQuery")
	static FVector GetNullValue() { return NullValue; }

protected:
	UPROPERTY()
	TSubclassOf<AActor> EnvironmentActorClass;
	UPROPERTY()
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;

	static FVector NullValue;
	std::atomic<bool> bReady{false};

public:
	UFUNCTION(BlueprintCallable, Category = "FA|LocationQuery")
	//Will not get location from Bounds that is not loaded, so result will always not in LOD2 Bound.
	FVector GetRandomReachableLocation(FVector ColliderSize = FVector::ZeroVector,
	                                   FVector ColliderOffset = FVector::ZeroVector,
	                                   int MaxSamplings = 1000);
};
