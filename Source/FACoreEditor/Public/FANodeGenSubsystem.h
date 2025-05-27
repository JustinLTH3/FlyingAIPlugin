// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Misc/SpinLock.h"
#include "Subsystems/WorldSubsystem.h"
#include "FANodeGenSubsystem.generated.h"

class UFABoundData;
class AFABound;

DECLARE_MULTICAST_DELEGATE(FFAOnNodeGenFinished)

//A dedicated thread for Node generation.
class FFANodeGenRunnable : public FRunnable
{
public:
	/**
	 *
	 * @param Path The path to store all generated data.
	 * @param World The world to generate nodes in.
	 * @param Bound The bound selected to generate nodes for.
	 * @param BoundData The bound data selected to store nodes connection data.
	 * @param InOnNodeGenFinished Callback delegate When Generation finished, used by system to clean up.
	 */
	FFANodeGenRunnable(FString Path, class UWorld* World, AFABound* Bound, UFABoundData* BoundData,
	                   const FFAOnNodeGenFinished::FDelegate& InOnNodeGenFinished);
	~FFANodeGenRunnable();
	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Exit() override;
	virtual void Stop() override;

protected:
	//Create a data table for storing nodes data.
	UDataTable* CreateNodeDataTable(FString InPath, FString Name);
	/**The world to generate nodes in.*/
	UWorld* World;
	/** The bound selected to generate nodes for. */
	AFABound* Bound;
	/** The bound data selected to store nodes connection data. */
	UFABoundData* BoundData;
	/** The thread to run on. */
	FRunnableThread* Thread;
	/** The path to store all generated data. */
	FString Path;
	//Data tables storing generating nodes data.
	TArray<UDataTable*> DataTables;
	/** Locks for data table access. */
	TArray<UE::FSpinLock*> DataTableLocks;
	/** All HPA Index that nodes have. Store in Bound Data. */
	TArray<AFABound*> HPAIndex;
	//Storing the start time of a node generation, used for calculating the generation time.
	FDateTime startGenTime;
	//Callback delegate When Generation finished, used by system to clean up.
	FFAOnNodeGenFinished OnNodeGenFinished;
};

/**
 * The World Subsystem to generate nodes for the bound. It will start a thread dedicated for node generation.
 * It will only generate for 1 bound at a time.
 */
UCLASS()
class FACOREEDITOR_API UFANodeGenSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	UFUNCTION()
	void GenerateBoundNodes(FString Path, UWorld* World, uint32 MaxDepth, AFABound* Bound,
	                        UFABoundData* BoundData = nullptr);
	virtual void BeginDestroy() override;

protected:
	//Only support Editor.
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	UPROPERTY(EditAnywhere, Category = "Pathfinding")
	TSubclassOf<AActor> EnvironmentActorClass;
	UPROPERTY(EditAnywhere, Category = "Pathfinding")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	// The thread dedicated for node generation.
	FFANodeGenRunnable* NodeGenRunnable = nullptr;
	//If the system is generating node, should not start another generation.
	bool bIsGeneratingNode{false};
	TFuture<bool> NodeGenFuture;
	UPROPERTY()
	//Storing the start time of a node generation, used for calculating the generation time.
	FDateTime startGenTime;
};
