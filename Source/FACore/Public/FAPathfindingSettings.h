// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FAPathfindingAlgo.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/Object.h"
#include "FAPathfindingSettings.generated.h"

/**
 * 
 */

USTRUCT(BlueprintType)
struct FFAMapSettings
{
	GENERATED_BODY()
	UPROPERTY(Config, EditAnywhere, Category = "Location Query")
	bool bUseLocationQuery;
};

UCLASS(DefaultConfig= "FAPathfindingSettings", config = Project, meta = (DisplayName = "Pathfinding Settings", ConfigExtension = "FAPathfindingSettings"))
class FACORE_API UFAPathfindingSettings : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Pathfinding")
	TSubclassOf<UFAPathfindingAlgo> PathfindingAlgorithmToUse = UFAPathfindingAlgo::StaticClass();
	UPROPERTY(Config, EditAnywhere, Category = "Pathfinding")
	TSubclassOf<AActor> EnvironmentActorClass = AStaticMeshActor::StaticClass();
	UPROPERTY(Config, EditAnywhere, Category = "Pathfinding")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	UPROPERTY(Config, EditAnywhere, Category = "Pathfinding")
	TMap<TSoftObjectPtr<UWorld>, FFAMapSettings> MapsSettings;
};
