// Fill out your copyright notice in the Description page of Project Settings.

#include "FANodeGenSubsystem.h"

#include "AssetToolsModule.h"
#include "Engine/World.h"
#include "FABound.h"
#include "FAPathfindingSettings.h"
#include "FAWorldSubsystem.h"
#include "FileHelpers.h"
#include "Engine/CompositeDataTable.h"
#include "Factories/CompositeDataTableFactory.h"
#include "Factories/DataAssetFactory.h"
#include "Factories/DataTableFactory.h"

FFANodeGenRunnable::FFANodeGenRunnable(FString Path, UWorld* World, AFABound* Bound,
                                       UFABoundData* BoundData,
                                       const FFAOnNodeGenFinished::FDelegate& InOnNodeGenFinished)
	: World(World),
	  Bound(Bound),
	  BoundData(BoundData),
	  Path(Path)
{
	Thread = FRunnableThread::Create(this, TEXT("FANodeGenThread"));
	OnNodeGenFinished.Add(InOnNodeGenFinished);
}

FFANodeGenRunnable::~FFANodeGenRunnable()
{
	if (Thread)
	{
		Thread->Kill();
		delete Thread;
		Thread = nullptr;
	}
	for (auto Lock : DataTableLocks)
	{
		delete Lock;
	}
}

bool FFANodeGenRunnable::Init()
{
	return Bound != nullptr && BoundData != nullptr;
}

