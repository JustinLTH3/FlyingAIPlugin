// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FunctionalTest.h"
#include "FAFunctionalTest.generated.h"

class ATriggerBox;

UCLASS()
class FATESTSUITE_API AFAFunctionalTest : public AFunctionalTest
{
	GENERATED_BODY()

public:
	AFAFunctionalTest();

protected:
	virtual void StartTest() override;
	UPROPERTY(EditAnywhere, Category ="FA|Test")
	ATriggerBox* Trigger = nullptr;
	UPROPERTY(EditAnywhere, Category ="FA|Test")
	APawn* AI;
	UFUNCTION()
	void OnTriggerOverlap(AActor* Actor, AActor* OtherActor);
public:
	virtual void Tick(float DeltaTime) override;
};
