// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FALevelData.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMeshActor.h"
#include "FABoundData.generated.h"

class UCompositeDataTable;
/**
 *
 */
UCLASS(BlueprintType)
class FACORE_API UFABoundData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "FA|BoundData")
	TArray<TSoftObjectPtr<AActor>> ActorsToIgnore;
	UPROPERTY(VisibleAnywhere, Category = "FA|BoundData")
	TSoftObjectPtr<UCompositeDataTable> CombinedNodes;
	//Connection between HPANodes.
	UPROPERTY(VisibleAnywhere, Category = "FA|BoundData")
	TMap<uint32, FFAConnectedHPANode> InternalHPAConnection;
	UPROPERTY(VisibleAnywhere, Category = "FA|BoundData")
	TArray<uint32> ContainingHPANodes;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FA|BoundData")
	FVector GeneratePosition;
	UPROPERTY(VisibleAnywhere, Category = "FA|BoundData")
	uint32 MaxDepth;
};
