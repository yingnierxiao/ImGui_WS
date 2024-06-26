// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealImGuiPanel.h"
#include "UnrealImGuiStatPanel.generated.h"


UCLASS()
class IMGUI_UNREALPANELS_API UUnrealImGuiStatPanel : public UUnrealImGuiPanelBase
{
	GENERATED_BODY()
public:
	UUnrealImGuiStatPanel();

	void Draw(UObject* Owner, UUnrealImGuiPanelBuilder* Builder, float DeltaSeconds) override;
};
