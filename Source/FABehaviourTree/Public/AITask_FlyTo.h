// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FAWorldSubsystem.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "Tasks/AITask_MoveTo.h"
#include "AITask_FlyTo.generated.h"

FABEHAVIOURTREE_API DECLARE_LOG_CATEGORY_EXTERN(LogFAAITask, Display, All);

/**
 *
 */
UCLASS()
class FABEHAVIOURTREE_API UAITask_FlyTo : public UAITask_MoveTo
{
	GENERATED_BODY()

public:
	UAITask_FlyTo(const FObjectInitializer& ObjectInitializer);

	void SetColliderSize(const FVector& InColliderSize)
	{
		ColliderSize = InColliderSize == UBlackboardKeyType_Vector::InvalidValue
			               ? FVector::ZeroVector
			               : InColliderSize;
	};

protected:
	virtual void PerformMove() override;
	bool AdjustInitialPath(UPathFollowingComponent* PFComp);
	virtual void
	OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result) override;
	void AddNextPath(const FFAFinePath& NewNextPath, FNavPathSharedPtr InPath);
	UE::Tasks::FTask GenTask;
	FVector ColliderSize;
	bool bIsStillAdjustingPath = false;
	FTimerHandle PathFinishDelegateHandle;
};
