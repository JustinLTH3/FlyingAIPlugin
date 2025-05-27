// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FALevelData.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct FFAConnectedHPANode
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, Category = "FA")
	TArray<uint32> Values;
};
