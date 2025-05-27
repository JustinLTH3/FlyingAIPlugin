// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FABoundData.h"
#include "Components/BoxComponent.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "Misc/SpinLock.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "FABound.generated.h"

class UFANeighbourData;

UCLASS()
class FACORE_API AFABound : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AFABound();
	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	FVector GetHalfExtent() const { return BoxComponent->GetScaledBoxExtent(); }

	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	void Subdivide(struct FFANewNodeChildType& ChildrenNodes);

	TSharedPtr<FStreamableHandle> GetNodeData();

	TObjectPtr<UDataTable> GetNodesData()
	{
		UE::TScopeLock Lock(NodesDataLock);
		return NodesData;
	}

	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	void SetLOD(uint8 InLOD);
	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	void OnLODChanged();
	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	TSoftObjectPtr<UFABoundData> GetBoundDataSoft() const { return BoundDataSoft; }

	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	UFABoundData* GetBoundData() const { return BoundData; }

	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	void SetBoundDataSoft(const TSoftObjectPtr<UFABoundData>& InBoundDataSoft)
	{
		if (BoundDataSoft == InBoundDataSoft) return;
		BoundDataSoft = InBoundDataSoft;
		BoundData = nullptr;
	}

	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	void SetBoundData(UFABoundData* InBoundData)
	{
		BoundData = InBoundData;
		BoundDataSoft = InBoundData;
	}

	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	void LoadBoundData() { BoundData = BoundDataSoft.LoadSynchronous(); }

	UFUNCTION()
	//Can't expose to blueprint due to uint32. May consider changing to int.
	TMap<uint32, uint32>& GetLocalToGlobalHPANodes()
	{
		return LocalToGlobalHPANodes;
	}

	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	/** Load Nodes data associate with the bound sync. */
	void LoadNodes();
	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	//Load Nodes data Associate with the bound async.Needs to Create a blueprint wrapper for this function to have callback.
	void LoadNodesAsync();
	UFUNCTION(BlueprintCallable, Category = "FA|Bound")
	/** Unload Nodes data associate with the bound sync. */
	void UnloadNodes();

	void AddNeighbourData(FString InName, UFANeighbourData* Data = nullptr);
	UFANeighbourData* FindNeighboursData(AFABound* Bound0, AFABound* Bound1);
	FCriticalSection& GetNodesDataLock() { return NodesDataLock; }

protected:
	UPROPERTY(EditAnywhere, Category = "FA|Bound")
	UBoxComponent* BoxComponent;
	UPROPERTY(EditAnywhere, Category = "FA|Bound")
	TSoftObjectPtr<UFABoundData> BoundDataSoft;
	//The data table holding the nodes data. Reset when unload.
	TSharedPtr<FStreamableHandle> LoadedDataHandle;
	UPROPERTY()
	TObjectPtr<UDataTable> NodesData;
	UPROPERTY()
	//Should not unload in anytime unless new Bound Data Soft is set.
	UFABoundData* BoundData;
	UPROPERTY(EditInstanceOnly, Category = "FA|Bound", meta = (ClampMax = 2))
	uint8 LOD{0};
	UPROPERTY()
	//To access the local hpa node index for instancing by the subsystem.
	//Should be init when spawned on game start.
	TMap<uint32, uint32> LocalToGlobalHPANodes;
	UPROPERTY()
	//The Connections to nodes in different nodes. The key is the save slot name.
	TMap<FString, TObjectPtr<UFANeighbourData>> NeighboursData;
	//Mutex for accessing the nodes' data.
	FCriticalSection NodesDataLock;
};
