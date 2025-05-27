// Fill out your copyright notice in the Description page of Project Settings.

#include "FAWorldSubsystem.h"

#include "FABound.h"
#include "FANode.h"
#include "EngineUtils.h"
#include "FALevelData.h"
#include "FANeighbourData.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "FAPathfindingSettings.h"
#include "Engine/Engine.h"
#include "Engine/AssetManager.h"
#include "Engine/CompositeDataTable.h"
#include "Logging/LogVerbosity.h"
#include "VisualLogger/VisualLogger.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Tasks/Pipe.h"
#include "Tasks/Task.h"

DEFINE_LOG_CATEGORY(LogFAWorldSubsystem)

bool UFAWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) return false;

	UWorld* World = Cast<UWorld>(Outer);
	if (World->WorldType == EWorldType::Editor) return true;

	UFAPathfindingSettings* LocalSettings = GetMutableDefault<UFAPathfindingSettings>();
	TArray<TSoftObjectPtr<UWorld>> temp;
	LocalSettings->MapsSettings.GetKeys(temp);
	auto WorldName = World->GetPathName().Replace(*World->GetMapName(),
	                                              *UWorld::RemovePIEPrefix(World->GetMapName()),
	                                              ESearchCase::Type::CaseSensitive);
	auto ptr = temp.FindByPredicate([World,WorldName](TSoftObjectPtr<UWorld>& a)
	{
		return a->GetPathName() == WorldName;
	});
	return ptr != nullptr;
}

void UFAWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UE_LOG(LogTemp, Display, TEXT("UFANewWorldSubsystem::OnWorldBeginPlay"));
	Settings = GetMutableDefault<UFAPathfindingSettings>();
	TArray<TSoftObjectPtr<UWorld>> temp;
	Settings->MapsSettings.GetKeys(temp);
	UWorld* World = GEngine->GetCurrentPlayWorld();

	auto ptr = temp.FindByPredicate([World](TSoftObjectPtr<UWorld>& a)
	{
		return a->GetPathName() == World->GetPathName();
	});
	if (ptr)
	{
		RegisterBoundInWorldStartUp();
	}
	else
	{
		GameSystemReady = true;
	}
}

void UFAWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Settings = GetMutableDefault<UFAPathfindingSettings>();
	PathfindingAlgoClass = Settings->PathfindingAlgorithmToUse;
	PathfindingAlgo = NewObject<UFAPathfindingAlgo>(Settings->PathfindingAlgorithmToUse);
	ThreadPool = FQueuedThreadPool::Allocate();
	verify(
		ThreadPool->Create(16, (128 * 1024),TPri_AboveNormal, TEXT("FAWorldSubsystemThreadPool") ));
}

void UFAWorldSubsystem::BeginDestroy()
{
	UE::Tasks::Wait(SetHPATasks);
	OnSystemReady.Clear();
	FPlatformProcess::ConditionalSleep([this]
	{
		return !UFAPathfindingAlgo::IsGenerating();
	});
	if (ThreadPool) delete ThreadPool;
	for (auto Data : NeighboursData)
	{
		if (UGameplayStatics::DoesSaveGameExist(Data.Key, 0))
			UGameplayStatics::DeleteGameInSlot(Data.Key, 0);
	}
	Super::BeginDestroy();
}

void UFAWorldSubsystem::RegisterBoundInWorld(AFABound* Bound)
{
	Bound->LoadBoundData();
	if (!Bound->GetBoundData()) return;
	csHPAIndex.Lock();
	RegisteredBound.Add(Bound);
	for (auto hpaIndex : Bound->GetBoundData()->ContainingHPANodes)
	{
		Bound->GetLocalToGlobalHPANodes().Add(hpaIndex, HPAIndex.Num());
		HPAConnection.Add(HPAIndex.Num());
		HPAIndex.Add(Bound);
	}
	for (auto hpaIndex : Bound->GetBoundData()->ContainingHPANodes)
	{
		HPAConnection[Bound->GetLocalToGlobalHPANodes()[hpaIndex]].Values.Reserve(
			Bound->GetBoundData()->InternalHPAConnection[hpaIndex].Values.Num());
		for (auto i : Bound->GetBoundData()->InternalHPAConnection[hpaIndex].Values)
		{
			HPAConnection[Bound->GetLocalToGlobalHPANodes()[hpaIndex]].Values.Add(
				Bound->GetLocalToGlobalHPANodes()[i]);
		}
	}
	csHPAIndex.Unlock();

	for (int i = 0; i < RegisteredBound.Num() - 1; i++)
	{
		SetHPATasks.Add(UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, i, Bound]
		{
			SetBoundNeighbour(RegisteredBound[i], Bound);
		}));
	}
	SetHPATasks.Add(UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, Bound]
	{
		AsyncTask(ENamedThreads::GameThread, [this, Bound]
		{
			Bound->OnLODChanged();
		});
	}, SetHPATasks));
}

