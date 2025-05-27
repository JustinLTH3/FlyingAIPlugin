// Fill out your copyright notice in the Description page of Project Settings.

#include <AITask_FlyTo.h>

#include "AIController.h"
#include "FAWorldSubsystem.h"
#include "GameplayTasksComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PawnMovementComponent.h"
#include "VisualLogger/VisualLogger.h"

DEFINE_LOG_CATEGORY(LogFAAITask);

UAITask_FlyTo::UAITask_FlyTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UAITask_FlyTo::PerformMove()
{
	UPathFollowingComponent* PFComp = OwnerController
		                                  ? GetAIController()->GetPathFollowingComponent()
		                                  : nullptr;
	if (PFComp == nullptr)
	{
		FinishMoveTask(EPathFollowingResult::Invalid);
		return;
	}

	ResetObservers();
	ResetTimers();

	// start new move request
	AdjustInitialPath(PFComp);
}

bool UAITask_FlyTo::AdjustInitialPath(UPathFollowingComponent* PFComp)
{
	UFAWorldSubsystem* system = GetWorld()->GetSubsystem<UFAWorldSubsystem>();
	check(system)
	if (PFComp->HasReached(MoveRequest))
	{
		FinishMoveTask(EPathFollowingResult::Success);
	}
	FFAHPAPath x = system->CreateHPAPath(
		OwnerController->GetPawn()->GetMovementComponent()->GetActorFeetLocation(),
		MoveRequest.GetDestination());
	if (!x.bIsSuccess || x.HPANodes.Num() == 0)
	{
		FinishMoveTask(EPathFollowingResult::Invalid);
		return false;
	}
	UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, x, system, PFComp]
	{
		auto finePath = system->CreateFinePathByHPA(x, ColliderSize,
		                                            ColliderSize.UnitZ() * ColliderSize);
		if (!finePath.bIsSuccess || finePath.Nodes.Num() == 0)
		{
			AsyncTask(ENamedThreads::GameThread, [this, finePath, PFComp]
			{
				FinishMoveTask(EPathFollowingResult::Invalid);
			});
			return;
		}
		system->InterpolateFinePath(finePath);

		AsyncTask(ENamedThreads::GameThread, [this, finePath, PFComp]
		{
			auto ResultData = OwnerController->MoveTo(MoveRequest, &Path);
			switch (ResultData.Code)
			{
			case EPathFollowingRequestResult::Failed:
				FinishMoveTask(EPathFollowingResult::Invalid);
				break;

			case EPathFollowingRequestResult::AlreadyAtGoal:
				MoveRequestID = ResultData.MoveId;
				OnRequestFinished(ResultData.MoveId,
				                  FPathFollowingResult(EPathFollowingResult::Success,
				                                       FPathFollowingResultFlags::AlreadyAtGoal));
				break;

			case EPathFollowingRequestResult::RequestSuccessful:
				bIsStillAdjustingPath = true;
				MoveRequestID = ResultData.MoveId;
				PFComp->OnRequestFinished.AddUObject(this, &UAITask_FlyTo::OnRequestFinished);
				SetObservedPath(Path);
				Path->SetIgnoreInvalidation(true);
				Path->GetPathPoints().Empty();
				AddNextPath(finePath, Path);
				Path->DoneUpdating(ENavPathUpdateType::NavigationChanged);
				if (IsFinished())
				{
					UE_VLOG(GetGameplayTasksComponent(), LogFAAITask, Error,
					        TEXT("%s> re-Activating Finished task!"), *GetName());
				}
				break;

			default: checkNoEntry();
				break;
			}
		});
	});
	return true;
}

void UAITask_FlyTo::OnRequestFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	if (!bIsStillAdjustingPath) Super::OnRequestFinished(RequestID, Result);
	else
	{
		FTimerDelegate Delegate;
		Delegate.BindLambda([this, RequestID, Result]
		{
			OnRequestFinished(RequestID, Result);
		});
		GetWorld()->GetTimerManager().SetTimer(PathFinishDelegateHandle, Delegate, 1, false);
	}
}

void UAITask_FlyTo::AddNextPath(const FFAFinePath& NextPath, FNavPathSharedPtr InPath)
{
	if (!NextPath.bIsSuccess || NextPath.InterpolatedPoints.Num() <= 0)
	{
		bIsStillAdjustingPath = false;
		return;
	}
	if (PathFinishDelegateHandle.IsValid() || !InPath.IsValid())
	{
		UPathFollowingComponent* PFComp = OwnerController
			                                  ? GetAIController()->GetPathFollowingComponent()
			                                  : nullptr;
		auto ResultData = OwnerController->MoveTo(MoveRequest, &Path);
		MoveRequestID = ResultData.MoveId;
		PFComp->OnRequestFinished.AddUObject(this, &UAITask_FlyTo::OnRequestFinished);
		SetObservedPath(Path);
		Path->SetIgnoreInvalidation(true);
		Path->GetPathPoints().Empty();
		InPath = Path;
		GetWorld()->GetTimerManager().ClearTimer(PathFinishDelegateHandle);
	}
	auto& PathPoints = InPath->GetPathPoints();

	PathPoints.Append(NextPath.InterpolatedPoints);
	InPath->DoneUpdating(ENavPathUpdateType::NavigationChanged);
	const auto system = GetWorld()->GetSubsystem<UFAWorldSubsystem>();
	bIsStillAdjustingPath = true;
	GenTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, NextPath, system, InPath]
	{
		auto finePath = system->CreateNextFinePath(NextPath, ColliderSize,
		                                           ColliderSize.UnitZ() * ColliderSize);
		while (!finePath.bBoundLoaded)
		{
			FPlatformProcess::Sleep(0.1f);
			if (TaskState == EGameplayTaskState::Finished) return;
			finePath = system->CreateNextFinePath(NextPath, ColliderSize,
			                                      ColliderSize.UnitZ() * ColliderSize);
		}
		if (finePath.bIsSuccess)
		{
			system->InterpolateFinePath(finePath);
		}
		AsyncTask(ENamedThreads::GameThread, [this, finePath, InPath]
		{
			AddNextPath(finePath, InPath);
		});
	});
}