uint32 FFANodeGenRunnable::Run()
{
	startGenTime = FDateTime::UtcNow();
	FFANewNodeChildType Children;
	for (int i = 0; i < 8; i++)
	{
		Children.ChildrenName[i] = *FString::Printf(TEXT("%d_TN"), i);
	}
	BoundData->GeneratePosition = Bound->GetActorLocation();
	Bound->Subdivide(Children);
	auto Subsystem = World->GetSubsystem<UFAWorldSubsystem>();
	DataTables.Reserve(8);
	{
		//Event to wait for all data tables created and move to next step.
		FScopedEvent Event;
		AsyncTask(ENamedThreads::GameThread, [this, &Event]()
		{
			for (int i = 0; i < 8; i++)
			{
				DataTableLocks.Add(new UE::FSpinLock());
				DataTables.Add(
					CreateNodeDataTable(
						Path, FString::Printf(TEXT("DT_%s_%d"), *Bound->GetName(), i)));
			}
			Event.Trigger();
		});
	}
	{
		TArray<TFuture<uint8>> Results;
		Results.Reserve(8);
		TArray<UE::FSpinLock*> Locks;
		Locks.Reserve(8);
		for (int i = 0; i < 8; i++)
		{
			Locks.Add(new UE::FSpinLock());
			Results.Add(AsyncPool(*Subsystem->GetThreadPool(),
			                      [Subsystem, this, &Children, i, &Locks]
			                      {
				                      return Subsystem->GenerateNodeBranch(
					                      BoundData, World, Children.Children[i],
					                      Children.ChildrenName[i], 0, Locks[i], DataTables[i]);
			                      }));
		}
		for (auto& Task : Results)
		{
			FPlatformProcess::ConditionalSleep([&Task] { return Task.IsReady(); });
		}
		// Adding the top level nodes to data tables.
		for (int i = 0; i < 8; i++)
		{
			if (Results[i].Get() == 0 || Results[i].Get() == 2)
			{
				Children.Children[i]->IsTraversable = Results[i].Get() == 0;
				DataTables[i]->AddRow(Children.ChildrenName[i], *Children.Children[i]);
			}
			delete Locks[i];
		}
	}
	{
		FScopedEvent Event;
		TArray<UPackage*> Packages;
		for (auto dt : DataTables)
		{
			Packages.Add(dt->GetPackage());
		}
		Packages.Add(BoundData->GetPackage());
		AsyncTask(ENamedThreads::GameThread, [this, Packages, &Event]
		{
			UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
			UCompositeDataTableFactory* Factory = NewObject<UCompositeDataTableFactory>();
			Factory->Struct = FFaNodeData::StaticStruct();
			FSoftObjectPath MyAssetPath(Path + "DT_" + Bound->GetName() + "Combined");
			UObject* MyAsset = MyAssetPath.TryLoad();
			UObject* Object = MyAsset
				                  ? MyAsset
				                  : FModuleManager::GetModuleChecked<
					                  FAssetToolsModule>("AssetTools").Get().CreateAsset(
					                  "DT_" + Bound->GetName() + "Combined", Path,
					                  UCompositeDataTable::StaticClass(), Factory);

			BoundData->CombinedNodes = Cast<UCompositeDataTable>(Object);
			BoundData->CombinedNodes->EmptyTable();
			BoundData->CombinedNodes->AppendParentTables(DataTables);
			Event.Trigger();
		});
	}
	//Set index to nodes for HPA* search.
	{
		TArray<TFuture<void>> Results;
		Results.Reserve(8);
		UE::FSpinLock Lock;
		for (int i = 0; i < 8; i++)
		{
			Results.Add(AsyncThread([i, this, Subsystem, &Lock]
			{
				Subsystem->SetHPAIndex(BoundData->CombinedNodes.Get(), DataTables[i], HPAIndex,
				                       Lock);
			}));
		}
		for (int i = 0; i < 8; i++)
		{
			FPlatformProcess::ConditionalSleep([&Results, i] { return Results[i].IsReady(); });
		}
	}
	{
		FScopedEvent Event;
		TArray<UPackage*> Packages;
		for (auto dt : DataTables)
		{
			Packages.Add(dt->GetPackage());
		}
		Packages.Add(BoundData->GetPackage());
		Packages.Add(BoundData->CombinedNodes.Get()->GetPackage());

		Bound->SetBoundData(BoundData);
		BoundData->GeneratePosition = Bound->GetActorLocation();
		for (int i = 0; i < HPAIndex.Num(); i++)
		{
			Bound->GetLocalToGlobalHPANodes().Add(i, i);
			BoundData->ContainingHPANodes.AddUnique(i);
		}
		//Saving assets has to be done in Game Thread.
		AsyncTask(ENamedThreads::GameThread, [this, &Event, Packages]
		{
			UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
			bool out;
			FText Error;
			UEditorLoadingAndSavingUtils::ReloadPackages({BoundData->CombinedNodes->GetPackage()},
			                                             out, Error);
			Bound->LoadNodes();
			Event.Trigger();
		});
	}
	{
		TArray<FFaNodeData*> RowNames;
		BoundData->CombinedNodes.LoadSynchronous()->GetAllRows<FFaNodeData>("", RowNames);
		RowNames.Sort([](FFaNodeData& a, FFaNodeData& b)
		{
			return a.HPANodeIndex < b.HPANodeIndex;
		});
		UE::FSpinLock Lock;
		/**
		 * Don't use Async Pool due to it uses the thread for pathfinding and the function uses the pathfinding function and the thread will be stuck.
		 * A solution is to use a separate thread pool for pathfinding, but there is no time to implement and test it.
		 * However, this will sometime freeze the editor while
		 * it is short enough for developers not to notice when the depth is under 7.
		 */
		TArray<UE::Tasks::FTask> Tasks;
		Tasks.Reserve(HPAIndex.Num() * HPAIndex.Num());
		for (uint32 i1 = 0; i1 < (uint32)HPAIndex.Num() - 1; i1++)
		{
			for (uint32 i2 = i1 + 1; i2 < (uint32)HPAIndex.Num(); i2++)
			{
				if (i1 == i2) continue;
				Tasks.Add(UE::Tasks::Launch(
					UE_SOURCE_LOCATION, [this, Subsystem, &RowNames, i2, i1, &Lock]
					{
						FPlatformProcess::ConditionalSleep([] { return !IsInGameThread(); });
						FFaNodeData* d1 = *RowNames.FindByPredicate([i1](FFaNodeData* a)
						{
							return a->HPANodeIndex == i1;
						});
						FFaNodeData* d2 = *RowNames.FindByPredicate([i2](FFaNodeData* a)
						{
							return a->HPANodeIndex == i2;
						});
						{
							FFAHPAPath HPAPath;
							HPAPath.StartLocation = d1->Position;
							HPAPath.EndLocation = d2->Position;
							HPAPath.HPANodes.Add(i1);
							HPAPath.HPANodes.Add(i2);
							HPAPath.HPAAssociateBounds.Add(Bound);
							HPAPath.HPAAssociateBounds.Add(Bound);
							HPAPath.StartNode = Subsystem->PointToNodeInBound(d1->Position, Bound);
							HPAPath.EndNode = Subsystem->PointToNodeInBound(d2->Position, Bound);
							// Try to find a path between the two Big nodes to see if they are connected.
							auto r = Subsystem->CreateFinePathByHPA(HPAPath);
							if (r.Nodes.Num() > 0)
							{
								UE::TScopeLock SLock(Lock);
								BoundData->InternalHPAConnection.FindOrAdd(i1).Values.AddUnique(i2);
								BoundData->InternalHPAConnection.FindOrAdd(i2).Values.AddUnique(i1);
							}
						}
					}));
			}
		}
		UE::Tasks::BusyWait(Tasks);
	}
	{
		FScopedEvent Event;

		TArray<UPackage*> Packages;
		for (auto dt : DataTables)
		{
			Packages.Add(dt->GetPackage());
		}
		Packages.Add(BoundData->GetPackage());
		Packages.Add(BoundData->CombinedNodes->GetPackage());
		AsyncTask(ENamedThreads::GameThread, [this, &Event, Packages]
		{
			UEditorLoadingAndSavingUtils::SavePackages(Packages, false);
			Bound->GetLocalToGlobalHPANodes().Empty();
			auto time = FDateTime::UtcNow() - startGenTime;
			UE_LOG(LogFAWorldSubsystem, Display, TEXT("Generation Finished:%dh %dMins %d"),
			       time.GetHours(), time.GetMinutes(), time.GetSeconds());

			Event.Trigger();
		});
	}
	return 0;
}

