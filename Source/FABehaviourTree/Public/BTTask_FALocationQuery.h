// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_FALocationQuery.generated.h"

struct FBT_FALocationQueryTaskMemory
{
	TFuture<FVector> Location;
};
DECLARE_LOG_CATEGORY_EXTERN(LogFABT, Log, All);
/**
 * 
 */
UCLASS()
class FABEHAVIOURTREE_API UBTTask_FALocationQuery : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_FALocationQuery(const FObjectInitializer& ObjectInitializer);
	virtual EBTNodeResult::Type
	ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual uint16 GetInstanceMemorySize() const override;
	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
protected:
	UPROPERTY(EditAnywhere, Category=FA)
	struct FBlackboardKeySelector ColliderSizeKey;
};
