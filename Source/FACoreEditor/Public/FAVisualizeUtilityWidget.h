// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "FAVisualizeUtilityWidget.generated.h"

class USinglePropertyView;

class UButton;

/**
 *
 */
UCLASS()
class FACOREEDITOR_API UFAVisualizeUtilityWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	UFUNCTION()
	//On Click Callback of VLOG button, vlog all traversable nodes of the data table.
	void OnVLogButton();
	UFUNCTION()
	//On Click Callback of VLOG button, vlog all non-traversable nodes of the data table.
	void OnVLogNonButton();
	UFUNCTION()
	//On Click Callback of VLOG Button, vlog all nodes of a specific HPA Index.
	void OnVLogSpecificHPANode();
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FA|VisualizeUtilityWidget", meta = (BindWidget))
	//Select the data table to vlog.
	USinglePropertyView* DataTableSView;
	UPROPERTY(EditAnywhere, Category = "FA|VisualizeUtilityWidget", meta = (RowType="FFaNodeData"))
	//Storing the data table to vlog.
	UDataTable* DT;
	UPROPERTY(EditAnywhere, Category = "FA|VisualizeUtilityWidget", meta = (BindWidget))
	//VLOG Button, Onclick call OnVLogButton.
	UButton* VLogButton;
	UPROPERTY(EditAnywhere, Category = "FA|VisualizeUtilityWidget", meta = (BindWidget))
	//OnClick call OnVLogNonButton.
	UButton* VLogNonTraversableButton;
	UPROPERTY(EditAnywhere, Category = "FA|VisualizeUtilityWidget", meta = (BindWidget))
	//OnClick Call OnVLogSpecificHPANode.
	UButton* VLogSpecificHPANode;
	UPROPERTY(EditAnywhere, Category = "FA|VisualizeUtilityWidget", meta = (BindWidget))
	//Input the HPA Index to vlog.
	USinglePropertyView* InputHPANode;
	UPROPERTY(EditAnywhere, Category = "FA|VisualizeUtilityWidget")
	//Storing the HPA index to vlog.
	uint32 HPANodeIndex;

};