//Return 0 == Nothing within box
//Return 1 == Partially filled.
//Return 2 == Fully filled.
//Return 3 == Reached Max Depth.
uint8 UFAWorldSubsystem::GenerateNodeBranch(const UFABoundData* BoundData, UWorld* World,
                                            TSharedPtr<FFaNodeData> Node, FName NodeName,
                                            int nodeIndex, UE::FSpinLock* DataLock,
                                            UDataTable* Data)
{
	if (!IsNodeOverlapping(Node.Get(), Settings->ObjectTypes, Settings->EnvironmentActorClass,
	                       ActorsToIgnore, World)) return 0;
	if (Node->Depth == BoundData->MaxDepth) return 3;
	FFANewNodeChildType NodeChildren;
	{
		int cnodeIndex = 8 * nodeIndex + 8;
		for (int i = 0; i < 8; i++)
		{
			FString name = FString::Printf(TEXT("%s_%d_FAN"), *Data->GetName(), cnodeIndex);
			NodeChildren.ChildrenName[i] = FName(*name);
			cnodeIndex++;
		}
	}
	Subdivide(Node, NodeChildren);
	TArray<uint8> Tasks;
	Tasks.Reserve(8);
	int cnodeIndex = 8 * nodeIndex + 8;
	for (int i = 0; i < 8; i++)
	{
		Tasks.Add(GenerateNodeBranch(BoundData, World, NodeChildren.Children[i],
		                             NodeChildren.ChildrenName[i], cnodeIndex, DataLock, Data));
		cnodeIndex++;
	}
	uint8 Results = 0;

	for (auto& Task : Tasks)
	{
		Results += Task;
	}
	//All children are filled.
	if (Results == 16) return 2;
	//All children are filled and reach max depth.
	if (Results == 24) return 2;

	{
		UE::TScopeLock Lock(*DataLock);
		for (int i = 0; i < 8; i++)
		{
			if (Tasks[i] != 1)
			{
				NodeChildren.Children[i]->IsTraversable = Tasks[i] == 0;
				Data->AddRow(NodeChildren.ChildrenName[i], *NodeChildren.Children[i]);
			}
		}
	}

	return 1;
}

void UFAWorldSubsystem::SetHPAIndex(UDataTable* CombinedNodes, UDataTable* DataTable,
                                    TArray<AFABound*>& InHPAIndex, UE::FSpinLock& _csHPAIndex)
{
	TArray<TPair<FName, uint8*>> RowMap = CombinedNodes->GetRowMap().Array();
	RowMap.Sort([](TPair<FName, uint8*> a, TPair<FName, uint8*> b)
	{
		return reinterpret_cast<FFaNodeData*>(a.Value)->Depth < reinterpret_cast<FFaNodeData*>(b.
			Value)->Depth;
	});
	UE::FSpinLock Lock;
	//Change to use ue task.
	TArray<TFuture<void>> Tasks;
	Tasks.Reserve(2000);
	for (int x = 0; x < RowMap.Num() - 1; x++)
	{
		auto r0 = reinterpret_cast<FFaNodeData*>(RowMap[x].Value);
		Tasks.Add(AsyncPool(*ThreadPool, [&RowMap, x, r0, &Lock, DataTable]
		{
			for (int y = x + 1; y < RowMap.Num(); y++)
			{
				auto r1 = reinterpret_cast<FFaNodeData*>(RowMap[y].Value);
				if (AABBOverlap(r0->Position, r1->Position, r0->HalfExtent, r1->HalfExtent))
				{
					UE::TScopeLock SL(Lock);
					if (auto d = DataTable->FindRow<FFaNodeData>(RowMap[x].Key, "", false))
						d->Neighbour.AddUnique(RowMap[y].Key);
					if (auto d = DataTable->FindRow<FFaNodeData>(RowMap[y].Key, "", false))
						d->Neighbour.AddUnique(RowMap[x].Key);
				}
			}
		}));
		if (Tasks.Num() == 2000)
		{
			for (auto& Task : Tasks)
			{
				FPlatformProcess::ConditionalSleep([&Task] { return Task.IsReady(); });
			}
			Tasks.Empty(2000);
		}
	}
	for (auto& Task : Tasks)
	{
		FPlatformProcess::ConditionalSleep([&Task] { return Task.IsReady(); });
	}
	Tasks.Empty();
	for (auto pair : DataTable->GetRowMap())
	{
		TArray<FName> OpenSet, ClosedSet;
		OpenSet.Reserve(DataTable->GetRowMap().Num());
		ClosedSet.Reserve(DataTable->GetRowMap().Num());

		auto nodeValue = reinterpret_cast<FFaNodeData*>(pair.Value);
		if (!nodeValue->IsTraversable) continue;
		FName Current = pair.Key;
		OpenSet.Add(Current);
		int32 Idx;
		{
			UE::TScopeLock HPALock(_csHPAIndex);
			Idx = nodeValue->HPANodeIndex == INDEX_NONE
				      ? InHPAIndex.Num()
				      : nodeValue->HPANodeIndex;
			if (nodeValue->HPANodeIndex == INDEX_NONE)
			{
				InHPAIndex.Add(nullptr);
			}
		}

		while (OpenSet.Num() > 0)
		{
			Current = OpenSet.Pop();
			ClosedSet.Add(Current);
			auto n = DataTable->FindRow<FFaNodeData>(Current, "", false);
			if (!n) continue;
			if (!n->IsTraversable || n->HPANodeIndex != INDEX_NONE) continue;
			n->HPANodeIndex = Idx;
			for (auto neighbour : n->Neighbour)
			{
				if (DataTable->FindRowUnchecked(neighbour) && !ClosedSet.Contains(neighbour))
				{
					OpenSet.AddUnique(neighbour);
				}
			}
		}
	}
}