void FFANodeGenRunnable::Exit()
{
	OnNodeGenFinished.Broadcast();
	delete this;
}

void FFANodeGenRunnable::Stop()
{
}

UDataTable* FFANodeGenRunnable::CreateNodeDataTable(FString InPath, FString Name)
{
	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = FFaNodeData::StaticStruct();
	FSoftObjectPath MyAssetPath(InPath + Name);
	UObject* MyAsset = MyAssetPath.TryLoad();
	UObject* Object = MyAsset
		                  ? MyAsset
		                  : FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().
		                  CreateAsset(Name, InPath, UDataTable::StaticClass(), Factory);
	auto result = Cast<UDataTable>(Object);
	result->EmptyTable();
	return result;
}

void UFANodeGenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	//Load Settings.
	UFAPathfindingSettings* Settings = GetMutableDefault<UFAPathfindingSettings>();
	ObjectTypes = Settings->ObjectTypes;
	EnvironmentActorClass = Settings->EnvironmentActorClass;
}

void UFANodeGenSubsystem::GenerateBoundNodes(FString Path, UWorld* World, uint32 MaxDepth,
                                             AFABound* Bound, UFABoundData* BoundData)
{
	if (bIsGeneratingNode || NodeGenRunnable)
	{
		UE_LOG(LogFAWorldSubsystem, Error, TEXT("Already Generating nodes"));
		return;
	}
	bIsGeneratingNode = true;
	UE_LOG(LogFAWorldSubsystem, Display, TEXT("Generating nodes"));

	if (!IsValid(World))
	{
		UE_LOG(LogFAWorldSubsystem, Error, TEXT("Level is not valid."));
		bIsGeneratingNode = false;
		return;
	}
	if (!IsValid(Bound))
	{
		UE_LOG(LogFAWorldSubsystem, Error, TEXT("Bound is not valid."));
		bIsGeneratingNode = false;
		return;
	}
	if (!IsValid(BoundData))
	{
		//Create Bound Data
		UDataAssetFactory* Factory = NewObject<UDataAssetFactory>();
		Factory->DataAssetClass = UFABoundData::StaticClass();
		UObject* Object = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get().
			CreateAssetWithDialog(*FString::Printf(TEXT("DA_FABD_%s"), *Bound->GetName()), Path,
			                      UFABoundData::StaticClass(), Factory, NAME_None, false);
		BoundData = Cast<UFABoundData>(Object);
		if (!IsValid(BoundData))
		{
			UE_LOG(LogFAWorldSubsystem, Error, TEXT("Bound is not valid."));
			bIsGeneratingNode = false;
			return;
		}
		BoundData->MaxDepth = MaxDepth;
	}

	NodeGenRunnable = new FFANodeGenRunnable(Path, World, Bound, BoundData,
	                                         FFAOnNodeGenFinished::FDelegate::CreateWeakLambda(
		                                         this, [this]
		                                         {
			                                         bIsGeneratingNode = false;
			                                         NodeGenRunnable = nullptr;
		                                         }));
}

void UFANodeGenSubsystem::BeginDestroy()
{
	Super::BeginDestroy();
	if (NodeGenRunnable)
	{
		NodeGenRunnable->Stop();
		delete NodeGenRunnable;
	}
}

bool UFANodeGenSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Editor;
}
