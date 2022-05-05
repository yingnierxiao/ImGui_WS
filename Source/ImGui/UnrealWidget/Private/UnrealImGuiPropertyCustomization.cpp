// Fill out your copyright notice in the Description page of Project Settings.


#include "UnrealImGuiPropertyCustomization.h"
#include <AssetRegistry/AssetRegistryModule.h>
#include <AssetRegistry/IAssetRegistry.h>
#include <EngineUtils.h>

#include "imgui.h"
#include "imgui_internal.h"

namespace UnrealImGui
{
	FPropertyDisableScope::FPropertyDisableScope(bool IsDisable)
		: Disable(GlobalValue::GEnableEditVisibleProperty == false && IsDisable)
	{
		if (Disable)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
	}

	FPropertyDisableScope::~FPropertyDisableScope()
	{
		if (Disable)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
	}

	template<typename TNameCustomizer, typename TValueExtend>
	void AddUnrealContainerPropertyInner(const FProperty* Property, const FStructArray& Containers, int32 Offset, const TNameCustomizer& NameCustomizer, const TValueExtend& ValueExtend)
	{
		const TSharedPtr<IUnrealPropertyCustomization> PropertyCustomization = UnrealPropertyCustomizeFactory::FindCustomizer(Property);
		if (!PropertyCustomization)
		{
			return;
		}

		bool IsShowChildren = false;
		const bool IsIdentical = IsAllPropertiesIdentical(Property, Containers, Offset);
		TSharedPtr<IUnrealStructCustomization> Customization = nullptr;
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			Customization = UnrealPropertyCustomizeFactory::FindCustomizer(StructProperty->Struct);
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		NameCustomizer(Property, Containers, Offset, IsIdentical, IsShowChildren);
		ImGui::TableSetColumnIndex(1);
		ImGui::SetNextItemWidth(InnerValue::ContainerValueRightWidth - (Customization ? Customization->ValueAdditiveRightWidth : 0.f));
		if (Customization)
		{
			Customization->CreateValueWidget(Property, Containers, Offset, IsIdentical);
		}
		else
		{
			PropertyCustomization->CreateValueWidget(Property, Containers, Offset, IsIdentical);
		}
		ValueExtend(Property, Containers, Offset, IsIdentical);

		if (IsShowChildren)
		{
			if (Customization)
			{
				Customization->CreateChildrenWidget(Property, Containers, Offset, IsIdentical);
			}
			else
			{
				PropertyCustomization->CreateChildrenWidget(Property, Containers, Offset, IsIdentical);
			}
			ImGui::TreePop();
		}

		ImGui::NextColumn();
	}

