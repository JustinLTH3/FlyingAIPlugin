// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "NavFilters/NavigationQueryFilter.h"
#include "AITypes.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BehaviorTree/Tasks/BTTask_MoveDirectlyToward.h"
#include "BTTask_FlyTo.generated.h"

class UAITask_MoveTo;

/**
 *
 */
UCLASS(config=Game)
class FABEHAVIOURTREE_API UBTTask_FlyTo : public UBTTask_MoveDirectlyToward
{
	GENERATED_BODY()

public:
	UBTTask_FlyTo(const FObjectInitializer& ObjectInitializer);
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
protected:
	virtual UAITask_MoveTo* PrepareMoveTask(UBehaviorTreeComponent& OwnerComp,
	                                        UAITask_MoveTo* ExistingTask,
	                                        FAIMoveRequest& MoveRequest) override;
	UPROPERTY(EditAnywhere, Category=FA)
	struct FBlackboardKeySelector ColliderSizeKey;
};
