// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Engine/DataTable.h"
#include "FANode.generated.h"

USTRUCT(BlueprintType)
struct FFaNodeData : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FA|NodeData")
	FVector Position;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FA|NodeData")
	FVector HalfExtent;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FA|NodeData")
	TArray<FName> Neighbour;
	UPROPERTY(VisibleAnywhere, Category = "FA|NodeData")
	uint32 Depth;
	UPROPERTY(VisibleAnywhere, Category = "FA|NodeData")
	uint32 HPANodeIndex = -1;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FA|NodeData")
	bool IsTraversable = false;

	bool operator==(const FFaNodeData& other) const
	{
		return Position == other.Position && HalfExtent == other.HalfExtent && Depth == other.Depth
			&& IsTraversable == other.IsTraversable && HPANodeIndex == other.HPANodeIndex;
	}
};