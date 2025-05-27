// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FAWorldSubsystem.h"
#include "FANode.h"
#include "UObject/Object.h"
#include "FAPathfindingAlgo.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FAPathfindingData
{
	GENERATED_BODY()
	FAPathfindingData(const FFAPathNodeData& Data = FFAPathNodeData(),
	                  const FVector& StartLocation = FVector::Zero(),
	                  const FVector2D& Cost = FVector2D::Zero())
		: Data(Data),
		  StartLocation(StartLocation),
		  Cost(Cost)
	{
	};
	FFAPathNodeData Data;
	FVector StartLocation;
	FVector2D Cost;
};

UCLASS()
class FACORE_API UFAPathfindingAlgo : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "FA|Pathfinding")
	virtual void GeneratePath(struct FFAFinePath& FinePath, const struct FFAPathNodeData& EndNode,
	                          UWorld* World, const UFAPathfindingSettings* Settings,
	                          FVector ColliderSize = FVector::ZeroVector,
	                          const FVector ColliderOffset = FVector::ZeroVector) const;

	UFUNCTION(BlueprintCallable, Category = "FA|Pathfinding")
	static bool IsGenerating()
	{
		UE::TScopeLock Lock(PathGenCalledNumLock);
		return PathGenCalledNum > 0;
	}

private:
	//Should be replaced by terminating thread. Task cannot be aborted and therefore this is here for preventing null bound pointer.
	static uint32 PathGenCalledNum;
	static UE::FSpinLock PathGenCalledNumLock;
};
