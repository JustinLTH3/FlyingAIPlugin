// Fill out your copyright notice in the Description page of Project Settings.

#include "FAGenUtilityWidget.h"

#include "FABound.h"
#include "FANodeGenSubsystem.h"
#include "Components/Button.h"
#include "Components/SinglePropertyView.h"
#include "Components/DetailsView.h"

void UFAGenUtilityWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GenerateButton->OnClicked.AddDynamic(this, &UFAGenUtilityWidget::GenerateButtonClicked);

	Path->SetObject(this);
	Path->SetPropertyName(TEXT("DirectoryPath"));
	BoundDataSView->SetObject(this);
	BoundDataSView->SetPropertyName(TEXT("SelectedBound"));

	MaxDepthView->SetObject(this);
	MaxDepthView->SetPropertyName(TEXT("MaxDepth"));
}

void UFAGenUtilityWidget::GenerateButtonClicked()
{
	UE_LOG(LogTemp, Display, TEXT("GenerateButtonClicked"));
	FString PathString = DirectoryPath.Path + "/";
	if (FPaths::IsUnderDirectory(PathString, FPaths::ProjectContentDir()))
	{
		FString PathString2 = FPaths::ProjectContentDir();
		PathString.RemoveFromStart(PathString2);
		PathString = "/Game/" + PathString;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Path needs to be in the project's Content folder."));
		return;
	}
	auto World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("World is null"));
		return;
	}
	auto system = World->GetSubsystem<UFANodeGenSubsystem>();
	if (!system)
	{
		UE_LOG(LogTemp, Error, TEXT("FA World Subsystem is null"));
		return;
	}
	system->GenerateBoundNodes(PathString, World, MaxDepth, SelectedBound.LoadSynchronous());
}