void UFAWorldSubsystem::SetBoundNeighbour(AFABound* Bound0, AFABound* Bound1)
{
	if (!AABBOverlap(Bound0->GetActorLocation(), Bound1->GetActorLocation(),
	                 Bound0->GetHalfExtent(), Bound1->GetHalfExtent())) return;

	AsyncTask(ENamedThreads::GameThread, [this, Bound0, Bound1]
	{
		Bound0->LoadNodes();
		Bound1->LoadNodes();
		Bound0->GetNodeData()->WaitUntilComplete();
		Bound1->GetNodeData()->WaitUntilComplete();
	});

	if (!Bound0->GetNodesData() || !Bound1->GetNodesData()) return;
	TArray<TPair<FName, uint8*>> Rows1 = Bound0->GetNodesData()->GetRowMap().Array();
	TArray<TPair<FName, uint8*>> Rows2 = Bound1->GetNodesData()->GetRowMap().Array();

	Rows1.RemoveAll([](TPair<FName, uint8*>& d)
	{
		auto dd = reinterpret_cast<FFaNodeData*>(d.Value);
		return !dd->IsTraversable || dd->HPANodeIndex == INDEX_NONE;
	});
	Rows2.RemoveAll([](TPair<FName, uint8*>& d)
	{
		auto dd = reinterpret_cast<FFaNodeData*>(d.Value);
		return !dd->IsTraversable || dd->HPANodeIndex == INDEX_NONE;
	});
	UFANeighbourData* NeighbourData = Cast<UFANeighbourData>(
		UGameplayStatics::CreateSaveGameObject(UFANeighbourData::StaticClass()));
	NeighbourData->Bound[0] = Bound0;
	NeighbourData->Bound[1] = Bound1;

	TMap<uint32, FFAConnectedHPANode> LocalHPAConnection;

	UE::FSpinLock LocalHPAConnectionLock, LocalNeighbourDataLock;

	TArray<TFuture<void>> Tasks;
	Tasks.Reserve(2000);
	for (auto& Row1 : Rows1)
	{
		auto d1 = reinterpret_cast<FFaNodeData*>(Row1.Value);
		Tasks.Add(AsyncPool(*ThreadPool,
		                    [&Rows2, d1, Bound0, Bound1, &LocalNeighbourDataLock, &NeighbourData,
			                    Row1, &LocalHPAConnectionLock, &LocalHPAConnection]
		                    {
			                    for (auto& Row2 : Rows2)
			                    {
				                    auto d2 = reinterpret_cast<FFaNodeData*>(Row2.Value);

				                    if (!AABBOverlap(
					                    d1->Position + Bound0->GetActorLocation() - Bound0->
					                    GetBoundData()->GeneratePosition,
					                    d2->Position + Bound1->GetActorLocation() - Bound1->
					                    GetBoundData()->GeneratePosition, d1->HalfExtent,
					                    d2->HalfExtent)) continue;
				                    {
					                    UE::TScopeLock Lock(LocalNeighbourDataLock);
					                    NeighbourData->Connection0.FindOrAdd(Row1.Key).Connected.
					                                   AddUnique(Row2.Key);
					                    NeighbourData->Connection1.FindOrAdd(Row2.Key).Connected.
					                                   AddUnique(Row1.Key);
				                    }
				                    {
					                    UE::TScopeLock Lock(LocalHPAConnectionLock);
					                    LocalHPAConnection.FindOrAdd(
						                                       Bound0->GetLocalToGlobalHPANodes()[d1
							                                       ->HPANodeIndex]).
					                                       Values.AddUnique(
						                                       Bound1->GetLocalToGlobalHPANodes()[d2
							                                       ->HPANodeIndex]);
					                    LocalHPAConnection.FindOrAdd(
						                                       Bound1->GetLocalToGlobalHPANodes()[d2
							                                       ->HPANodeIndex]).
					                                       Values.AddUnique(
						                                       Bound0->GetLocalToGlobalHPANodes()[d1
							                                       ->HPANodeIndex]);
				                    }
			                    }
		                    }));
		if (Tasks.Num() == 2000)
		{
			for (auto& Task : Tasks)
			{
				FPlatformProcess::ConditionalSleep([&Task] { return Task.IsReady(); });
			}
			Tasks.Empty(2000);
		}
	}
	for (auto& Task : Tasks)
	{
		FPlatformProcess::ConditionalSleep([&Task] { return Task.IsReady(); });
	}
	Tasks.Empty();
	for (auto& connection : LocalHPAConnection)
	{
		FScopeLock Lock(&HPAConnectionLock);
		HPAConnection[connection.Key].Values.Append(connection.Value.Values);
	}
	UGameplayStatics::AsyncSaveGameToSlot(NeighbourData,
	                                      FString::Printf(TEXT("%p%p"), Bound0, Bound1), 0);
	Bound0->AddNeighbourData(FString::Printf(TEXT("%p%p"), Bound0, Bound1), NeighbourData);
	Bound1->AddNeighbourData(FString::Printf(TEXT("%p%p"), Bound0, Bound1), NeighbourData);
	NeighboursDataLock.Lock();
	NeighboursData.Add(FString::Printf(TEXT("%p%p"), Bound0, Bound1), NeighbourData);
	NeighboursDataLock.Unlock();
}

