// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_FlyTo.h"

#include "AIController.h"
#include "AITask_FlyTo.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Tasks/AITask_MoveTo.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "VisualLogger/VisualLogger.h"

UBTTask_FlyTo::UBTTask_FlyTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NodeName = "Fly To";

	bRequireNavigableEndLocation = false;
	bProjectGoalLocation = false;
	bUsePathfinding = false;
	ColliderSizeKey.AddVectorFilter(this, GET_MEMBER_NAME_CHECKED(UBTTask_FlyTo, ColliderSizeKey));
}

EBTNodeResult::Type UBTTask_FlyTo::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	if (!GetWorld()->GetSubsystem<UFAWorldSubsystem>()) return EBTNodeResult::Failed;
	return Super::ExecuteTask(OwnerComp, NodeMemory);
}

void UBTTask_FlyTo::InitializeFromAsset(UBehaviorTree& Asset)
{
	Super::InitializeFromAsset(Asset);
	UBlackboardData* BBAsset = GetBlackboardAsset();
	if (BBAsset)
	{
		ColliderSizeKey.ResolveSelectedKey(*BBAsset);
	}
	else
	{
		UE_LOG(LogBehaviorTree, Warning,
		       TEXT(
			       "Can't initialize task: %s, make sure that behavior tree specifies blackboard asset!"
		       ), *GetName());
	}
}

UAITask_MoveTo* UBTTask_FlyTo::PrepareMoveTask(UBehaviorTreeComponent& OwnerComp,
                                               UAITask_MoveTo* ExistingTask,
                                               FAIMoveRequest& MoveRequest)
{
	UAITask_FlyTo* MoveTask = ExistingTask
		                          ? Cast<UAITask_FlyTo>(ExistingTask)
		                          : NewBTAITask<UAITask_FlyTo>(OwnerComp);
	if (MoveTask)
	{
		MoveTask->SetUp(MoveTask->GetAIController(), MoveRequest);
		if (ColliderSizeKey.SelectedKeyType == UBlackboardKeyType_Vector::StaticClass())
		{
			FVector ColliderSize = OwnerComp.GetBlackboardComponent()->GetValue<
				UBlackboardKeyType_Vector>(ColliderSizeKey.GetSelectedKeyID());
			MoveTask->SetColliderSize(ColliderSize);
		}
	}

	return MoveTask;
}
