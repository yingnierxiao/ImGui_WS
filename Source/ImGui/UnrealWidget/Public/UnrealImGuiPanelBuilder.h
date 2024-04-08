// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UnrealImGuiPanelBuilder.generated.h"

struct FStreamableHandle;
class UUnrealImGuiPanelBase;
class UUnrealImGuiLayoutBase;

UCLASS(Config = ImGuiPanelConfig, PerObjectConfig)
class IMGUI_API UUnrealImGuiPanelBuilder : public UObject
{
	GENERATED_BODY()
public:
	UUnrealImGuiPanelBuilder();
	
	// Layout name
	FName DockSpaceName = NAME_None;

	void Register(UObject* Owner);
	void Unregister(UObject* Owner);

	void LoadDefaultLayout(UObject* Owner);
	void DrawPanels(UObject* Owner, float DeltaSeconds);

	void DrawPanelStateMenu(UObject* Owner);
	void DrawLayoutStateMenu(UObject* Owner);

	UUnrealImGuiPanelBase* FindPanel(const TSubclassOf<UUnrealImGuiPanelBase>& PanelType) const;
	template<typename T>
	T* FindPanel() const
	{
		static_assert(TIsDerivedFrom<T, UUnrealImGuiPanelBase>::Value);
		return (T*)FindPanel(T::StaticClass());
	}

	UPROPERTY(Transient)
	TArray<UUnrealImGuiLayoutBase*> Layouts;
	int32 ActiveLayoutIndex = 0;
	UUnrealImGuiLayoutBase* GetActiveLayout() const { return Layouts[ActiveLayoutIndex]; }
	UPROPERTY(Config)
	TSoftClassPtr<UUnrealImGuiLayoutBase> ActiveLayoutClass;

	UPROPERTY(Transient)
	TArray<UUnrealImGuiPanelBase*> Panels;
	struct FCategoryPanels
	{
		FText Category;
		TArray<UUnrealImGuiPanelBase*> Panels;
		TMap<FName, TUniquePtr<FCategoryPanels>> Children;
	};
	FCategoryPanels CategoryPanels;
private:
	TSharedPtr<FStreamableHandle> StreamableHandle;
};