void UFAWorldSubsystem::RegisterBoundInWorldStartUp()
{
	TActorIterator<AFABound> BoundIt(GetWorld());
	for (auto& Bound = BoundIt; Bound; ++Bound)
	{
		Bound->LoadBoundData();
		if (!Bound->GetBoundData()) continue;
		csHPAIndex.Lock();
		RegisteredBound.Add(*Bound);
		for (auto hpaIndex : Bound->GetBoundData()->ContainingHPANodes)
		{
			Bound->GetLocalToGlobalHPANodes().Add(hpaIndex, HPAIndex.Num());
			HPAConnection.Add(HPAIndex.Num());
			HPAIndex.Add(*Bound);
		}
		for (auto hpaIndex : Bound->GetBoundData()->ContainingHPANodes)
		{
			if (!Bound->GetBoundData()->InternalHPAConnection.Contains(hpaIndex)) continue;
			HPAConnection[Bound->GetLocalToGlobalHPANodes()[hpaIndex]].Values.Reserve(
				Bound->GetBoundData()->InternalHPAConnection[hpaIndex].Values.Num());
			for (auto i : Bound->GetBoundData()->InternalHPAConnection[hpaIndex].Values)
			{
				HPAConnection[Bound->GetLocalToGlobalHPANodes()[hpaIndex]].Values.Add(
					Bound->GetLocalToGlobalHPANodes()[i]);
			}
		}
		//If System is not loaded and destroy, it will crash.
		OnSystemReady.AddLambda([Bound]
		{
			Bound->OnLODChanged();
		});
		csHPAIndex.Unlock();
	}
	for (int i = 0; i < RegisteredBound.Num() - 1; i++)
	{
		for (int j = i + 1; j < RegisteredBound.Num(); j++)
		{
			SetHPATasks.Add(UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, i, j]
			{
				SetBoundNeighbour(RegisteredBound[i], RegisteredBound[j]);
			}));
		}
	}
	SetHPATasks.Add(UE::Tasks::Launch(UE_SOURCE_LOCATION, [this]
	{
		AsyncTask(ENamedThreads::GameThread, [this]
		{
			UE::TScopeLock Lock(OnSystemReadyLock);
			GameSystemReady = true;
			UE_VLOG(this, LogFAWorldSubsystem, Display, TEXT("GameSystemReady"));
			OnSystemReady.Broadcast();
		});
	}, SetHPATasks));
}

void UFAWorldSubsystem::Subdivide(TSharedPtr<FFaNodeData> Node, FFANewNodeChildType& Children)
{
	const uint32 NewDepth = Node->Depth + 1;
	const FVector NewHalfExtent = Node->HalfExtent / 2;
	for (uint8 i = 0; i < 8; ++i)
	{
		FVector NewPosition = Node->Position;
		NewPosition += NewHalfExtent * FVector(FA::XPositiveChildrenIndex.Contains(i) ? 1 : -1,
		                                       FA::YPositiveChildrenIndex.Contains(i) ? 1 : -1,
		                                       FA::ZPositiveChildrenIndex.Contains(i) ? 1 : -1);
		Children.Children[i] = Children.Children[i] != nullptr
			                       ? Children.Children[i]
			                       : MakeShared<FFaNodeData>();
		Children.Children[i]->Position = NewPosition;
		Children.Children[i]->Depth = NewDepth;
		Children.Children[i]->HalfExtent = NewHalfExtent;
	}
}

FFAHPAPath UFAWorldSubsystem::CreateHPAPath(const FVector& StartLocation,
                                            const FVector& EndLocation)
{
	return InternalCreateHPAPath(StartLocation, EndLocation, RegisteredBound);
}

UFANeighbourData* UFAWorldSubsystem::GetNeighbourData(FString Key)
{
	UE::TScopeLock Lock(NeighboursDataLock);
	if (NeighboursData.Contains(Key))
	{
		if (NeighboursData[Key].IsValid())
		{
			return NeighboursData[Key].Get();
		}
		auto result = Cast<UFANeighbourData>(UGameplayStatics::LoadGameFromSlot(Key, 0));
		NeighboursData[Key] = result;
		return result;
	}
	//Should not return value if the key does not exist.
	return nullptr;
}

FFAHPAPath UFAWorldSubsystem::InternalCreateHPAPath(FVector StartLocation, FVector EndLocation,
                                                    TArray<AFABound*> Bounds)
{
	FFAPathNodeData StartNode, EndNode;
	for (auto Bound : Bounds)
	{
		if (StartNode.NodeName.IsNone()) StartNode = PointToNodeInBound(StartLocation, Bound);
		if (EndNode.NodeName.IsNone()) EndNode = PointToNodeInBound(EndLocation, Bound);
	}
	FFAHPAPath Result;
	if (StartNode.NodeName.IsNone() || EndNode.NodeName.IsNone())
	{
		UE_LOG(LogTemp, Display, TEXT("Point not in Bound"));
		return Result;
	}
	if (!StartNode.NodeData.IsTraversable || !EndNode.NodeData.IsTraversable)
	{
		UE_LOG(LogTemp, Display, TEXT("Point is not traversable"));
		return Result;
	}

	Result.StartLocation = StartLocation;
	Result.EndLocation = EndLocation;

	Result.StartNode = StartNode;
	Result.EndNode = EndNode;

	auto StartHPANode = StartNode.NodeData.HPANodeIndex;
	auto EndHPANode = EndNode.NodeData.HPANodeIndex;
	if (StartHPANode == EndHPANode)
	{
		Result.HPANodes.Add(StartHPANode);
		Result.HPAAssociateBounds.Add(StartNode.NodeBound);
		Result.bIsSuccess = true;
		return Result;
	}

	TArray<uint32> Visited;
	TArray<TPair<uint32, uint32>> Paths;
	TArray<uint32> Queue;
	Queue.Add(StartHPANode);
	uint32 Current = -1;
	while (Queue.Num() > 0)
	{
		uint32 i = Queue[0];
		Queue.RemoveAt(0);
		Visited.AddUnique(i);
		for (auto x : HPAConnection[i].Values)
		{
			if (Visited.Contains(x)) continue;
			Paths.Add(TPair<uint32, uint32>(x, i));
			if (x == EndHPANode)
			{
				Current = x;
				break;
			}
			Queue.AddUnique(x);
			Visited.AddUnique(x);
		}
	}
	if (Current == -1) return Result;

	while (Current != StartHPANode)
	{
		Result.HPANodes.Add(Current);
		Result.HPAAssociateBounds.Add(HPAIndex[Current]);
		Current = Paths.FindByPredicate([Current](TPair<uint32, uint32>& a)
		{
			return Current == a.Key;
		})->Value;
	}
	Result.HPANodes.Add(Current);
	Result.HPAAssociateBounds.Add(HPAIndex[Current]);
	Algo::Reverse(Result.HPANodes);
	Algo::Reverse(Result.HPAAssociateBounds);
	Result.bIsSuccess = true;
	return Result;
}

