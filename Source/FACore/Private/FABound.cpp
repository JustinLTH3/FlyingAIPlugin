// Fill out your copyright notice in the Description page of Project Settings.

#include "FABound.h"

#include "FANeighbourData.h"
#include "FAWorldSubsystem.h"
#include "FAPathfindingAlgo.h"
#include "Engine/AssetManager.h"

// Sets default values
AFABound::AFABound()
{
	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	SetRootComponent(BoxComponent);
}

void AFABound::BeginDestroy()
{
	//Wait for all pathfinding ends due to this might be referenced in the algorithm.
	FPlatformProcess::ConditionalSleep([] { return !UFAPathfindingAlgo::IsGenerating(); });
	Super::BeginDestroy();
}

void AFABound::Subdivide(FFANewNodeChildType& ChildrenNodes)
{
	const FVector NewHalfExtent = GetHalfExtent() / 2;
	for (uint8 i = 0; i < 8; ++i)
	{
		FVector NewPosition = GetActorLocation();
		NewPosition += NewHalfExtent * FVector(FA::XPositiveChildrenIndex.Contains(i) ? 1 : -1,
		                                       FA::YPositiveChildrenIndex.Contains(i) ? 1 : -1,
		                                       FA::ZPositiveChildrenIndex.Contains(i) ? 1 : -1);
		ChildrenNodes.Children[i] = ChildrenNodes.Children[i]
			                            ? ChildrenNodes.Children[i]
			                            : MakeShared<FFaNodeData>();
		ChildrenNodes.Children[i]->Position = NewPosition;
		ChildrenNodes.Children[i]->Depth = 0;
		ChildrenNodes.Children[i]->HalfExtent = NewHalfExtent;
	}
}

TSharedPtr<FStreamableHandle> AFABound::GetNodeData()
{
	return LoadedDataHandle;
}

void AFABound::SetLOD(uint8 InLOD)
{
	InLOD = FMath::Clamp(InLOD, 0, 2);
	if (InLOD == LOD) return;
	LOD = InLOD;
	OnLODChanged();
}

void AFABound::OnLODChanged()
{
	switch (LOD)
	{
	case 0:
		if (!LoadedDataHandle.IsValid())
		{
			LoadNodes();
		}
		else if (!LoadedDataHandle->IsActive())
		{
			LoadNodes();
		}
		for (auto& m : NeighboursData)
		{
			if (!m.Value)
			{
				m.Value = GetWorld()->GetSubsystem<UFAWorldSubsystem>()->GetNeighbourData(m.Key);
			}
		}
		break;
	case 1:
		if (!LoadedDataHandle.IsValid())
		{
			LoadNodesAsync();
		}
		else if (!LoadedDataHandle->IsActive())
		{
			LoadNodesAsync();
		}
		break;
	case 2:
		UnloadNodes();
		for (auto& m : NeighboursData)
		{
			m.Value = nullptr;
		}
		break;
	default: checkNoEntry();
	}
}

void AFABound::LoadNodes()
{
	UE::TScopeLock Lock(NodesDataLock);
	if (LoadedDataHandle.IsValid() && LoadedDataHandle->IsActive()) return;
	LoadedDataHandle = UAssetManager::GetStreamableManager().RequestSyncLoad(
		BoundData->CombinedNodes.ToSoftObjectPath());
	NodesData = Cast<UDataTable>(LoadedDataHandle->GetLoadedAsset());
}

void AFABound::LoadNodesAsync()
{
	UE::TScopeLock Lock(NodesDataLock);
	LoadedDataHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		BoundData->CombinedNodes.ToSoftObjectPath(), FStreamableDelegate::CreateLambda([this]
		{
			UE::TScopeLock Lock(NodesDataLock);
			NodesData = Cast<UDataTable>(LoadedDataHandle->GetLoadedAsset());
		}));
}

void AFABound::UnloadNodes()
{
	LoadedDataHandle.Reset();
	NodesData = nullptr;
}

void AFABound::AddNeighbourData(FString InName, UFANeighbourData* Data)
{
	NeighboursData.FindOrAdd(InName, Data);
}

UFANeighbourData* AFABound::FindNeighboursData(AFABound* Bound0, AFABound* Bound1)
{
	for (auto Data : NeighboursData)
	{
		if (Data.Value)
		{
			if ((Data.Value->Bound[0] == Bound0 && Data.Value->Bound[1] == Bound1) || (Data.Value->
				Bound[0] == Bound1 && Data.Value->Bound[1] == Bound0)) return Data.Value.Get();
		}
	}
	return nullptr;
}
