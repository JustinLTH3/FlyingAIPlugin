// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FABoundData.h"
#include "FANode.h"
#include "Misc/SpinLock.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tasks/Task.h"
#include "Async/Future.h"
#include "FAWorldSubsystem.generated.h"

class UFAPathfindingSettings;
class AFABound;
class UFABoundData;
class UFANewNode;
class UDataTable;
class UCompositeDataTable;
/**
 * 
 */
FACORE_API DECLARE_LOG_CATEGORY_EXTERN(LogFAWorldSubsystem, Log, Display);

USTRUCT(BlueprintType)
/**
 * @brief Wrapper of every data required for pathfinding for each node.
 */
struct FFAPathNodeData
{
	GENERATED_BODY()
	UPROPERTY()
	//The position should always be the real location instead of the generation location.
	FFaNodeData NodeData;
	UPROPERTY()
	//The key to access the node in the data table.
	FName NodeName;
	UPROPERTY()
	// The bound that the node is in.
	AFABound* NodeBound;

	bool operator==(const FFAPathNodeData& other) const
	{
		return NodeName == other.NodeName && NodeBound == other.NodeBound;
	}
};

USTRUCT(BlueprintType)
struct FFAHPAPath
{
	GENERATED_BODY()
	UPROPERTY()
	FFAPathNodeData StartNode;
	UPROPERTY()
	FFAPathNodeData EndNode;
	UPROPERTY()
	//The exact start location.
	FVector StartLocation;
	UPROPERTY()
	//The exact end location.
	FVector EndLocation;
	UPROPERTY()
	TArray<AFABound*> HPAAssociateBounds;
	UPROPERTY()
	//The HPA* searched Path.
	TArray<uint32> HPANodes;
	UPROPERTY()
	bool bIsSuccess{false};
};

USTRUCT(BlueprintType)
struct FFAFinePath
{
	GENERATED_BODY()
	FFAHPAPath HPAPath;
	TArray<FFAPathNodeData> Nodes;
	UPROPERTY()
	FFAPathNodeData LocalStartNode;
	UPROPERTY()
	FVector LocalStartLocation;
	UPROPERTY()
	TArray<FVector> ControlPoints;
	UPROPERTY()
	TArray<FVector> InterpolatedPoints;
	UPROPERTY()
	int CurrentHPANodeIndex;
	UPROPERTY()
	bool bIsSuccess{false};
	UPROPERTY()
	/** Indicate whether the path cannot be found is because of the bound is not loaded or not. */
	bool bBoundLoaded{true};
};

USTRUCT(BlueprintType)
struct FFANewNodeChildType
{
	GENERATED_BODY()
	TSharedPtr<FFaNodeData> Children[8];
	FName ChildrenName[8];
};

DECLARE_MULTICAST_DELEGATE(FFAOnBackToMainThread)
DECLARE_MULTICAST_DELEGATE(FFAOnSystemReady)

namespace FA
{
	static __readonly TArray<uint8> XPositiveChildrenIndex{1, 3, 5, 7};
	static __readonly TArray<uint8> XNegativeChildrenIndex{2, 3, 6, 7};
	static __readonly TArray<uint8> YPositiveChildrenIndex{4, 5, 6, 7};
	static __readonly TArray<uint8> YNegativeChildrenIndex{0, 2, 4, 6};
	static __readonly TArray<uint8> ZPositiveChildrenIndex{0, 1, 4, 5};
	static __readonly TArray<uint8> ZNegativeChildrenIndex{0, 1, 2, 3};
}

UCLASS()
class FACORE_API UFAWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	uint8 GenerateNodeBranch(const UFABoundData* BoundData, UWorld* World,
	                         TSharedPtr<FFaNodeData> Node, FName NodeName, int nodeIndex,
	                         UE::FSpinLock* DataLock, UDataTable* Data);
	void SetHPAIndex(UDataTable* CombinedNodes, UDataTable* DataTable,
	                 TArray<AFABound*>& InHPAIndex, UE::FSpinLock& _csHPAIndex);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//Subdividing node into 8 octants.
	static void Subdivide(TSharedPtr<FFaNodeData> Node, FFANewNodeChildType& Children);

	virtual void BeginDestroy() override;
	UFUNCTION(BlueprintCallable, Category = "FA|WorldSubsystem")
	void RegisterBoundInWorld(AFABound* Bound);

protected:
	UFUNCTION()
	void SetBoundNeighbour(AFABound* Bound0, AFABound* Bound1);
	UFUNCTION()
	void RegisterBoundInWorldStartUp();
	UPROPERTY(BlueprintReadOnly, Category = "FA|WorldSubsystem")
	TArray<AFABound*> HPAIndex;
	UE::FSpinLock csHPAIndex;

	TSubclassOf<class UFAPathfindingAlgo> PathfindingAlgoClass;
	UPROPERTY(BlueprintReadOnly, Category = "FA|WorldSubsystem")
	/** The instance used for pathfinding. */
	UFAPathfindingAlgo* PathfindingAlgo;

	TArray<UE::Tasks::FTask> SetHPATasks;
	UPROPERTY()
	TArray<AActor*> ActorsToIgnore;