bool UFAWorldSubsystem::AABBOverlap(FVector P1, FVector P2, FVector H1, FVector H2)
{
	const FVector P1Min = P1 - H1, P2Min = P2 - H2, P1Max = P1 + H1, P2Max = P2 + H2;
	return P1Min.X <= P2Max.X && P1Max.X >= P2Min.X && P1Min.Y <= P2Max.Y && P1Max.Y >= P2Min.Y &&
		P1Min.Z <= P2Max.Z && P1Max.Z >= P2Min.Z;
}

FFAFinePath UFAWorldSubsystem::CreateFinePathByHPA(FFAHPAPath HPAPath, const FVector ColliderSize,
                                                   const FVector& ColliderOffset)
{
	auto AResult = CreateFinePathByHPAAsync(HPAPath, ColliderSize, ColliderOffset);
	AResult.Wait();
	return AResult.Get();
}

TFuture<FFAFinePath> UFAWorldSubsystem::CreateFinePathByHPAAsync(FFAHPAPath& HPAPath,
                                                                 const FVector& ColliderSize,
                                                                 const FVector& ColliderOffset)
{
	auto AResult = AsyncPool(*ThreadPool, [this, HPAPath, ColliderSize, ColliderOffset]
	{
		FFAFinePath Result{};
		if (HPAPath.HPANodes.Num() == 0) return Result;
		Result.HPAPath = HPAPath;
		Result.CurrentHPANodeIndex = 0;
		Result.LocalStartNode = HPAPath.StartNode;
		Result.LocalStartLocation = HPAPath.StartLocation;

		PathfindingAlgo->GeneratePath(Result, HPAPath.EndNode, GetWorld(), Settings, ColliderSize,
		                              ColliderOffset);
		return Result;
	});
	return AResult;
}

FFAFinePath UFAWorldSubsystem::CreateNextFinePath(const FFAFinePath& InFinePath,
                                                  const FVector& ColliderSize,
                                                  const FVector& ColliderOffset)
{
	auto AResult = CreateNextFinePathAsync(InFinePath, ColliderSize, ColliderOffset);

	AResult.Wait();

	return AResult.Get();
}

TFuture<FFAFinePath> UFAWorldSubsystem::CreateNextFinePathAsync(
	const FFAFinePath& InFinePath, const FVector& ColliderSize, const FVector& ColliderOffset)
{
	return AsyncPool(*ThreadPool, [this, InFinePath, ColliderSize,ColliderOffset]
	{
		if (!InFinePath.bIsSuccess) return InFinePath;

		FFAFinePath Result;
		Result.HPAPath = InFinePath.HPAPath;
		Result.CurrentHPANodeIndex = InFinePath.CurrentHPANodeIndex + 1;
		Result.bBoundLoaded = true;
		if (Result.CurrentHPANodeIndex >= InFinePath.HPAPath.HPANodes.Num()) return Result;
		if (!Result.HPAPath.HPAAssociateBounds[Result.CurrentHPANodeIndex]->GetNodesData() || !
			Result.HPAPath.HPAAssociateBounds[InFinePath.CurrentHPANodeIndex]->GetNodesData())
		{
			Result.bBoundLoaded = false;
			return Result;
		}
		Result.bBoundLoaded = true;
		Result.LocalStartNode = InFinePath.Nodes.Last();
		Result.LocalStartLocation = InFinePath.InterpolatedPoints.Last();

		PathfindingAlgo->GeneratePath(Result, InFinePath.HPAPath.EndNode, GetWorld(), Settings,
		                              ColliderSize, ColliderOffset);
		Result.ControlPoints.Insert(InFinePath.InterpolatedPoints.Last(), 0);
		return Result;
	});
}

void UFAWorldSubsystem::InterpolateFinePath(FFAFinePath& InFinePath)
{
	if (!InFinePath.bIsSuccess) return;
	if (InFinePath.Nodes.Num() == 0) return;
	if (InFinePath.Nodes.Num() == 1 || (InFinePath.Nodes.Num() == 2 && InFinePath.
		CurrentHPANodeIndex + 1 == InFinePath.HPAPath.HPANodes.Num()))
	{
		InFinePath.InterpolatedPoints.Add(InFinePath.LocalStartLocation);
		InFinePath.InterpolatedPoints.Add(InFinePath.HPAPath.EndLocation);
		return;
	}

	for (int i = 1; i < InFinePath.ControlPoints.Num() - 2; i++)
	{
		//Interpolation. Catmull-Rom Spline.
		float t = 0;
		float dist = FVector::Distance(InFinePath.ControlPoints[i],
		                               InFinePath.ControlPoints[i + 1]);
		while (t <= 1)
		{
			//catmull-rom spline
			FVector P = InFinePath.ControlPoints[i] + ((InFinePath.ControlPoints[i + 1] - InFinePath
					.ControlPoints[i - 1]) * t + (2 * InFinePath.ControlPoints[i - 1] - 5 *
					InFinePath.
					ControlPoints[i] + 4 * InFinePath.ControlPoints[i + 1] - InFinePath.
					ControlPoints[i
						+ 2]) * t * t + (-InFinePath.ControlPoints[i - 1] + 3 * InFinePath.
					ControlPoints
					[i] - 3 * InFinePath.ControlPoints[i + 1] + InFinePath.ControlPoints[i + 2]) * t
				* t
				* t) / 2;
			InFinePath.InterpolatedPoints.Add(P);
			t += FMath::Clamp(70 / dist, 0, 0.9);
		}
	}
#if ENABLE_VISUAL_LOG
	for (auto i = 0; i < InFinePath.ControlPoints.Num(); i++)
	{
		UE_VLOG_LOCATION(this, LogFAWorldSubsystem, Display, InFinePath.ControlPoints[i], 1,
		                 FColor::Blue, TEXT("%d"), i);
	}
	for (auto i = 0; i < InFinePath.InterpolatedPoints.Num(); i++)
	{
		UE_VLOG_LOCATION(this, LogFAWorldSubsystem, Display, InFinePath.InterpolatedPoints[i], 1,
		                 FColor::Yellow, TEXT("%d"), i);
	}
	for (auto node : InFinePath.Nodes)
	{
		UE_VLOG_BOX(this, LogTemp, Display,
		            FBox(node.NodeData.Position-node.NodeData.HalfExtent,node. NodeData.Position+
			            node.NodeData. HalfExtent ), FColor::Green, TEXT("Path"));
	}
#endif
}