	void FBoolPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		uint8* FirstValuePtr = Containers[0] + Offset;
		const FBoolProperty* BoolProperty = CastFieldChecked<FBoolProperty>(Property);
		bool FirstValue = BoolProperty->GetPropertyValue(FirstValuePtr);
		if (ImGui::Checkbox(TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical)), &FirstValue))
		{
			for (uint8* Container : Containers)
			{
				BoolProperty->SetPropertyValue(Container + Offset, FirstValue);
			}
			NotifyPostPropertyValueChanged(Property);
		}
	}

	void FNumericPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FNumericProperty* NumericProperty = CastFieldChecked<FNumericProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		const auto PropertyLabelName = TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical));
		if (UEnum* EnumDef = NumericProperty->GetIntPropertyEnum())
		{
			const int64 EnumValue = NumericProperty->GetSignedIntPropertyValue(FirstValuePtr);

			FString EnumName = EnumDef->GetNameByValue(EnumValue).ToString();
			EnumName.Split(TEXT("::"), nullptr, &EnumName);
			if (ImGui::BeginCombo(PropertyLabelName, TCHAR_TO_UTF8(*EnumName), ImGuiComboFlags_PopupAlignLeft))
			{
				for (int32 Idx = 0; Idx < EnumDef->NumEnums() - 1; ++Idx)
				{
					const int64 CurrentEnumValue = EnumDef->GetValueByIndex(Idx);
					const bool IsSelected = CurrentEnumValue == EnumValue;
					EnumName = EnumDef->GetNameByIndex(Idx).ToString();
					EnumName.Split(TEXT("::"), nullptr, &EnumName);
					if (ImGui::Selectable(TCHAR_TO_UTF8(*EnumName), IsSelected))
					{
						for (uint8* Container : Containers)
						{
							NumericProperty->SetIntPropertyValue(Container + Offset, CurrentEnumValue);
						}
						NotifyPostPropertyValueChanged(Property);
					}
					if (IsSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
		else if (NumericProperty->IsInteger())
		{
			int64 Number = NumericProperty->GetSignedIntPropertyValue(FirstValuePtr);
			ImGui::InputScalar(PropertyLabelName, ImGuiDataType_S64, &Number);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				for (uint8* Container : Containers)
				{
					NumericProperty->SetIntPropertyValue(Container + Offset, Number);
				}
				NotifyPostPropertyValueChanged(Property);
			}
		}
		else if (NumericProperty->IsFloatingPoint())
		{
			float Number = static_cast<float>(NumericProperty->GetFloatingPointPropertyValue(FirstValuePtr));
			ImGui::InputScalar(PropertyLabelName, ImGuiDataType_Float, &Number);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				for (uint8* Container : Containers)
				{
					NumericProperty->SetFloatingPointPropertyValue(Container + Offset, Number);
				}
				NotifyPostPropertyValueChanged(Property);
			}
		}
	}

	bool FObjectPropertyCustomization::HasChildPropertiesOverride(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		const FObjectProperty* ObjectProperty = CastFieldChecked<FObjectProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		return ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference) && ObjectProperty->GetObjectPropertyValue(FirstValuePtr) != nullptr;
	}

	void FObjectPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };

		const FObjectProperty* ObjectProperty = CastFieldChecked<FObjectProperty>(Property);
		if (ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference))
		{
			const UObject* FirstValue = ObjectProperty->GetPropertyValue(Containers[0] + Offset);

			if (ImGui::BeginCombo(
				TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical)),
				TCHAR_TO_UTF8(FirstValue ? *FirstValue->GetName() : TEXT("Null"))))
			{
				if (ImGui::Selectable("Clear"))
				{
					for (uint8* Container : Containers)
					{
						ObjectProperty->SetPropertyValue(Container + Offset, nullptr);
					}
					NotifyPostPropertyValueChanged(Property);
				}
				ImGui::Separator();

				if (CachedInstancedClass != ObjectProperty->PropertyClass)
				{
					CachedInstancedClass = ObjectProperty->PropertyClass;
					CachedClassList.Reset();
					TArray<UClass*> DerivedClasses;
					GetDerivedClasses(ObjectProperty->PropertyClass, DerivedClasses);
					DerivedClasses.Add(ObjectProperty->PropertyClass);
					DerivedClasses.Sort([](const UClass& LHS, const UClass& RHS)
					{
						return LHS.GetFName().FastLess(RHS.GetFName());
					});
					for (UClass* DerivedClass : DerivedClasses)
					{
						if (DerivedClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
						{
							continue;
						}
						if (DerivedClass->HasAllClassFlags(CLASS_EditInlineNew))
						{
							CachedClassList.Add(DerivedClass);
						}
					}
				}
				const UClass* FirstValueClass = FirstValue ? FirstValue->GetClass() : nullptr;
				for (const TWeakObjectPtr<UClass>& ClassPtr : CachedClassList)
				{
					if (UClass* Class = ClassPtr.Get())
					{
						const bool IsSelected = Class == FirstValueClass;
						if (ImGui::Selectable(TCHAR_TO_UTF8(*Class->GetName()), IsSelected))
						{
							for (int32 Idx = 0; Idx < Containers.Num(); ++Idx)
							{
								uint8* Container = Containers[Idx];
								UObject* Outer = InnerValue::GOuters[Idx];
								check(Outer);
								ObjectProperty->SetPropertyValue(Container + Offset, NewObject<UObject>(Outer, Class));
							}
							NotifyPostPropertyValueChanged(Property);
						}
						if (IsSelected)
						{
							ImGui::SetItemDefaultFocus();
						}

						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::Text(TCHAR_TO_UTF8(*Class->GetFullName()));
							ImGui::EndTooltip();
						}
					}
				}
				ImGui::EndCombo();
			}
		}
		else
		{
			uint8* FirstValuePtr = Containers[0] + Offset;
			FName ObjectName = NAME_None;
			const UObject* FirstValue = IsIdentical ? ObjectProperty->GetPropertyValue(FirstValuePtr) : nullptr;
			if (IsIdentical)
			{
				if (IsValid(FirstValue))
				{
					ObjectName = FirstValue->GetFName();
				}
				else
				{
					static FName NullName = TEXT("Null");
					ObjectName = NullName;
				}
			}
			else
			{
				static FName MultiValueName = TEXT("Multi Values");
				ObjectName = MultiValueName;
			}
			UClass* ObjectClass = ObjectProperty->PropertyClass;

			if (ImGui::BeginCombo(
				TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical)), TCHAR_TO_UTF8(*ObjectName.ToString())))
			{
				if (ImGui::Selectable("Clear"))
				{
					for (uint8* Container : Containers)
					{
						ObjectProperty->SetPropertyValue(Container + Offset, nullptr);
					}
					NotifyPostPropertyValueChanged(Property);
				}
				ImGui::Separator();
				if (CachedAssetClass != ObjectClass)
				{
					CachedAssetClass = ObjectClass;
					CachedAssetList.Reset();
					static IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
					FARFilter ARFilter;
					ARFilter.bRecursivePaths = true;
					ARFilter.bRecursiveClasses = true;
					ARFilter.ClassNames.Add(ObjectClass->GetFName());
					AssetRegistry.GetAssets(ARFilter, CachedAssetList);
					CachedAssetList.Sort([](const FAssetData& LHS, const FAssetData& RHS)
					{
						return LHS.AssetName.FastLess(RHS.AssetName);
					});
				}
				for (const FAssetData& Asset : CachedAssetList)
				{
					const bool IsSelected = Asset.AssetName == ObjectName;
					if (ImGui::Selectable(TCHAR_TO_UTF8(*Asset.AssetName.ToString()), IsSelected))
					{
						for (uint8* Container : Containers)
						{
							ObjectProperty->SetPropertyValue(Container + Offset, Asset.GetAsset());
						}
						NotifyPostPropertyValueChanged(Property);
					}
					if (IsSelected)
					{
						ImGui::SetItemDefaultFocus();
					}

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text(TCHAR_TO_UTF8(*Asset.ObjectPath.ToString()));
						ImGui::EndTooltip();
					}
				}
				ImGui::EndCombo();
			}
			if (FirstValue)
			{
				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text(TCHAR_TO_UTF8(*FirstValue->GetPathName()));
					ImGui::EndTooltip();
				}
			}
		}
	}

	void FObjectPropertyCustomization::CreateChildrenWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		const FObjectProperty* ObjectProperty = CastFieldChecked<FObjectProperty>(Property);
		check(ObjectProperty->HasAnyPropertyFlags(CPF_InstancedReference));

		if (ObjectProperty->GetObjectPropertyValue(Containers[0] + Offset) == nullptr)
		{
			return;
		}

		TGuardValue<int32> DepthGuard(InnerValue::GPropertyDepth, InnerValue::GPropertyDepth + 1);

		FObjectArray Instances;
		for (uint8* Container : Containers)
		{
			Instances.Add(ObjectProperty->GetObjectPropertyValue(Container + Offset));
		}
		const UClass* TopClass = GetTopClass(Instances);
		TGuardValue<FObjectArray> GOutersGuard{InnerValue::GOuters, Instances};

		const TSharedPtr<IUnrealClassCustomization> Customizer = UnrealPropertyCustomizeFactory::FindCustomizer(TopClass);
		if (Customizer)
		{
			Customizer->CreateClassDetails(TopClass, Instances, 0);
		}
		else
		{
			DrawDefaultClassDetails(TopClass, true, Instances, 0);
		}
	}

	void FSoftObjectPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FSoftObjectProperty* SoftObjectProperty = CastFieldChecked<FSoftObjectProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		const FSoftObjectPtr& FirstValue = *SoftObjectProperty->GetPropertyValuePtr(FirstValuePtr);

		const FString ObjectName = [&]()-> FString
		{
			if (IsIdentical)
			{
				if (FirstValue.IsNull())
				{
					return TEXT("Null");
				}
				else if (UObject* Object = FirstValue.Get())
				{
					return Object->GetName();
				}
				else
				{
					return FirstValue.GetAssetName();
				}
			}
			return TEXT("Multi Values");
		}();
		UClass* ObjectClass = SoftObjectProperty->PropertyClass;

		if (ObjectClass->IsChildOf(AActor::StaticClass()))
		{
			if (ImGui::BeginCombo(
				TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical)), TCHAR_TO_UTF8(*ObjectName)))
			{
				if (ImGui::Selectable("Clear"))
				{
					for (uint8* Container : Containers)
					{
						SoftObjectProperty->SetObjectPropertyValue(Container + Offset, nullptr);
					}
					NotifyPostPropertyValueChanged(Property);
				}
				ImGui::Separator();

				if (CachedActorClass != SoftObjectProperty->PropertyClass)
				{
					CachedActorClass = SoftObjectProperty->PropertyClass;
					CachedActorList.Reset();
					const UObject* Outer = InnerValue::GOuters[0];
					for (TActorIterator<AActor> It(Outer ? Outer->GetWorld() : GWorld, SoftObjectProperty->PropertyClass); It; ++It)
					{
						CachedActorList.Add(*It);
					}
				}
				for (const TWeakObjectPtr<AActor>& ActorPtr : CachedActorList)
				{
					if (AActor* Actor = ActorPtr.Get())
					{
						const bool IsSelected = FirstValue.Get() == Actor;
						if (ImGui::Selectable(TCHAR_TO_UTF8(*Actor->GetName()), IsSelected))
						{
							const FSoftObjectPtr NewValue{Actor};
							for (uint8* Container : Containers)
							{
								SoftObjectProperty->SetPropertyValue(Container + Offset, NewValue);
							}
							NotifyPostPropertyValueChanged(Property);
						}
						if (IsSelected)
						{
							ImGui::SetItemDefaultFocus();
						}

						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::Text(TCHAR_TO_UTF8(*Actor->GetFullName()));
							ImGui::EndTooltip();
						}
					}
				}

				ImGui::EndCombo();
			}
		}
		else
		{
			if (ImGui::BeginCombo(
				TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical)), TCHAR_TO_UTF8(*ObjectName)))
			{
				if (ImGui::Selectable("Clear"))
				{
					for (uint8* Container : Containers)
					{
						SoftObjectProperty->SetObjectPropertyValue(Container + Offset, nullptr);
					}
					NotifyPostPropertyValueChanged(Property);
				}
				ImGui::Separator();

				if (CachedAssetClass != ObjectClass)
				{
					CachedAssetClass = ObjectClass;
					CachedAssetList.Reset();
					static IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
					FARFilter ARFilter;
					ARFilter.bRecursivePaths = true;
					ARFilter.bRecursiveClasses = true;
					ARFilter.ClassNames.Add(ObjectClass->GetFName());
					AssetRegistry.GetAssets(ARFilter, CachedAssetList);
					CachedAssetList.Sort([](const FAssetData& LHS, const FAssetData& RHS)
					{
						return LHS.AssetName.FastLess(RHS.AssetName);
					});
				}
				for (const FAssetData& Asset : CachedAssetList)
				{
					const bool IsSelected = FirstValue.GetUniqueID().GetAssetPathName() == Asset.ObjectPath;
					if (ImGui::Selectable(TCHAR_TO_UTF8(*Asset.AssetName.ToString()), IsSelected))
					{
						const FSoftObjectPtr NewValue{Asset.ToSoftObjectPath()};
						for (uint8* Container : Containers)
						{
							SoftObjectProperty->SetPropertyValue(Container + Offset, NewValue);
						}
						NotifyPostPropertyValueChanged(Property);
					}
					if (IsSelected)
					{
						ImGui::SetItemDefaultFocus();
					}

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text(TCHAR_TO_UTF8(*Asset.ObjectPath.ToString()));
						ImGui::EndTooltip();
					}
				}
				ImGui::EndCombo();
			}
		}
		if (IsIdentical && FirstValue.IsNull() == false)
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text(TCHAR_TO_UTF8(*FirstValue.ToString()));
				ImGui::EndTooltip();
			}
		}
	}

	void FClassPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FClassProperty* ClassProperty = CastFieldChecked<FClassProperty>(Property);
		const UObject* FirstValue = ClassProperty->GetPropertyValue(Containers[0] + Offset);

		if (ImGui::BeginCombo(
			TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical)),
			TCHAR_TO_UTF8(FirstValue ? *FirstValue->GetName() : TEXT("Null"))))
		{
			if (ImGui::Selectable("Clear"))
			{
				for (uint8* Container : Containers)
				{
					ClassProperty->SetPropertyValue(Container + Offset, nullptr);
				}
				NotifyPostPropertyValueChanged(Property);
			}
			ImGui::Separator();

			if (CachedInstancedClass != ClassProperty->MetaClass)
			{
				CachedInstancedClass = ClassProperty->MetaClass;
				CachedClassList.Reset();
				TArray<UClass*> DerivedClasses;
				GetDerivedClasses(ClassProperty->MetaClass, DerivedClasses);
				DerivedClasses.Add(ClassProperty->MetaClass);
				DerivedClasses.Sort([](const UClass& LHS, const UClass& RHS)
				{
					return LHS.GetFName().FastLess(RHS.GetFName());
				});
				for (UClass* DerivedClass : DerivedClasses)
				{
					if (DerivedClass->HasAnyClassFlags(CLASS_Deprecated))
					{
						continue;
					}
					CachedClassList.Add(DerivedClass);
				}
			}
			for (const TWeakObjectPtr<UClass>& ClassPtr : CachedClassList)
			{
				if (UClass* Class = ClassPtr.Get())
				{
					const bool IsSelected = Class == ClassProperty->MetaClass;
					if (ImGui::Selectable(TCHAR_TO_UTF8(*Class->GetName()), IsSelected))
					{
						for (uint8* Container : Containers)
						{
							ClassProperty->SetPropertyValue(Container + Offset, Class);
						}
						NotifyPostPropertyValueChanged(Property);
					}
					if (IsSelected)
					{
						ImGui::SetItemDefaultFocus();
					}

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text(TCHAR_TO_UTF8(*Class->GetFullName()));
						ImGui::EndTooltip();
					}
				}
			}
			ImGui::EndCombo();
		}
		if (FirstValue)
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text(TCHAR_TO_UTF8(*FirstValue->GetPathName()));
				ImGui::EndTooltip();
			}
		}
	}

	void FSoftClassPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FSoftClassProperty* SoftClassProperty = CastFieldChecked<FSoftClassProperty>(Property);

		const FSoftObjectPtr FirstValue = SoftClassProperty->GetPropertyValue(Containers[0] + Offset);
		if (ImGui::BeginCombo(
			TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical)),
			TCHAR_TO_UTF8(FirstValue.IsNull() ? TEXT("Null") : *FirstValue.GetUniqueID().GetAssetName())))
		{
			if (ImGui::Selectable("Clear"))
			{
				for (uint8* Container : Containers)
				{
					SoftClassProperty->SetPropertyValue(Container + Offset, FSoftObjectPtr());
				}
				NotifyPostPropertyValueChanged(Property);
			}
			ImGui::Separator();

			if (CachedInstancedClass != SoftClassProperty->MetaClass)
			{
				CachedInstancedClass = SoftClassProperty->MetaClass;
				CachedClassList.Reset();
				TArray<UClass*> DerivedClasses;
				GetDerivedClasses(SoftClassProperty->MetaClass, DerivedClasses);
				DerivedClasses.Add(SoftClassProperty->MetaClass);
				DerivedClasses.Sort([](const UClass& LHS, const UClass& RHS)
				{
					return LHS.GetFName().FastLess(RHS.GetFName());
				});
				for (UClass* DerivedClass : DerivedClasses)
				{
					if (DerivedClass->HasAnyClassFlags(CLASS_Deprecated))
					{
						continue;
					}
					CachedClassList.Add(DerivedClass);
				}
			}
			for (const TWeakObjectPtr<UClass>& ClassPtr : CachedClassList)
			{
				if (UClass* Class = ClassPtr.Get())
				{
					const bool IsSelected = Class == SoftClassProperty->MetaClass;
					if (ImGui::Selectable(TCHAR_TO_UTF8(*Class->GetName()), IsSelected))
					{
						const FSoftObjectPtr NewValue{Class};
						for (uint8* Container : Containers)
						{
							SoftClassProperty->SetPropertyValue(Container + Offset, NewValue);
						}
						NotifyPostPropertyValueChanged(Property);
					}
					if (IsSelected)
					{
						ImGui::SetItemDefaultFocus();
					}

					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text(TCHAR_TO_UTF8(*Class->GetFullName()));
						ImGui::EndTooltip();
					}
				}
			}
			ImGui::EndCombo();
		}
		if (IsIdentical && FirstValue.IsNull() == false)
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text(TCHAR_TO_UTF8(*FirstValue.ToString()));
				ImGui::EndTooltip();
			}
		}
	}

	void FStringPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FStrProperty* StringProperty = CastFieldChecked<FStrProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		const FString FirstValue = StringProperty->GetPropertyValue(FirstValuePtr);
		const auto StringPoint = FTCHARToUTF8(*FirstValue);
		char Buff[512];
		FMemory::Memcpy(&Buff, StringPoint.Get(), StringPoint.Length() + 1);
		ImGui::InputText(TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical)), Buff, sizeof(Buff));
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			const FString ChangedValue = UTF8_TO_TCHAR(Buff);
			for (uint8* Container : Containers)
			{
				StringProperty->SetPropertyValue(Container + Offset, ChangedValue);
			}
			NotifyPostPropertyValueChanged(Property);
		}
	}

	void FNamePropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FNameProperty* NameProperty = CastFieldChecked<FNameProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		const auto PropertyLabelName = TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical));
		const FName FirstValue = NameProperty->GetPropertyValue(FirstValuePtr);
		const auto StringPoint = FTCHARToUTF8(*FirstValue.ToString());
		char Buff[512];
		FMemory::Memcpy(&Buff, StringPoint.Get(), StringPoint.Length() + 1);
		ImGui::InputText(PropertyLabelName, Buff, sizeof(Buff));
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			const FName ChangedValue = UTF8_TO_TCHAR(Buff);
			for (uint8* Container : Containers)
			{
				NameProperty->SetPropertyValue(Container + Offset, ChangedValue);
			}
			NotifyPostPropertyValueChanged(Property);
		}
	}

	void FTextPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FTextProperty* TextProperty = CastFieldChecked<FTextProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		const FText FirstValue = TextProperty->GetPropertyValue(FirstValuePtr);
		const auto StringPoint = FTCHARToUTF8(*FirstValue.ToString());
		char Buff[512];
		FMemory::Memcpy(&Buff, StringPoint.Get(), StringPoint.Length() + 1);
		ImGui::InputText(TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical)), Buff, sizeof(Buff));
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			const FText ChangedValue = FText::FromString(UTF8_TO_TCHAR(Buff));
			for (uint8* Container : Containers)
			{
				TextProperty->SetPropertyValue(Container + Offset, ChangedValue);
			}
			NotifyPostPropertyValueChanged(Property);
		}
	}

	void FEnumPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FEnumProperty* EnumProperty = CastFieldChecked<FEnumProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		UEnum* EnumDef = EnumProperty->GetEnum();
		FNumericProperty* UnderlyingProperty = EnumProperty->GetUnderlyingProperty();
		const int64 EnumValue = UnderlyingProperty->GetSignedIntPropertyValue(FirstValuePtr);

		FString EnumName = EnumDef->GetNameByValue(EnumValue).ToString();
		EnumName.Split(TEXT("::"), nullptr, &EnumName);
		if (ImGui::BeginCombo(TCHAR_TO_UTF8(*GetPropertyDefaultLabel(Property, IsIdentical)), TCHAR_TO_UTF8(*EnumName), ImGuiComboFlags_PopupAlignLeft))
		{
			for (int32 Idx = 0; Idx < EnumDef->NumEnums() - 1; ++Idx)
			{
				const int64 CurrentEnumValue = EnumDef->GetValueByIndex(Idx);
				const bool IsSelected = CurrentEnumValue == EnumValue;
				EnumName = EnumDef->GetNameByIndex(Idx).ToString();
				EnumName.Split(TEXT("::"), nullptr, &EnumName);
				if (ImGui::Selectable(TCHAR_TO_UTF8(*EnumName), IsSelected))
				{
					for (uint8* Container : Containers)
					{
						UnderlyingProperty->SetIntPropertyValue(Container + Offset, CurrentEnumValue);
					}
					NotifyPostPropertyValueChanged(Property);
				}
				if (IsSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}

	bool FArrayPropertyCustomization::HasChildPropertiesOverride(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		const int32 FirstCount = FScriptArrayHelper(ArrayProperty, FirstValuePtr).Num();
		if (FirstCount == 0)
		{
			return false;
		}
		if (IsIdentical)
		{
			return true;
		}
		for (int32 Idx = 1; Idx < Containers.Num(); ++Idx)
		{
			if (FScriptArrayHelper(ArrayProperty, Containers[Idx] + Offset).Num() != FirstCount)
			{
				return false;
			}
		}
		return true;
	}

	void FArrayPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		if (IsIdentical)
		{
			const int32 ArrayCount = FScriptArrayHelper(ArrayProperty, FirstValuePtr).Num();
			ImGui::Text("%d Elements", ArrayCount);
			ImGui::SameLine();
			if (ImGui::Button(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("+")))))
			{
				for (uint8* Container : Containers)
				{
					FScriptArrayHelper(ArrayProperty, Container + Offset).AddValue();
				}
				NotifyPostPropertyValueChanged(Property);
			}
			ImGui::SameLine();
			if (ImGui::Button(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("x")))))
			{
				for (uint8* Container : Containers)
				{
					FScriptArrayHelper(ArrayProperty, Container + Offset).EmptyValues();
				}
				NotifyPostPropertyValueChanged(Property);
			}
		}
		else
		{
			ImGui::Text("Different Elements *");
			ImGui::SameLine();
			if (ImGui::Button(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("x")))))
			{
				for (uint8* Container : Containers)
				{
					FScriptArrayHelper(ArrayProperty, Container + Offset).EmptyValues();
				}
				NotifyPostPropertyValueChanged(Property);
			}
		}
	}

	void FArrayPropertyCustomization::CreateChildrenWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };

		const FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(Property);
		const TSharedPtr<IUnrealPropertyCustomization> PropertyCustomization = UnrealPropertyCustomizeFactory::FindCustomizer(ArrayProperty->Inner);
		if (!PropertyCustomization)
		{
			return;
		}

		TArray<FScriptArrayHelper> Helpers;
		for (uint8* Container : Containers)
		{
			Helpers.Emplace(FScriptArrayHelper(ArrayProperty, Container + Offset));
		}
		FStructArray ArrayRawPtr;
		ArrayRawPtr.SetNum(Containers.Num());
		for (int32 ElemIdx = 0; ElemIdx < Helpers[0].Num(); ++ElemIdx)
		{
			for (int32 Idx = 0; Idx < Containers.Num(); ++Idx)
			{
				ArrayRawPtr[Idx] = Helpers[Idx].GetRawPtr(ElemIdx);
			}
			TGuardValue<int32> GImGuiContainerIndexGuard(InnerValue::GImGuiContainerIndex, ElemIdx);

			AddUnrealContainerPropertyInner(ArrayProperty->Inner, ArrayRawPtr, 0,
				[ElemIdx, &PropertyCustomization](const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical, bool& IsShowChildren)
			{
				const FString ElementName = FString::Printf(TEXT("%d##%s%d"), ElemIdx, *Property->GetName(), InnerValue::GPropertyDepth);
				CreateUnrealPropertyNameWidget(Property, Containers, Offset, IsIdentical, PropertyCustomization->HasChildProperties(Property, Containers, Offset, IsIdentical), IsShowChildren, &ElementName);
			}, [ElemIdx, &Helpers](const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical)
			{
				ImGui::SameLine();

				const auto ArrayPopupID = TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("unreal_array_popup")));
				if (ImGui::ArrowButton(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT(">"))), ImGuiDir_Down))
				{
					ImGui::OpenPopup(ArrayPopupID);
				}

				ImGui::SameLine();
				if (ImGui::BeginPopup(ArrayPopupID))
				{
					if (ImGui::Selectable("Insert"))
					{
						for (FScriptArrayHelper& Helper : Helpers)
						{
							Helper.InsertValues(ElemIdx);
						}
						NotifyPostPropertyValueChanged(Property);
					}
					if (ImGui::Selectable("Delete"))
					{
						for (FScriptArrayHelper& Helper : Helpers)
						{
							Helper.RemoveValues(ElemIdx);
						}
						NotifyPostPropertyValueChanged(Property);
					}
					ImGui::EndPopup();
				}
			});
		}
	}

	bool FSetPropertyCustomization::HasChildPropertiesOverride(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		const FSetProperty* SetProperty = CastFieldChecked<FSetProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		const int32 FirstCount = FScriptSetHelper(SetProperty, FirstValuePtr).Num();
		if (FirstCount == 0)
		{
			return false;
		}
		if (IsIdentical)
		{
			return true;
		}
		for (int32 Idx = 1; Idx < Containers.Num(); ++Idx)
		{
			if (FScriptSetHelper(SetProperty, Containers[Idx] + Offset).Num() != FirstCount)
			{
				return false;
			}
		}
		return true;
	}

	void FSetPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FSetProperty* SetProperty = CastFieldChecked<FSetProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		if (IsIdentical)
		{
			const int32 SetCount = FScriptSetHelper(SetProperty, FirstValuePtr).Num();
			ImGui::Text("%d Elements", SetCount);
			ImGui::SameLine();
			if (ImGui::Button(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("+")))))
			{
				TArray<uint8> AddElem;
				AddElem.SetNumUninitialized(SetProperty->ElementProp->ElementSize);
				SetProperty->ElementProp->InitializeValue(AddElem.GetData());
				for (uint8* Container : Containers)
				{
					FScriptSetHelper(SetProperty, Container + Offset).AddElement(AddElem.GetData());
				}
				NotifyPostPropertyValueChanged(Property);
			}
			ImGui::SameLine();
			if (ImGui::Button(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("x")))))
			{
				for (uint8* Container : Containers)
				{
					FScriptSetHelper(SetProperty, Container + Offset).EmptyElements();
				}
				NotifyPostPropertyValueChanged(Property);
			}
		}
		else
		{
			ImGui::Text("Different Elements *");
			ImGui::SameLine();
			if (ImGui::Button(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("x")))))
			{
				for (uint8* Container : Containers)
				{
					FScriptSetHelper(SetProperty, Container + Offset).EmptyElements();
				}
				NotifyPostPropertyValueChanged(Property);
			}
		}
	}

	void FSetPropertyCustomization::CreateChildrenWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };

		const FSetProperty* SetProperty = CastFieldChecked<FSetProperty>(Property);
		const TSharedPtr<IUnrealPropertyCustomization> PropertyCustomization = UnrealPropertyCustomizeFactory::FindCustomizer(SetProperty->ElementProp);
		if (!PropertyCustomization)
		{
			return;
		}

		TArray<FScriptSetHelper> Helpers;
		for (uint8* Container : Containers)
		{
			Helpers.Emplace(FScriptSetHelper(SetProperty, Container + Offset));
		}
		FStructArray SetRawPtr;
		SetRawPtr.SetNum(Containers.Num());
		for (int32 ElemIdx = 0; ElemIdx < Helpers[0].Num(); ++ElemIdx)
		{
			for (int32 Idx = 0; Idx < Containers.Num(); ++Idx)
			{
				SetRawPtr[Idx] = Helpers[Idx].GetElementPtr(ElemIdx);
			}
			TGuardValue<int32> GImGuiContainerIndexGuard(InnerValue::GImGuiContainerIndex, ElemIdx);

			AddUnrealContainerPropertyInner(SetProperty->ElementProp, SetRawPtr, 0,
				[ElemIdx, &PropertyCustomization](const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical, bool& IsShowChildren)
			{
				const FString ElementName = FString::Printf(TEXT("%d##%s%d"), ElemIdx, *Property->GetName(), InnerValue::GPropertyDepth);
				CreateUnrealPropertyNameWidget(Property, Containers, Offset, IsIdentical, PropertyCustomization->HasChildProperties(Property, Containers, Offset, IsIdentical), IsShowChildren, &ElementName);
			}, [ElemIdx, &Helpers](const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical)
			{
				ImGui::SameLine();

				const auto SetPopupID = TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("unreal_set_popup")));
				if (ImGui::ArrowButton(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT(">"))), ImGuiDir_Down))
				{
					ImGui::OpenPopup(SetPopupID);
				}
				ImGui::SameLine();
				if (ImGui::BeginPopup(SetPopupID))
				{
					if (ImGui::MenuItem("Delete"))
					{
						for (FScriptSetHelper& Helper : Helpers)
						{
							Helper.RemoveAt(ElemIdx);
						}
						NotifyPostPropertyValueChanged(Property);
					}
					ImGui::EndPopup();
				}
			});
		}
	}

	bool FMapPropertyCustomization::HasChildPropertiesOverride(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		const FMapProperty* MapProperty = CastFieldChecked<FMapProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		const int32 FirstCount = FScriptMapHelper(MapProperty, FirstValuePtr).Num();
		if (FirstCount == 0)
		{
			return false;
		}
		if (IsIdentical)
		{
			return true;
		}
		for (int32 Idx = 1; Idx < Containers.Num(); ++Idx)
		{
			if (FScriptMapHelper(MapProperty, Containers[Idx] + Offset).Num() != FirstCount)
			{
				return false;
			}
		}
		return true;
	}

	void FMapPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FMapProperty* MapProperty = CastFieldChecked<FMapProperty>(Property);
		uint8* FirstValuePtr = Containers[0] + Offset;
		if (IsIdentical)
		{
			const int32 MapCount = FScriptMapHelper(MapProperty, FirstValuePtr).Num();
			ImGui::Text("%d Elements", MapCount);
			ImGui::SameLine();
			if (ImGui::Button(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("+")))))
			{
				TArray<uint8> KeyElem;
				KeyElem.SetNumUninitialized(MapProperty->KeyProp->ElementSize);
				MapProperty->KeyProp->InitializeValue(KeyElem.GetData());
				TArray<uint8> ValueElem;
				ValueElem.SetNumUninitialized(MapProperty->ValueProp->ElementSize);
				MapProperty->ValueProp->InitializeValue(ValueElem.GetData());
				for (uint8* Container : Containers)
				{
					if (FScriptMapHelper(MapProperty, Container + Offset).FindOrAdd(KeyElem.GetData()) == nullptr)
					{
						FScriptMapHelper(MapProperty, Container + Offset).AddPair(
							KeyElem.GetData(), ValueElem.GetData());
					}
				}
				NotifyPostPropertyValueChanged(Property);
			}
			ImGui::SameLine();
			if (ImGui::Button(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("x")))))
			{
				for (uint8* Container : Containers)
				{
					FScriptMapHelper(MapProperty, Container + Offset).EmptyValues();
				}
				NotifyPostPropertyValueChanged(Property);
			}
		}
		else
		{
			ImGui::Text("Different Elements *");
			ImGui::SameLine();
			if (ImGui::Button(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("x")))))
			{
				for (uint8* Container : Containers)
				{
					FScriptMapHelper(MapProperty, Container + Offset).EmptyValues();
				}
				NotifyPostPropertyValueChanged(Property);
			}
		}
	}

	void FMapPropertyCustomization::CreateChildrenWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };

		const FMapProperty* MapProperty = CastFieldChecked<FMapProperty>(Property);
		const TSharedPtr<IUnrealPropertyCustomization> KeyPropertyCustomization = UnrealPropertyCustomizeFactory::FindCustomizer(MapProperty->KeyProp);
		const TSharedPtr<IUnrealPropertyCustomization> ValuePropertyCustomization = UnrealPropertyCustomizeFactory::FindCustomizer(MapProperty->ValueProp);
		if (!KeyPropertyCustomization || !ValuePropertyCustomization)
		{
			return;
		}

		TArray<FScriptMapHelper> Helpers;
		for (uint8* Container : Containers)
		{
			Helpers.Emplace(FScriptMapHelper(MapProperty, Container + Offset));
		}

		FStructArray KeyRawPtr;
		KeyRawPtr.SetNum(Containers.Num());
		FStructArray ValueRawPtr;
		ValueRawPtr.SetNum(Containers.Num());
		for (int32 ElemIdx = 0; ElemIdx < Helpers[0].Num(); ++ElemIdx)
		{
			for (int32 Idx = 0; Idx < Containers.Num(); ++Idx)
			{
				KeyRawPtr[Idx] = Helpers[Idx].GetKeyPtr(ElemIdx);
				ValueRawPtr[Idx] = Helpers[Idx].GetValuePtr(ElemIdx);
			}
			TGuardValue<int32> GImGuiContainerIndexGuard(InnerValue::GImGuiContainerIndex, ElemIdx);

			TSharedPtr<IUnrealStructCustomization> Customization = nullptr;
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				Customization = UnrealPropertyCustomizeFactory::FindCustomizer(StructProperty->Struct);
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			bool IsShowChildren = false;
			const bool KeyIsIdentical = IsAllPropertiesIdentical(MapProperty->KeyProp, KeyRawPtr, 0);
			const bool ValueIsIdentical = IsAllPropertiesIdentical(MapProperty->ValueProp, ValueRawPtr, 0);
			const bool KeyHasChildProperties = KeyPropertyCustomization->HasChildProperties(MapProperty->KeyProp, KeyRawPtr, 0, IsIdentical);
			const bool ValueHasChildProperties = ValuePropertyCustomization->HasChildProperties(MapProperty->ValueProp, ValueRawPtr, 0, IsIdentical);
			{
				const FString Name = FString::Printf(TEXT("%d##K%s%d"), ElemIdx, *Property->GetName(), InnerValue::GPropertyDepth);
				if (KeyHasChildProperties || ValueHasChildProperties)
				{
					IsShowChildren = ImGui::TreeNode(TCHAR_TO_UTF8(*Name));
				}
				else
				{
					const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_Bullet;
					ImGui::TreeNodeEx(TCHAR_TO_UTF8(*Name), flags);
				}
				ImGui::SameLine();
				if (Customization)
				{
					Customization->CreateValueWidget(MapProperty->KeyProp, KeyRawPtr, 0, KeyIsIdentical);
				}
				else
				{
					KeyPropertyCustomization->CreateValueWidget(MapProperty->KeyProp, KeyRawPtr, 0, KeyIsIdentical);
				}
				if (KeyHasChildProperties && IsShowChildren)
				{
					if (Customization)
					{
						Customization->CreateChildrenWidget(MapProperty->KeyProp, KeyRawPtr, 0, KeyIsIdentical);
					}
					else
					{
						KeyPropertyCustomization->CreateChildrenWidget(MapProperty->KeyProp, KeyRawPtr, 0, KeyIsIdentical);
					}
					ImGui::TreePop();
				}
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(InnerValue::ContainerValueRightWidth - (Customization ? Customization->ValueAdditiveRightWidth : 0.f));
			{
				const FString Name = FString::Printf(TEXT("%d##V%s%d"), ElemIdx, *Property->GetName(), InnerValue::GPropertyDepth);
				if (Customization)
				{
					Customization->CreateValueWidget(MapProperty->ValueProp, ValueRawPtr, 0, ValueIsIdentical);
				}
				else
				{
					ValuePropertyCustomization->CreateValueWidget(MapProperty->ValueProp, ValueRawPtr, 0, ValueIsIdentical);
				}
				ImGui::SameLine();
				const auto MapPopupID = TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT("unreal_map_popup")));
				if (ImGui::ArrowButton(TCHAR_TO_UTF8(*CreatePropertyLabel(Property, TEXT(">"))), ImGuiDir_Down))
				{
					ImGui::OpenPopup(MapPopupID);
				}
				ImGui::SameLine();
				if (ImGui::BeginPopup(MapPopupID))
				{
					if (ImGui::MenuItem("Delete"))
					{
						for (FScriptMapHelper& Helper : Helpers)
						{
							Helper.RemoveAt(ElemIdx);
						}
						NotifyPostPropertyValueChanged(Property);
					}
					ImGui::EndPopup();
				}
				if (ValueHasChildProperties && IsShowChildren)
				{
					if (Customization)
					{
						Customization->CreateChildrenWidget(MapProperty->ValueProp, ValueRawPtr, 0, ValueIsIdentical);
					}
					else
					{
						ValuePropertyCustomization->CreateChildrenWidget(MapProperty->ValueProp, ValueRawPtr, 0, ValueIsIdentical);
					}
					ImGui::TreePop();
				}
			}

			ImGui::NextColumn();
		}
	}

	void FStructPropertyCustomization::CreateValueWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };
		
		const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Property);
		int32 ElementCount = 0;
		for (FProperty* ChildProperty = StructProperty->Struct->PropertyLink; ChildProperty; ChildProperty =
		     ChildProperty->PropertyLinkNext)
		{
			if (IsPropertyShow(ChildProperty))
			{
				ElementCount += 1;
			}
		}
		ImGui::Text("%d Elements %s", ElementCount, IsIdentical ? "" : "*");
	}

	void FStructPropertyCustomization::CreateChildrenWidget(const FProperty* Property, const FStructArray& Containers, int32 Offset, bool IsIdentical) const
	{
		FPropertyDisableScope ImGuiDisableScope{ Property->HasAnyPropertyFlags(CPF_EditConst | CPF_DisableEditOnInstance) };

		const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Property);
		TGuardValue<int32> DepthGuard(InnerValue::GPropertyDepth, InnerValue::GPropertyDepth + 1);
		DrawDefaultStructDetails(StructProperty->Struct, Containers, Offset);
	}
}