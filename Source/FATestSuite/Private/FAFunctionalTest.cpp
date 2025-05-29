// Fill out your copyright notice in the Description page of Project Settings.

#include "FAFunctionalTest.h"

#include "Engine/TriggerBox.h"

AFAFunctionalTest::AFAFunctionalTest()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AFAFunctionalTest::StartTest()
{
	Super::StartTest();
	check(Trigger);
	Trigger->OnActorBeginOverlap.AddDynamic(this, &AFAFunctionalTest::OnTriggerOverlap);
}

void AFAFunctionalTest::OnTriggerOverlap(AActor* Actor, AActor* OtherActor)
{
	if (OtherActor == AI)
	{
		FinishTest(EFunctionalTestResult::Succeeded, TEXT(""));
	}
}

void AFAFunctionalTest::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
