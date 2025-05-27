// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "FANeighbourData.generated.h"

class AFABound;
/**
 *
 */
USTRUCT()
struct FNeighbourBoundConnected
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, Category = "FA|NeighbourData")
	TArray<FName> Connected;
};

UCLASS()
class FACORE_API UFANeighbourData : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "FA|NeighbourData")
	TObjectPtr<AFABound> Bound[2];
	UPROPERTY(VisibleAnywhere, Category = "FA|NeighbourData")
	TMap<FName, FNeighbourBoundConnected> Connection0;
	UPROPERTY(VisibleAnywhere, Category = "FA|NeighbourData")
	TMap<FName, FNeighbourBoundConnected> Connection1;
};
