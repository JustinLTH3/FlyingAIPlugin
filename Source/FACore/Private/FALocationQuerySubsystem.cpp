// Fill out your copyright notice in the Description page of Project Settings.

#include "FALocationQuerySubsystem.h"

#include "FABound.h"
#include "FAPathfindingSettings.h"
#include "Algo/SelectRandomWeighted.h"
#include "Kismet/KismetSystemLibrary.h"

FVector UFALocationQuerySubsystem::NullValue = FVector(UE_MAX_FLT);

bool UFALocationQuerySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer)) return false;

	UWorld* World = Cast<UWorld>(Outer);
	if (World->WorldType == EWorldType::Editor) return false;

	UFAPathfindingSettings* Settings = GetMutableDefault<UFAPathfindingSettings>();
	TArray<TSoftObjectPtr<UWorld>> temp;
	Settings->MapsSettings.GetKeys(temp);
	auto WorldName = World->GetPathName().Replace(*World->GetMapName(),
	                                              *UWorld::RemovePIEPrefix(World->GetMapName()),
	                                              ESearchCase::Type::CaseSensitive);
	auto ptr = temp.FindByPredicate([World,WorldName](TSoftObjectPtr<UWorld>& a)
	{
		UE_LOG(LogTemp, Display, TEXT("%s == %s"), *a->GetPathName(), *WorldName);
		return a->GetPathName() == WorldName;
	});
	return ptr != nullptr && Settings->MapsSettings[*ptr].bUseLocationQuery;
}

void UFALocationQuerySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	UFAPathfindingSettings* Settings = GetMutableDefault<UFAPathfindingSettings>();
	EnvironmentActorClass = Settings->EnvironmentActorClass;
	ObjectTypes = Settings->ObjectTypes;
	UFAWorldSubsystem* System = GetWorld()->GetSubsystem<UFAWorldSubsystem>();
	System->GetOnSystemReady().AddLambda([this] { bReady = true; });
}

FVector UFALocationQuerySubsystem::GetRandomReachableLocation(
	FVector ColliderSize, FVector ColliderOffset, int MaxSamplings)
{
	TArray<FFaNodeData*> Datas;
	UFAWorldSubsystem* System = GetWorld()->GetSubsystem<UFAWorldSubsystem>();
	if (!bReady) return NullValue;
	auto Bounds = System->GetRegisteredBound();
	Bounds.RemoveAll([](AFABound* Bound) { return !Bound->GetNodesData(); });
	int Samplings = 0;
	while (Samplings < MaxSamplings)
	{
		Datas.Empty();
		auto Bound = *Algo::SelectRandomWeightedBy(Bounds, [](AFABound* a)
		{
			return a->GetHalfExtent().SquaredLength();
		});
		Bound->GetNodesData()->GetAllRows("", Datas);

		Datas.RemoveAll([](FFaNodeData* Data)
		{
			return !Data->IsTraversable;
		});
		if (Datas.Num() == 0) return FVector::Zero();
		Datas.Shrink();
		auto result = *Algo::SelectRandomWeightedBy(Datas, [](FFaNodeData* a)
		{
			return a->HalfExtent.SquaredLength();
		});

		FVector ReachableLocation = FMath::RandPointInBox(FBox(
			result->Position - result->HalfExtent, result->Position + result->HalfExtent));
		ReachableLocation -= Bound->GetBoundData()->GeneratePosition;
		ReachableLocation += Bound->GetActorLocation();
		TArray<AActor*> Actors;
		if (!UKismetSystemLibrary::BoxOverlapActors(GetWorld(), ReachableLocation + ColliderOffset,
		                                            ColliderSize, ObjectTypes,
		                                            EnvironmentActorClass, {},
		                                            Actors)) return ReachableLocation;
		Samplings++;
	}
	return NullValue;
}
