// Fill out your copyright notice in the Description page of Project Settings.

#include "FAPathfindingAlgo.h"
#include "FAWorldSubsystem.h"
#include "FABoundData.h"
#include "FANeighbourData.h"
#include "FABound.h"
#include "FAPathfindingSettings.h"
#include "Kismet/KismetSystemLibrary.h"

uint32 UFAPathfindingAlgo::PathGenCalledNum = 0;
UE::FSpinLock UFAPathfindingAlgo::PathGenCalledNumLock = UE::FSpinLock();

void UFAPathfindingAlgo::GeneratePath(FFAFinePath& FinePath, const FFAPathNodeData& EndNode,
                                      UWorld* World, const UFAPathfindingSettings* Settings,
                                      const FVector ColliderSize, const FVector ColliderOffset) const
{
	ON_SCOPE_EXIT
	{
		UE::TScopeLock Lock(PathGenCalledNumLock);
		PathGenCalledNum--;
	};
	{
		UE::TScopeLock Lock(PathGenCalledNumLock);
		PathGenCalledNum++;
	}
	auto& HPANodes = FinePath.HPAPath.HPANodes;
	if (HPANodes.Num() == 0) return;
	bool NextHPANodeIndexExists = FinePath.CurrentHPANodeIndex + 1 == HPANodes.Num();
	auto EndHPANodeIndex = NextHPANodeIndexExists
		                       ? FinePath.CurrentHPANodeIndex
		                       : FinePath.CurrentHPANodeIndex + 1;
	auto StartHPANode = HPANodes[FinePath.CurrentHPANodeIndex];
	auto EndHPANode = HPANodes[EndHPANodeIndex];

	//First hpa node should be where start node in.
	if (FinePath.LocalStartNode.NodeData.HPANodeIndex != StartHPANode) return;
	bool bShouldFindEndNode = EndNode.NodeData.HPANodeIndex == EndHPANode;
	//If end node is not in the last hpa node, if hpa nodes num is 1, return.
	if (!bShouldFindEndNode && NextHPANodeIndexExists) return;
	//else when find a node in end hpa node, path is found.

	bool PathFound = false;
	bool bIsDifferentBound = FinePath.LocalStartNode.NodeBound != FinePath.HPAPath.
		HPAAssociateBounds[EndHPANodeIndex];
	UFANeighbourData* SavedNeighbourData = nullptr;
	if (bIsDifferentBound)
	{
		SavedNeighbourData = FinePath.LocalStartNode.NodeBound->FindNeighboursData(
			FinePath.LocalStartNode.NodeBound,
			FinePath.HPAPath.HPAAssociateBounds[EndHPANodeIndex]);
		check(SavedNeighbourData);
	}
	FScopeLock LockBound0(
		&FinePath.HPAPath.HPAAssociateBounds[FinePath.CurrentHPANodeIndex]->GetNodesDataLock());
	if (bIsDifferentBound) FinePath.HPAPath.HPAAssociateBounds[EndHPANodeIndex]->GetNodesDataLock().
		Lock();

	TMap<FString, FAPathfindingData> OpenSet, ClosedSet;
	TMap<FString, FString> PathLink;
	FString StartNodeName = FString::Printf(
		TEXT("%s%p"), *FinePath.LocalStartNode.NodeName.ToString(),
		FinePath.LocalStartNode.NodeBound);
	FString CurrentNode = FString::Printf(
		TEXT("%s%p"), *FinePath.LocalStartNode.NodeName.ToString(),
		FinePath.LocalStartNode.NodeBound);
	FString EndNodeName = FString::Printf(
		TEXT("%s%p"), *FinePath.HPAPath.EndNode.NodeName.ToString(),
		FinePath.HPAPath.EndNode.NodeBound);

	OpenSet.Add(CurrentNode,
	            FAPathfindingData(FinePath.LocalStartNode, FinePath.LocalStartLocation,
	                              FVector2D(0, 0)));

	while (OpenSet.Num() > 0 && !PathFound)
	{
		auto It = OpenSet.CreateIterator();
		CurrentNode = It.Key();
		for (++It; It; ++It)
		{
			//if the node has less fCost than the current node, or the same but less Hcost
			if (It.Value().Cost.X + It.Value().Cost.Y < OpenSet[CurrentNode].Cost.X + OpenSet[
				CurrentNode].Cost.Y || (It.Value().Cost.X + It.Value().Cost.Y == OpenSet[
				CurrentNode].Cost.X + OpenSet[CurrentNode].Cost.Y && It.Value().Cost.Y < OpenSet[
				CurrentNode].Cost.Y))
			{
				CurrentNode = It.Key();
			}
		}
		ClosedSet.Add(CurrentNode);
		OpenSet.RemoveAndCopyValue(CurrentNode, ClosedSet[CurrentNode]);

		if (CurrentNode == EndNodeName)
		{
			PathFound = true;
			continue;
			//Retrace Path
		}
		if (ClosedSet[CurrentNode].Data.NodeData.HPANodeIndex == EndHPANode && !bShouldFindEndNode)
		{
			PathFound = true;
			continue;
			//Retrace Path
		}

		AFABound* Bound = ClosedSet[CurrentNode].Data.NodeBound;
		for (auto& CurrentNeighbour : ClosedSet[CurrentNode].Data.NodeData.Neighbour)
		{
			if (CurrentNeighbour.IsNone()) continue;

			FFaNodeData* Neighbour = Bound->GetNodesData()->FindRow<FFaNodeData>(
				CurrentNeighbour, "");
			FFAPathNodeData NeighbourData{
				.NodeData = *Neighbour, .NodeName = CurrentNeighbour, .NodeBound = Bound
			};
			NeighbourData.NodeData.HPANodeIndex = Neighbour->HPANodeIndex == INDEX_NONE
				                                      ? INDEX_NONE
				                                      : Bound->GetLocalToGlobalHPANodes()[Neighbour
					                                      ->HPANodeIndex];
			NeighbourData.NodeData.Position -= Bound->GetBoundData()->GeneratePosition;
			NeighbourData.NodeData.Position += Bound->GetActorLocation();
			FString NeighbourName = FString::Printf(
				TEXT("%s%p"), *NeighbourData.NodeName.ToString(), NeighbourData.NodeBound);

			if (NeighbourData.NodeData.HPANodeIndex != INDEX_NONE && NeighbourData.NodeData.
				HPANodeIndex != StartHPANode && NeighbourData.NodeData.HPANodeIndex != EndHPANode)
				continue;
			if (!NeighbourData.NodeData.IsTraversable || ClosedSet.Contains(NeighbourName))
			{
				continue;
			}

			FVector ij = NeighbourData.NodeData.Position - ClosedSet[CurrentNode].Data.NodeData.
				Position;
			ij.Normalize();
			ij *= ClosedSet[CurrentNode].Data.NodeData.HalfExtent;
			ij += ClosedSet[CurrentNode].Data.NodeData.Position;
			{
				TArray<AActor*> Actors;
				if (UKismetSystemLibrary::BoxOverlapActors(World, ij+ColliderOffset, ColliderSize,
				                                           Settings->ObjectTypes,
				                                           Settings->EnvironmentActorClass, {},
				                                           Actors)) continue;
			}

			float newMoveCost = ClosedSet[CurrentNode].Cost.X + FVector::Distance(
				ClosedSet[CurrentNode].Data.NodeData.Position, NeighbourData.NodeData.Position);

			//if the new move costs less or this neighbour isnt in the open set
			if (!OpenSet.Contains(NeighbourName))
			{
				OpenSet.Add(NeighbourName,
				            FAPathfindingData(NeighbourData, ij, FVector2D(
					                              newMoveCost,
					                              FVector::Distance(
						                              NeighbourData.NodeData.Position,
						                              EndNode.NodeData.Position))));
				PathLink.Add(NeighbourName, CurrentNode);
			}
			else if (newMoveCost < OpenSet[NeighbourName].Cost.X)
			{
				OpenSet[NeighbourName].Cost.X = newMoveCost;
				OpenSet[NeighbourName].StartLocation = ij;
				OpenSet[NeighbourName].Cost.Y = FVector::Distance(ij, EndNode.NodeData.Position);
				PathLink.FindOrAdd(NeighbourName) = CurrentNode;
			}
		}

		if (!bIsDifferentBound) continue;

		bool bHasNeighbourInDifferentBound = ClosedSet[CurrentNode].Data.NodeBound ==
		                                     SavedNeighbourData->Bound[0]
			                                     ? SavedNeighbourData->Connection0.Contains(
				                                     ClosedSet[CurrentNode].Data.NodeName)
			                                     : SavedNeighbourData->Connection1.Contains(
				                                     ClosedSet[CurrentNode].Data.NodeName);
		if (!bHasNeighbourInDifferentBound) continue;
		bool equalBound0 = ClosedSet[CurrentNode].Data.NodeBound == SavedNeighbourData->Bound[0];
		auto& NeighbourConnectionData = equalBound0
			                                ? SavedNeighbourData->Connection0[ClosedSet[CurrentNode]
				                                .Data.NodeName].Connected
			                                : SavedNeighbourData->Connection1[ClosedSet[CurrentNode]
				                                .Data.NodeName].Connected;
		Bound = equalBound0 ? SavedNeighbourData->Bound[1] : SavedNeighbourData->Bound[0];
		for (auto& ConnectedNeighbour : NeighbourConnectionData)
		{
			FFaNodeData* Neighbour = Bound->GetNodesData()->FindRow<FFaNodeData>(
				ConnectedNeighbour, "");

			FFAPathNodeData NeighbourData{
				.NodeData = *Neighbour, .NodeName = ConnectedNeighbour, .NodeBound = Bound
			};
			NeighbourData.NodeData.HPANodeIndex = Neighbour->HPANodeIndex == INDEX_NONE
				                                      ? INDEX_NONE
				                                      : Bound->GetLocalToGlobalHPANodes()[Neighbour
					                                      ->HPANodeIndex];
			NeighbourData.NodeData.Position += Bound->GetActorLocation() - Bound->GetBoundData()->
				GeneratePosition;
			FString NeighbourName = FString::Printf(
				TEXT("%s%p"), *NeighbourData.NodeName.ToString(), NeighbourData.NodeBound);

			if (NeighbourData.NodeData.HPANodeIndex != INDEX_NONE && NeighbourData.NodeData.
				HPANodeIndex != StartHPANode && NeighbourData.NodeData.HPANodeIndex != EndHPANode)
				continue;
			if (!NeighbourData.NodeData.IsTraversable || ClosedSet.Contains(NeighbourName))
			{
				continue;
			}

			FVector ij = NeighbourData.NodeData.Position - ClosedSet[CurrentNode].Data.NodeData.
				Position;
			ij.Normalize();
			ij *= ClosedSet[CurrentNode].Data.NodeData.HalfExtent;
			ij += ClosedSet[CurrentNode].Data.NodeData.Position;

			{
				TArray<AActor*> Actors;
				if (UKismetSystemLibrary::BoxOverlapActors(World, ij+ColliderOffset, ColliderSize,
				                                           Settings->ObjectTypes,
				                                           Settings->EnvironmentActorClass, {},
				                                           Actors)) continue;
			}

			float newMoveCost = ClosedSet[CurrentNode].Cost.X + FVector::Distance(
				ClosedSet[CurrentNode].Data.NodeData.Position, NeighbourData.NodeData.Position);

			//if the new move costs less or this neighbour isnt in the open set
			if (!OpenSet.Contains(NeighbourName))
			{
				OpenSet.Add(NeighbourName,
				            FAPathfindingData(NeighbourData, ij, FVector2D(
					                              newMoveCost,
					                              FVector::Distance(
						                              ij, EndNode.NodeData.Position))));
				PathLink.Add(NeighbourName, CurrentNode);
			}
			else if (newMoveCost < OpenSet[NeighbourName].Cost.X)
			{
				OpenSet[NeighbourName].StartLocation = ij;
				OpenSet[NeighbourName].Cost.X = newMoveCost;
				OpenSet[NeighbourName].Cost.Y = FVector::Distance(ij, EndNode.NodeData.Position);
				PathLink.FindOrAdd(NeighbourName) = CurrentNode;
			}
		}
	}
	if (bIsDifferentBound) FinePath.HPAPath.HPAAssociateBounds[EndHPANodeIndex]->GetNodesDataLock().
		Unlock();
	if (!PathFound) return;
	while (CurrentNode != StartNodeName)
	{
		FinePath.Nodes.Add(ClosedSet[CurrentNode].Data);
		FinePath.ControlPoints.Add(ClosedSet[CurrentNode].StartLocation);
		PathLink.RemoveAndCopyValue(CurrentNode, CurrentNode);
	}
	FinePath.Nodes.Add(ClosedSet[CurrentNode].Data);
	FinePath.ControlPoints.Add(ClosedSet[CurrentNode].StartLocation);
	if (FinePath.CurrentHPANodeIndex == 0 && FinePath.ControlPoints.Num() > 1)
	{
		FVector x = 2 * FinePath.ControlPoints.Last() - FinePath.ControlPoints.Last(1);
		FinePath.ControlPoints.Add(x);
	}
	Algo::Reverse(FinePath.Nodes);
	Algo::Reverse(FinePath.ControlPoints);
	FVector x;
	if (bShouldFindEndNode)
	{
		x = FinePath.HPAPath.EndLocation;
		FinePath.ControlPoints.Add(x);
	}
	if (FinePath.ControlPoints.Num() > 1)
	{
		x = 2 * FinePath.ControlPoints.Last() - FinePath.ControlPoints.Last(1);
		FinePath.ControlPoints.Add(x);
	}
	FinePath.bIsSuccess = true;
}