FFAPathNodeData UFAWorldSubsystem::PointToNodeInBound(FVector Point, AFABound* Bound)
{
	FVector BoundPosition = Bound->GetActorLocation();
	if (!UKismetMathLibrary::IsPointInBox(Point, BoundPosition, Bound->GetHalfExtent()))
		return FFAPathNodeData();
	//Bound is not loaded.
	if (!Bound->GetNodesData()) return FFAPathNodeData();;
	FFaNodeData* RData = nullptr;
	FFAPathNodeData Result;
	FVector Transformed = BoundPosition - Bound->GetBoundData()->GeneratePosition;
	Result.NodeBound = Bound;
	for (auto row : Bound->GetNodesData()->GetRowMap())
	{
		auto Node = reinterpret_cast<FFaNodeData*>(row.Value);
		if (UKismetMathLibrary::IsPointInBox(Point, Node->Position + Transformed, Node->HalfExtent))
		{
			if (RData)
			{
				if (FVector::DistSquared(Point, Node->Position) < FVector::DistSquared(
					Point, RData->Position))
				{
					Result.NodeName = row.Key;
					RData = Node;
				}
			}
			else
			{
				Result.NodeName = row.Key;
				RData = Node;
			}
		}
	}

	Result.NodeData = *RData;
	Result.NodeData.HPANodeIndex = Result.NodeData.HPANodeIndex == INDEX_NONE
		                               ? INDEX_NONE
		                               : Result.NodeBound->GetLocalToGlobalHPANodes()[Result.
			                               NodeData.HPANodeIndex];
	Result.NodeData.Position += Transformed;
	return Result;
}

bool UFAWorldSubsystem::IsNodeOverlapping(FFaNodeData* NodeData,
                                          const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
                                          TSubclassOf<AActor> ActorClassToConsider,
                                          const TArray<AActor*>& ActorsToIgnore, UWorld* World)
{
	TArray<AActor*> Actors;
	UKismetSystemLibrary::BoxOverlapActors(World, NodeData->Position, NodeData->HalfExtent,
	                                       ObjectTypes, ActorClassToConsider, ActorsToIgnore,
	                                       Actors);
	return Actors.Num() > 0;
}

// BEGIN_DEFINE_SPEC(FGenerateBranchTest,
//                   "FlyingAIPlugin.FACore.FANewWorldSubsystem.GenerateBranchTest",
//                   EAutomationTestFlags::ProductFilter | EAutomationTestFlags::
//                   ApplicationContextMask)
//
// 	TObjectPtr<UFANewWorldSubsystem> System;
//
// 	UFABoundData* BoundData;
//
// 	TArray<UDataTable*> DT;
//
// END_DEFINE_SPEC(FGenerateBranchTest)

