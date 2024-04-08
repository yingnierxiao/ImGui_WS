﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UnrealImGuiPanel.generated.h"

class UUnrealImGuiLayoutBase;

USTRUCT()
struct FImGuiDefaultPanelState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	bool bOpen{ false };
	UPROPERTY(EditAnywhere)
	bool bEnableDock{ true };
};

USTRUCT()
struct FImGuiDefaultDockLayout : public FImGuiDefaultPanelState
{
	GENERATED_BODY()

	FImGuiDefaultDockLayout() = default;
	FImGuiDefaultDockLayout(int32 DockId)
		: FImGuiDefaultDockLayout(DockId, true)
	{}
	FImGuiDefaultDockLayout(int32 DockId, bool bOpen, bool bEnableDock = true)
		: FImGuiDefaultPanelState{ bOpen, bEnableDock }
		, DockId(DockId)
	{}
	FImGuiDefaultDockLayout(int32 DockId, const FImGuiDefaultPanelState& DefaultPanelState)
		: FImGuiDefaultPanelState{ DefaultPanelState }
		, DockId(DockId)
	{}

	UPROPERTY(EditAnywhere)
	int32 DockId = 0;
};

UCLASS(Abstract, Config = ImGuiPanelConfig, PerObjectConfig, Blueprintable)
class IMGUI_API UUnrealImGuiPanelBase : public UObject
{
	GENERATED_BODY()

public:
	UUnrealImGuiPanelBase();

	UPROPERTY(EditDefaultsOnly, meta = (Bitmask, BitmaskEnum = "/Script/ImGui.EImGuiWindowFlags"), Category = Settings)
	int32 ImGuiWindowFlags;
	UPROPERTY(EditDefaultsOnly, Category = Settings)
	FText Title;
	UPROPERTY(EditDefaultsOnly, Category = Settings)
	TArray<FText> Categories;
	UPROPERTY(EditDefaultsOnly, Category = Settings)
	FImGuiDefaultPanelState DefaultState{ false, true };
	// Key为类型名
	UPROPERTY(EditDefaultsOnly, Category = Settings)
	TMap<FName, FImGuiDefaultDockLayout> DefaultDockSpace;

	bool IsOpen() const { return bIsOpen; }
	void SetOpenState(bool bOpen);
	FString GetLayoutPanelName(const FString& LayoutName) const { return FString::Printf(TEXT("%s##%s_%s"), *Title.ToString(), *GetClass()->GetName(), *LayoutName); }

	friend class UUnrealImGuiPanelBuilder;
	friend class UUnrealImGuiLayoutBase;
	
	UPROPERTY(Transient)
	uint8 bIsOpen : 1;
	UPROPERTY(Config)
	TMap<FName, bool> PanelOpenState;

	virtual bool ShouldCreatePanel(UObject* Owner) const { FEditorScriptExecutionGuard EditorScriptExecutionGuard; return ReceiveShouldCreatePanel(Owner); }

	virtual void Register(UObject* Owner, UUnrealImGuiPanelBuilder* Builder) { FEditorScriptExecutionGuard EditorScriptExecutionGuard; ReveiveRegister(Owner, Builder); }
	virtual void Unregister(UObject* Owner, UUnrealImGuiPanelBuilder* Builder) { FEditorScriptExecutionGuard EditorScriptExecutionGuard; ReveiveUnregister(Owner, Builder); }

	virtual void WhenOpen(UObject* Owner, UUnrealImGuiPanelBuilder* Builder) { FEditorScriptExecutionGuard EditorScriptExecutionGuard; ReveiveWhenOpen(Owner, Builder); }
	virtual void WhenClose(UObject* Owner, UUnrealImGuiPanelBuilder* Builder) { FEditorScriptExecutionGuard EditorScriptExecutionGuard; ReveiveWhenClose(Owner, Builder); }
	
	virtual void Draw(UObject* Owner, UUnrealImGuiPanelBuilder* Builder, float DeltaSeconds) { FEditorScriptExecutionGuard EditorScriptExecutionGuard; ReveiveDraw(Owner, Builder, DeltaSeconds); }
	virtual void DrawWindow(UUnrealImGuiLayoutBase* Layout, UObject* Owner, UUnrealImGuiPanelBuilder* Builder, float DeltaSeconds);

	UFUNCTION(BlueprintNativeEvent)
	bool ReceiveShouldCreatePanel(UObject* Owner) const;
	UFUNCTION(BlueprintImplementableEvent)
	void ReveiveRegister(UObject* Owner, UUnrealImGuiPanelBuilder* Builder);
	UFUNCTION(BlueprintImplementableEvent)
	void ReveiveUnregister(UObject* Owner, UUnrealImGuiPanelBuilder* Builder);
	UFUNCTION(BlueprintImplementableEvent)
	void ReveiveWhenOpen(UObject* Owner, UUnrealImGuiPanelBuilder* Builder);
	UFUNCTION(BlueprintImplementableEvent)
	void ReveiveWhenClose(UObject* Owner, UUnrealImGuiPanelBuilder* Builder);
	UFUNCTION(BlueprintImplementableEvent)
	void ReveiveDraw(UObject* Owner, UUnrealImGuiPanelBuilder* Builder, float DeltaSeconds);
};