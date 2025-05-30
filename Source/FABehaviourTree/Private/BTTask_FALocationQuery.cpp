// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_FALocationQuery.h"

#include "FALocationQuerySubsystem.h"
#include "FAWorldSubsystem.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Tasks/BTTask_RunEQSQuery.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "GameFramework/Pawn.h"
#include "Async/Async.h"
#include <VisualLogger/VisualLogger.h>
#include "HLSLTree/HLSLTreeTypes.h"

DEFINE_LOG_CATEGORY(LogFABT);

UBTTask_FALocationQuery::UBTTask_FALocationQuery(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = "Run Location Query";
	ColliderSizeKey.AddVectorFilter(
		this, GET_MEMBER_NAME_CHECKED(UBTTask_FALocationQuery, ColliderSizeKey));
}

EBTNodeResult::Type UBTTask_FALocationQuery::ExecuteTask(UBehaviorTreeComponent& OwnerComp,
                                                         uint8* NodeMemory)
{
	if (BlackboardKey.SelectedKeyType != UBlackboardKeyType_Vector::StaticClass())
		return EBTNodeResult::Failed;
	FBT_FALocationQueryTaskMemory* Memory = CastInstanceNodeMemory<
		FBT_FALocationQueryTaskMemory>(NodeMemory);
	auto bb = OwnerComp.GetBlackboardComponent();
	FVector ColliderSize = bb->GetValue<UBlackboardKeyType_Vector>(
		ColliderSizeKey.GetSelectedKeyID());
	ColliderSize = ColliderSize == UBlackboardKeyType_Vector::InvalidValue
					   ? FVector::ZeroVector
					   : ColliderSize;


	AActor* QueryOwner = OwnerComp.GetOwner();
	AController* ControllerOwner = Cast<AController>(QueryOwner);

	if (!GetWorld()->GetSubsystem<UFAWorldSubsystem>()) return EBTNodeResult::Failed;
	TUniqueFunction<void()> Callback = [this, ControllerOwner]
	{
		AsyncTask(ENamedThreads::GameThread, [this, ControllerOwner]
		{
			UBehaviorTreeComponent* MyComp = ControllerOwner
				                                 ? ControllerOwner->FindComponentByClass<
					                                 UBehaviorTreeComponent>()
				                                 : nullptr;
			if (!MyComp)
			{
				UE_LOG(LogFABT, Warning,
				       TEXT( "Unable to find behavior tree to notify about finished query from %s!"
				       ), *GetNameSafe(ControllerOwner));
				FinishLatentTask(*MyComp, EBTNodeResult::Failed);
				return;
			}
			auto mem = CastInstanceNodeMemory<FBT_FALocationQueryTaskMemory>(
				MyComp->GetNodeMemory(this, MyComp->FindInstanceContainingNode(this)));
			if (mem && mem->Location.IsValid() && mem->Location.Get() !=
				UFALocationQuerySubsystem::GetNullValue())
			{
				MyComp->GetBlackboardComponent()->SetValue<UBlackboardKeyType_Vector>(
					BlackboardKey.GetSelectedKeyID(), mem->Location.Get());
				UE_VLOG_LOCATION(ControllerOwner, LogFABT, Display, mem->Location.Get(), 0.1f,
				                 FColor::Red, TEXT("Location Query"));
				mem->Location.Reset();
				FinishLatentTask(*MyComp, EBTNodeResult::Succeeded);
			}
			else
			{
				mem->Location.Reset();
				FinishLatentTask(*MyComp, EBTNodeResult::Failed);
			}
		});
	};
	Memory->Location = AsyncPool(*GetWorld()->GetSubsystem<UFAWorldSubsystem>()->GetThreadPool(),
	                             [this, &OwnerComp, &ColliderSize]
	                             {
		                             return GetWorld()->GetSubsystem<UFALocationQuerySubsystem>()->
		                                                GetRandomReachableLocation(
			                                                ColliderSize,
			                                                ColliderSize.Z * ColliderSize.UnitZ());
	                             }, MoveTemp(Callback));
	if (Memory->Location.IsValid() && Memory->Location.IsReady())
	{
		if (Memory->Location.Get() == UFALocationQuerySubsystem::GetNullValue())
			return EBTNodeResult::Failed;
		OwnerComp.GetBlackboardComponent()->SetValue<UBlackboardKeyType_Vector>(
			BlackboardKey.GetSelectedKeyID(), Memory->Location.Get());
		UE_VLOG_LOCATION(ControllerOwner, LogFABT, Display, Memory->Location.Get(), 0.1f,
		                 FColor::Red, TEXT("Location Query"));
		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::InProgress;
}

EBTNodeResult::Type UBTTask_FALocationQuery::AbortTask(UBehaviorTreeComponent& OwnerComp,
                                                       uint8* NodeMemory)
{
	FBT_FALocationQueryTaskMemory* Memory = CastInstanceNodeMemory<
		FBT_FALocationQueryTaskMemory>(NodeMemory);
	if (Memory->Location.IsValid()) Memory->Location.Reset();
	return EBTNodeResult::Aborted;
}

uint16 UBTTask_FALocationQuery::GetInstanceMemorySize() const
{
	return sizeof(FBT_FALocationQueryTaskMemory);
}

void UBTTask_FALocationQuery::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	UBlackboardData* BBAsset = GetBlackboardAsset();
	if (BBAsset)
	{
		ColliderSizeKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		UE_LOG(LogFABT, Warning,
		       TEXT(
			       "Can't initialize task: %s, make sure that behavior tree specifies blackboard asset!"
		       ), *GetName());
	}
}

void UBTTask_FALocationQuery::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp,
                                                    uint8* NodeMemory,
                                                    EBTDescriptionVerbosity::Type Verbosity,
                                                    TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	if (Verbosity == EBTDescriptionVerbosity::Detailed)
	{
		FBT_FALocationQueryTaskMemory* MyMemory = (FBT_FALocationQueryTaskMemory*)NodeMemory;
		Values.Add(FString::Printf(TEXT("request ready: %d"), MyMemory->Location.IsReady()));
	}
}