// void FGenerateBranchTest::Define()
// {
// 	Describe("Pre-Gen", [this]()
// 	{
// 		BeforeEach([this]()
// 		{
// 			System = GEditor->GetEditorWorldContext().World()->GetSubsystem<UFANewWorldSubsystem>();
// 			DT.Empty();
// 		});
// 		It("Should not be nullptr", [this]()
// 		{
// 			TestEqual(TEXT(""), System == nullptr, false);
// 		});
// 		Describe("Generation", [this]()
// 		{
// 			System = GEditor->GetEditorWorldContext().World()->GetSubsystem<UFANewWorldSubsystem>();
// 			System->GenerateBoundNodes("/Game/AutomationTests/",
// 			                      GEditor->GetEditorWorldContext().World(), 5, BoundData);
// 			BeforeEach([this]()
// 			{
// 				DT.Empty();
// 				for (int i = 0; i < 8; i++)
// 				{
// 					auto dt = LoadObject<UDataTable>(
// 						nullptr, *FString::Printf(
// 							TEXT("/Game/AutomationTests/DT_%s%d"),
// 							*GEditor->GetEditorWorldContext().World()->GetMapName(), i));
// 					TestEqual("", dt == nullptr, false);
// 					DT.Add(dt);
// 				}
// 				LevelData = LoadObject<UFALevelData>(
// 					nullptr, TEXT("/Game/AutomationTests/TestMap_LevelData"));
// 			});
// 			It("Should generate 8 data tables", [this]()
// 			{
// 				TestEqual("", DT.Num(), 8);
// 			});
//
// 			It("Should have 8*n+1 rows", [this]
// 			{
// 				for (int i = 0; i < 8; i++)
// 				{
// 					TestEqual("", DT[i]->GetRowNames().Num() % 8 == 1, true);
// 				}
// 			});
// 			It("Should have 8 children or None", [this]()
// 			{
// 				for (auto Cdt : DT)
// 				{
// 					TArray<FFaNodeData*> Rows;
// 					Cdt->GetAllRows<FFaNodeData>("", Rows);
// 					for (auto child : Rows)
// 					{
// 						uint8 childCount = 0;
// 						for (auto c : child->Children)
// 						{
// 							childCount++;
// 						}
// 						TestTrue(
// 							TEXT("Have 8 children or None"), childCount == 8 || childCount == 0);
// 					}
// 				}
// 			});
// 			It("Should have at least 3 neighbour", [this]()
// 			{
// 				for (auto Cdt : DT)
// 				{
// 					TArray<FFaNodeData*> Rows;
// 					Cdt->GetAllRows<FFaNodeData>("", Rows);
// 					for (auto child : Rows)
// 					{
// 						uint8 neighbourCount = 0;
// 						for (auto c : child->Neighbour)
// 						{
// 							neighbourCount++;
// 						}
// 						TestTrue(TEXT("Have at least 3 neighbour"), neighbourCount >= 3);
// 					}
// 				}
// 			});
// 			It("Should Have Neighbour in the right place", [this]()
// 			{
// 				TArray<FFaNodeData*> Rows;
// 				auto Cdt = LevelData->CombinedNodes.LoadSynchronous();
// 				Cdt->GetAllRows<FFaNodeData>("", Rows);
// 				for (auto Row : Rows)
// 				{
// 					FFaNodeData* Neighbour = nullptr;
// 					if (!Row->Neighbour[0].IsNone())
// 					{
// 						Neighbour = Cdt->FindRow<FFaNodeData>(Row->Neighbour[0], "");
//
// 						TestEqual(
// 							TEXT("X Positive Position"), Neighbour->Position.X,
// 							Row->Position.X + Row->HalfExtent.X + Neighbour->HalfExtent.X);
// 					}
// 					if (!Row->Neighbour[1].IsNone())
// 					{
// 						Neighbour = Cdt->FindRow<FFaNodeData>(Row->Neighbour[1], "");
// 						TestEqual(
// 							TEXT("X Negative Position"), Neighbour->Position.X,
// 							Row->Position.X - Row->HalfExtent.X - Neighbour->HalfExtent.X);
// 					}
// 					if (!Row->Neighbour[2].IsNone())
// 					{
// 						Neighbour = Cdt->FindRow<FFaNodeData>(Row->Neighbour[2], "");
// 						TestEqual(
// 							TEXT("Y Positive Position"), Neighbour->Position.Y,
// 							Row->Position.Y + Row->HalfExtent.Y + Neighbour->HalfExtent.Y);
// 					}
// 					if (!Row->Neighbour[3].IsNone())
// 					{
// 						Neighbour = Cdt->FindRow<FFaNodeData>(Row->Neighbour[3], "");
// 						TestEqual(
// 							TEXT("Y Negative Position"), Neighbour->Position.Y,
// 							Row->Position.Y - Row->HalfExtent.Y - Neighbour->HalfExtent.Y);
// 					}
// 					if (!Row->Neighbour[4].IsNone())
// 					{
// 						Neighbour = Cdt->FindRow<FFaNodeData>(Row->Neighbour[4], "");
// 						TestEqual(
// 							TEXT("Z Positive Position"), Neighbour->Position.Z,
// 							Row->Position.Z + Row->HalfExtent.Z + Neighbour->HalfExtent.Z);
// 					}
// 					if (!Row->Neighbour[5].IsNone())
// 					{
// 						Neighbour = Cdt->FindRow<FFaNodeData>(Row->Neighbour[5], "");
// 						TestEqual(
// 							TEXT("Z Negative Position"), Neighbour->Position.Z,
// 							Row->Position.Z - Row->HalfExtent.Z - Neighbour->HalfExtent.Z);
// 					}
// 				}
// 			});
// 			It("Should have children in the right place", [this]()
// 			{
// 				for (auto Cdt : DT)
// 				{
// 					TArray<FFaNodeData*> Rows;
// 					Cdt->GetAllRows<FFaNodeData>("", Rows);
// 					for (auto Row : Rows)
// 					{
// 						if (Row->Children[0].IsNone()) continue;
// 						for (uint8 i = 0; i < 8; i++)
// 						{
// 							auto Child = Cdt->FindRow<FFaNodeData>(Row->Children[i], "");
//
// 							TestEqual(
// 								TEXT("Half Extent should be half of parent's"), Child->HalfExtent,
// 								Row->HalfExtent / 2);
// 							TestEqual(
// 								TEXT("X Position should be (-)Half Extent"), Child->Position.X,
// 								Row->Position.X + (i % 2 == 1 ? 1 : -1) * Child->HalfExtent.X);
// 							TestEqual(
// 								TEXT("Y Position should be (-)Half Extent"), Child->Position.Y,
// 								Row->Position.Y + ((i >> 1) % 2 == 1 ? 1 : -1) * Child->HalfExtent.
// 								Y);
// 							TestEqual(
// 								TEXT("Z Position should be (-)Half Extent"), Child->Position.Z,
// 								Row->Position.Z + ((i >> 2) % 2 == 1 ? 1 : -1) * Child->HalfExtent.
// 								Z);
// 						}
// 					}
// 				}
// 			});
// 			It("Should linked with same HPA index", [this]()
// 			{
// 				for (auto Cdt : DT)
// 				{
// 					TArray<FFaNodeData*> Rows;
// 					Cdt->GetAllRows<FFaNodeData>("", Rows);
// 					for (auto Row : Rows)
// 					{
// 						if (!Row->IsTraversable) continue;
// 						uint8 childCount = 0;
// 						uint8 notChildCount = 0;
// 						for (auto c : Row->Neighbour)
// 						{
// 							if (auto cd = Cdt->FindRow<FFaNodeData>(c, "", false))
// 							{
// 								if (cd->HPANodeIndex == Row->HPANodeIndex && cd->
// 									IsTraversable) childCount++;
// 								else if (cd->HPANodeIndex != INDEX_NONE) notChildCount++;
// 							}
// 						}
// 						TestEqual(
// 							TEXT("Should connect with same HPA Node"),
// 							childCount > 0 || notChildCount == 0, true);
// 					}
// 				}
// 			});
// 		});
// 		AfterEach([this]()
// 		{
// 			DT.Empty();
// 		});
// 	});
// }

