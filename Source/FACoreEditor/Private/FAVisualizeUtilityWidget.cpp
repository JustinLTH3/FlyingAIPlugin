// Fill out your copyright notice in the Description page of Project Settings.

#include "..\Public\FAVisualizeUtilityWidget.h"

#include "FANode.h"
#include "Components/Button.h"
#include "Components/SinglePropertyView.h"

void UFAVisualizeUtilityWidget::NativeConstruct()
{
	Super::NativeConstruct();
	VLogButton->OnClicked.AddDynamic(this, &UFAVisualizeUtilityWidget::OnVLogButton);

	DataTableSView->SetPropertyName(TEXT("DT"));
	DataTableSView->SetObject(this);

	VLogNonTraversableButton->OnClicked.AddDynamic(
		this, &UFAVisualizeUtilityWidget::OnVLogNonButton);
	VLogSpecificHPANode->OnClicked.AddDynamic(
		this, &UFAVisualizeUtilityWidget::OnVLogSpecificHPANode);

	InputHPANode->SetObject(this);
	InputHPANode->SetPropertyName(TEXT("HPANodeIndex"));
}

void UFAVisualizeUtilityWidget::OnVLogButton()
{
	if (!DT) return;
	for (auto d : DT->GetRowMap())
	{
		auto node = reinterpret_cast<FFaNodeData*>(d.Value);
		if (node->HPANodeIndex == INDEX_NONE) continue;
		UE_VLOG_BOX(GEditor->GetEditorWorldContext().World(), LogTemp, Display,
		            FBox(node->Position-node->HalfExtent,node->Position+ node-> HalfExtent ),
		            FColor::Green, TEXT("%d"), node->HPANodeIndex);
	}
}

void UFAVisualizeUtilityWidget::OnVLogNonButton()
{
	if (!DT) return;
	for (auto d : DT->GetRowMap())
	{
		auto node = reinterpret_cast<FFaNodeData*>(d.Value);
		if (node->IsTraversable) continue;
		UE_VLOG_BOX(GEditor->GetEditorWorldContext().World(), LogTemp, Display,
		            FBox(node->Position-node->HalfExtent,node->Position+ node-> HalfExtent ),
		            FColor::Blue, TEXT(""));
	}
}

void UFAVisualizeUtilityWidget::OnVLogSpecificHPANode()
{
	if (!DT) return;
	for (auto d : DT->GetRowMap())
	{
		auto node = reinterpret_cast<FFaNodeData*>(d.Value);
		if (node->HPANodeIndex != HPANodeIndex) continue;
		UE_VLOG_BOX(GEditor->GetEditorWorldContext().World(), LogTemp, Display,
		            FBox(node->Position-node->HalfExtent,node->Position+ node-> HalfExtent ),
		            FColor::Blue, TEXT(""));
	}
}