public:
	UFUNCTION(BlueprintCallable, Category = "FA|WorldSubsystem")
	//Both points have to be in LOD 0, 1 bounds which have nodesData loaded.
	FFAHPAPath CreateHPAPath(const FVector& StartLocation, const FVector& EndLocation);
	UFUNCTION(BlueprintCallable, Category = "FA|WorldSubsystem")
	//Blocks thread and may cause short-freeze. Intended to not run on game thread.
	FFAFinePath CreateNextFinePath(const FFAFinePath& InFinePath,
	                               const FVector& ColliderSize = FVector::ZeroVector,
	                               const FVector& ColliderOffset = FVector::ZeroVector);
	UFUNCTION(BlueprintCallable, Category = "FA|WorldSubsystem")
	//Blocks thread and may cause short-freeze. Intended to not run on game thread.
	FFAFinePath CreateFinePathByHPA(FFAHPAPath HPAPath, FVector ColliderSize = FVector::ZeroVector,
	                                const FVector& ColliderOffset = FVector::ZeroVector);

	/**
	 * @brief Create a path from an HPA*-searched path, which should be the beginning of the path.
	 * @param HPAPath An HPA*-searched path to create from.
	 * @param ColliderSize The size of the Collider.
	 */
	TFuture<FFAFinePath> CreateFinePathByHPAAsync(FFAHPAPath& HPAPath,
	                                              const FVector& ColliderSize = FVector::ZeroVector,
	                                              const FVector& ColliderOffset =
		                                              FVector::ZeroVector);
	/**
	 * @brief Create a path from an existing path.
	 * @param InFinePath The existing path to create from.
	 * @param ColliderSize The size of the Collider.
	 */
	TFuture<FFAFinePath> CreateNextFinePathAsync(const FFAFinePath& InFinePath,
	                                             const FVector& ColliderSize,
	                                             const FVector& ColliderOffset =
		                                             FVector::ZeroVector);
	UFUNCTION(BlueprintCallable, Category = "FA|WorldSubsystem")
	void InterpolateFinePath(FFAFinePath& InFinePath);

	UFUNCTION(BlueprintCallable, Category = "FA|WorldSubsystem")
	FFAPathNodeData PointToNodeInBound(FVector Point, AFABound* Bound);

	//Used for generation only.
	static bool IsNodeOverlapping(FFaNodeData* NodeData,
	                              const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
	                              TSubclassOf<AActor> ActorClassToConsider,
	                              const TArray<AActor*>& ActorsToIgnore, UWorld* World);
	FFAOnSystemReady& GetOnSystemReady() { return OnSystemReady; }
	UFUNCTION()
	TArray<AFABound*>& GetRegisteredBound()
	{
		UE::TScopeLock Lock(RegisteredBoundLock);
		return RegisteredBound;
	}

	UFUNCTION(BlueprintCallable, Category = "FA|WorldSubsystem")
	class UFANeighbourData* GetNeighbourData(FString Key);
	UFUNCTION(BlueprintCallable, Category = "FA|WorldSubsystem")
	bool GetGameSystemReady()
	{
		UE::TScopeLock Lock(OnSystemReadyLock);
		return GameSystemReady;
	}

	class FQueuedThreadPool* GetThreadPool()
	{
		UE::TScopeLock Lock(ThreadPoolLock);
		return ThreadPool;
	}

protected:
	UFUNCTION()
	FFAHPAPath InternalCreateHPAPath(FVector StartLocation, FVector EndLocation,
	                                 TArray<AFABound*> Bounds);
	UPROPERTY()
	TArray<AFABound*> RegisteredBound;
	FCriticalSection RegisteredBoundLock;
	UPROPERTY()
	TMap<uint32, FFAConnectedHPANode> HPAConnection;
	FCriticalSection HPAConnectionLock;
	UPROPERTY()
	TMap<FString, TWeakObjectPtr<UFANeighbourData>> NeighboursData;
	UE::FSpinLock NeighboursDataLock;
	UPROPERTY()
	//Is the Subsystem ready for use in game.
	bool GameSystemReady = false;
	FFAOnSystemReady OnSystemReady;
	UE::FSpinLock OnSystemReadyLock;
	UPROPERTY()
	UFAPathfindingSettings* Settings;
	FQueuedThreadPool* ThreadPool = nullptr;
	UE::FSpinLock ThreadPoolLock;
	static bool AABBOverlap(FVector P1, FVector P2, FVector H1, FVector H2);
};
