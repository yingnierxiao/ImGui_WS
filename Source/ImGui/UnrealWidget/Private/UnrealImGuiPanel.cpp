﻿// Fill out your copyright notice in the Description page of Project Settings.


#include "UnrealImGuiPanel.h"

#include "imgui.h"
#include "UnrealImGuiLayout.h"

UUnrealImGuiPanelBase::UUnrealImGuiPanelBase()
	: bIsOpen(false)
{
	
}

void UUnrealImGuiPanelBase::DrawWindow(UUnrealImGuiLayoutBase* Layout, UObject* Owner, float DeltaSeconds)
{
	if (bIsOpen == false)
	{
		return;
	}
	
	bool IsOpen = bIsOpen;
	const FString WindowName = GetLayoutPanelName(Layout->LayoutName.ToString());
	if (ImGui::Begin(TCHAR_TO_UTF8(*WindowName), &IsOpen, ImGuiWindowFlags))
	{
		Draw(Owner, DeltaSeconds);
		ImGui::End();
	}
	if (bIsOpen != IsOpen)
	{
		bIsOpen = IsOpen;
		PanelOpenState.Add(Layout->GetClass()->GetFName(), IsOpen);
		SaveConfig();
	}
}