// IMPLEMENT_SIMPLE_AUTOMATION_TEST(NewSubdivisionTest,
//                                  "FlyingAIPlugin.FACore.FANewWorldSubsystem.NewSubdivisionTest",
//                                  EAutomationTestFlags::EditorContext | EAutomationTestFlags::
//                                  EngineFilter);
//
// bool NewSubdivisionTest::RunTest(const FString& Parameters)
// {
// 	TSharedPtr<FFaNodeData> NodeData = MakeShared<FFaNodeData>();
// 	NodeData->Position = FVector(0, 0, 0);
// 	NodeData->HalfExtent = FVector(500);
// 	NodeData->Depth = 0;
// 	FFANewNodeChildType Children;
// 	int i = 0;
// 	for (auto& child : Children.ChildrenName)
// 	{
// 		child = FName(FString::Printf(TEXT("%d"), i));
// 		i++;
// 	}
// 	UFANewWorldSubsystem::Subdivide(NodeData, FName("TestNodeData"), Children);
// 	i = 0;
// 	for (auto Child : Children.Children)
// 	{
// 		TestEqual(TEXT("Parent Name"), Child->Parent, FName("TestNodeData"));
//
// 		FFaNodeData* Neighbour = nullptr;
// 		if (!Child->Neighbour[0].IsNone())
// 		{
// 			Neighbour = Children.Children[int(
// 				std::find(Children.ChildrenName, &Children.ChildrenName[8], Child->Neighbour[0]) -
// 				Children.ChildrenName)].Get();
//
// 			TestEqual(TEXT("X Positive Position"), Neighbour->Position.X,
// 			          Child->Position.X + Child->HalfExtent.X + Neighbour->HalfExtent.X);
// 			TestEqual(TEXT("Neighbour"), Neighbour->Neighbour[1], Children.ChildrenName[i]);
// 		}
// 		if (!Child->Neighbour[1].IsNone())
// 		{
// 			Neighbour = Children.Children[int(
// 				std::find(Children.ChildrenName, &Children.ChildrenName[8], Child->Neighbour[1]) -
// 				Children.ChildrenName)].Get();
//
// 			TestEqual(TEXT("X Negative Position"), Neighbour->Position.X,
// 			          Child->Position.X - Child->HalfExtent.X - Neighbour->HalfExtent.X);
// 			TestEqual(TEXT("Neighbour"), Neighbour->Neighbour[0], Children.ChildrenName[i]);
// 		}
// 		if (!Child->Neighbour[2].IsNone())
// 		{
// 			Neighbour = Children.Children[int(
// 				std::find(Children.ChildrenName, &Children.ChildrenName[8], Child->Neighbour[2]) -
// 				Children.ChildrenName)].Get();
//
// 			TestEqual(TEXT("Y Positive Position"), Neighbour->Position.Y,
// 			          Child->Position.Y + Child->HalfExtent.Y + Neighbour->HalfExtent.Y);
// 			TestEqual(TEXT("Neighbour"), Neighbour->Neighbour[3], Children.ChildrenName[i]);
// 		}
// 		if (!Child->Neighbour[3].IsNone())
// 		{
// 			Neighbour = Children.Children[int(
// 				std::find(Children.ChildrenName, &Children.ChildrenName[8], Child->Neighbour[3]) -
// 				Children.ChildrenName)].Get();
//
// 			TestEqual(TEXT("Y Negative Position"), Neighbour->Position.Y,
// 			          Child->Position.Y - Child->HalfExtent.Y - Neighbour->HalfExtent.Y);
// 			TestEqual(TEXT("Neighbour"), Neighbour->Neighbour[2], Children.ChildrenName[i]);
// 		}
// 		if (!Child->Neighbour[4].IsNone())
// 		{
// 			Neighbour = Children.Children[int(
// 				std::find(Children.ChildrenName, &Children.ChildrenName[8], Child->Neighbour[4]) -
// 				Children.ChildrenName)].Get();
//
// 			TestEqual(TEXT("Z Positive Position"), Neighbour->Position.Z,
// 			          Child->Position.Z + Child->HalfExtent.Z + Neighbour->HalfExtent.Z);
// 			TestEqual(TEXT("Neighbour"), Neighbour->Neighbour[5], Children.ChildrenName[i]);
// 		}
// 		if (!Child->Neighbour[5].IsNone())
// 		{
// 			Neighbour = Children.Children[int(
// 				std::find(Children.ChildrenName, &Children.ChildrenName[8], Child->Neighbour[5]) -
// 				Children.ChildrenName)].Get();
//
// 			TestEqual(TEXT("Z Negative Position"), Neighbour->Position.Z,
// 			          Child->Position.Z - Child->HalfExtent.Z - Neighbour->HalfExtent.Z);
//
// 			TestEqual(TEXT("Neighbour"), Neighbour->Neighbour[4], Children.ChildrenName[i]);
// 		}
// 		i++;
// 	}
// 	return true;
// }
