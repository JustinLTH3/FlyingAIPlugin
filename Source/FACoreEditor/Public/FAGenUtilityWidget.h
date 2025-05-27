// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityWidget.h"
#include "FALevelData.h"
#include "Misc/Paths.h"
#include "FAGenUtilityWidget.generated.h"

class UDetailsView;
/**
 * 
 */
UCLASS()
class FACOREEDITOR_API UFAGenUtilityWidget : public UEditorUtilityWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	UPROPERTY(EditAnywhere, Category = "FA|GenUtilityWidget", meta = (BindWidget))
	class UButton* GenerateButton;
	UPROPERTY(EditAnywhere, Category = "FA|GenUtilityWidget", meta = (BindWidget))
	/** Input the directory of where the generated data are stored*/
	class USinglePropertyView* Path;
	UPROPERTY(EditAnywhere, Category = "FA|GenUtilityWidget", meta = (BindWidget))
	//Input the max depth of generation.
	USinglePropertyView* MaxDepthView;
	UPROPERTY(EditAnywhere, Category = "FA|GenUtilityWidget", BlueprintReadWrite, meta = (BindWidget))
	USinglePropertyView* BoundDataSView;

protected:
	UPROPERTY(EditAnywhere, Category = "FA|GenUtilityWidget")
	FDirectoryPath DirectoryPath;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FA|GenUtilityWidget")
	TSoftObjectPtr<class AFABound> SelectedBound;
	UPROPERTY(EditAnywhere, Category = "FA|GenUtilityWidget")
	uint32 MaxDepth = 4;
	UFUNCTION()
	void GenerateButtonClicked();
};
