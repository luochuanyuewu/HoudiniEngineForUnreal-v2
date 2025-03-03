/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HoudiniOutputDetails.h"

#include "HoudiniEngineEditorPrivatePCH.h"

#include "HoudiniAssetComponentDetails.h"
#include "HoudiniMeshTranslator.h"
#include "HoudiniInstanceTranslator.h"
#include "HoudiniAssetComponent.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniEngineBakeUtils.h"
#include "HoudiniEngineEditor.h"
#include "HoudiniEngineEditorUtils.h"
#include "HoudiniEnginePrivatePCH.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniEngineRuntimePrivatePCH.h"
#include "HoudiniAsset.h"
#include "HoudiniSplineComponent.h"
#include "HoudiniStaticMesh.h"
#include "HoudiniEngineCommands.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Input/SRotatorInputBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor/UnrealEd/Public/AssetThumbnail.h"
#include "SAssetDropTarget.h"
#include "Engine/StaticMesh.h"
#include "Components/SplineComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Sound/SoundBase.h"
#include "Engine/SkeletalMesh.h"
#include "Particles/ParticleSystem.h"
//#include "Landscape.h"
#include "LandscapeProxy.h"
#include "ScopedTransaction.h"
#include "PhysicsEngine/BodySetup.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "Editor/PropertyEditor/Public/PropertyCustomizationHelpers.h"

#define LOCTEXT_NAMESPACE HOUDINI_LOCTEXT_NAMESPACE

void
FHoudiniOutputDetails::CreateWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	TArray<UHoudiniOutput*> InOutputs)
{
	if (InOutputs.Num() <= 0)
		return;

	UHoudiniOutput* MainOutput = InOutputs[0];
	if (!IsValid(MainOutput)) 
		return;

	// Don't create UI for editable curve.
	if (MainOutput->IsEditableNode() && MainOutput->GetType() == EHoudiniOutputType::Curve)
		return;

	// Get thumbnail pool for this builder.
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = HouOutputCategory.GetParentLayout().GetThumbnailPool();

	switch (MainOutput->GetType()) 
	{
		case EHoudiniOutputType::Mesh: 
		{
			FHoudiniOutputDetails::CreateMeshOutputWidget(HouOutputCategory, MainOutput);
			break;
		}

		case EHoudiniOutputType::Landscape: 
		{
			FHoudiniOutputDetails::CreateLandscapeOutputWidget(HouOutputCategory, MainOutput);
			break;
		}

		case EHoudiniOutputType::Instancer:
		{
			FHoudiniOutputDetails::CreateInstancerOutputWidget(HouOutputCategory, MainOutput);
			break;
		}

		case EHoudiniOutputType::Curve:
		{
			FHoudiniOutputDetails::CreateCurveOutputWidget(HouOutputCategory, MainOutput);
			break;
		}
		case EHoudiniOutputType::Skeletal:
		default: 
		{
			FHoudiniOutputDetails::CreateDefaultOutputWidget(HouOutputCategory, MainOutput);
			break;
		}
	}
}


void 
FHoudiniOutputDetails::CreateLandscapeOutputWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	UHoudiniOutput* InOutput)
{
	if (!InOutput || InOutput->IsPendingKill())
		return;

	// Go through this output's objects
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	for (auto& CurrentOutputObj : OutputObjects) 
	{
		FHoudiniOutputObjectIdentifier& Identifier = CurrentOutputObj.Key;
		const FHoudiniGeoPartObject *HGPO = nullptr;
		for (const auto& CurHGPO : InOutput->GetHoudiniGeoPartObjects())
		{
			if (!Identifier.Matches(CurHGPO))
				continue;

			HGPO = &CurHGPO;
			break;
		}
		
		if (!HGPO)
			continue;
		
		
		if (UHoudiniLandscapePtr* LandscapePointer = Cast<UHoudiniLandscapePtr>(CurrentOutputObj.Value.OutputObject))
		{
			CreateLandscapeOutputWidget_Helper(HouOutputCategory, InOutput, *HGPO, LandscapePointer, Identifier);
		}
		else if (UHoudiniLandscapeEditLayer* LandscapeLayer = Cast<UHoudiniLandscapeEditLayer>(CurrentOutputObj.Value.OutputObject))
		{
			// TODO: Create widget for landscape editlayer output
			CreateLandscapeEditLayerOutputWidget_Helper(HouOutputCategory, InOutput, *HGPO, LandscapeLayer, Identifier);
		}

		

	}
}

void
FHoudiniOutputDetails::CreateLandscapeOutputWidget_Helper(
	IDetailCategoryBuilder& HouOutputCategory,
	UHoudiniOutput* InOutput,
	const FHoudiniGeoPartObject& HGPO,
	UHoudiniLandscapePtr* LandscapePointer,
	const FHoudiniOutputObjectIdentifier & OutputIdentifier)
{
	if (!LandscapePointer || LandscapePointer->IsPendingKill() || !LandscapePointer->LandscapeSoftPtr.IsValid())
		return;

	if (!InOutput || InOutput->IsPendingKill())
		return;

	UHoudiniAssetComponent * HAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	if (!HAC || HAC->IsPendingKill())
		return;

	AActor * OwnerActor = HAC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return;

	ALandscapeProxy * Landscape = LandscapePointer->LandscapeSoftPtr.Get();
	if (!Landscape || Landscape->IsPendingKill())
		return;

	// TODO: Get bake base name
	FString Label = Landscape->GetName();

	EHoudiniLandscapeOutputBakeType & LandscapeOutputBakeType = LandscapePointer->BakeType;

	// Get thumbnail pool for this builder
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	TArray<TSharedPtr<FString>>* BakeOptionString = FHoudiniEngineEditor::Get().GetHoudiniLandscapeOutputBakeOptionsLabels();

	// Create bake mesh name textfield.
	IDetailGroup& LandscapeGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label));
	LandscapeGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BakeBaseName", "Bake Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(Label))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(LOCTEXT("BakeNameTip", "The base name of the baked asset"))
			.HintText(LOCTEXT("BakeNameHintText", "Input bake name to override default"))
			.OnTextCommitted_Lambda([InOutput, OutputIdentifier](const FText& Val, ETextCommit::Type TextCommitType)
			{
				FHoudiniOutputDetails::OnBakeNameCommitted(Val, TextCommitType, InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
			})
		]
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RevertNameOverride", "Revert bake name override"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda([InOutput, OutputIdentifier]() 
			{
				FHoudiniOutputDetails::OnRevertBakeNameToDefault(InOutput, OutputIdentifier);
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];

	// Create the thumbnail for the landscape output object.
	TSharedPtr< FAssetThumbnail > LandscapeThumbnail =
		MakeShareable(new FAssetThumbnail(Landscape, 64, 64, AssetThumbnailPool));

	TSharedPtr< SBorder > LandscapeThumbnailBorder;
	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	LandscapeGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(SSpacer)
		.Size(FVector2D(250, 64))
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		VerticalBox
	];

	VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
	[
		SNew(SBox).WidthOverride(175)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 0.0f, 2.0f, 0.0f)
			.AutoWidth()
			[
				SAssignNew(LandscapeThumbnailBorder, SBorder)
				.Padding(5.0f)
				.BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (UObject*)Landscape)
				.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (UObject *)Landscape)
				[
					SNew(SBox)
					.WidthOverride(64)
					.HeightOverride(64)
					.ToolTipText(FText::FromString(Landscape->GetPathName()))
					[
						LandscapeThumbnail->MakeThumbnailWidget()
					]
				]
			]

			+ SHorizontalBox::Slot()
			.Padding(0.0f, 4.0f, 4.0f, 4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(40.0f)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("Bake", "Bake"))
					.IsEnabled(true)
					.OnClicked_Lambda([InOutput, OutputIdentifier, HAC, OwnerActor, HGPO, Landscape, LandscapeOutputBakeType]()
					{
						FHoudiniOutputObject* FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
						if (FoundOutputObject)
						{
							TArray<UHoudiniOutput*> AllOutputs;
							AllOutputs.Reserve(HAC->GetNumOutputs());
							HAC->GetOutputs(AllOutputs);
							FHoudiniOutputDetails::OnBakeOutputObject(
								FoundOutputObject->BakeName,
								Landscape,
								OutputIdentifier,
								*FoundOutputObject,
								HGPO,
								HAC,
								OwnerActor->GetName(),
								HAC->BakeFolder.Path,
								HAC->TemporaryCookFolder.Path,
								InOutput->GetType(),
								LandscapeOutputBakeType,
								AllOutputs);
						}

						// TODO: Remove the output landscape if the landscape bake type is Detachment?
						return FReply::Handled();
					})
					.ToolTipText(LOCTEXT("HoudiniLandscapeBakeButton", "Bake this landscape"))	
				]	
			]
			+ SHorizontalBox::Slot()
			.Padding(0.0f, 4.0f, 4.0f, 4.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(120.f)
				[
					SNew(SComboBox<TSharedPtr<FString>>)
					.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniLandscapeOutputBakeOptionsLabels())
					.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniLandscapeOutputBakeOptionsLabels())[(uint8)LandscapeOutputBakeType])
					.OnGenerateWidget_Lambda(
						[](TSharedPtr< FString > InItem)
					{
						return SNew(STextBlock).Text(FText::FromString(*InItem));
					})
					.OnSelectionChanged_Lambda(
						[LandscapePointer, InOutput](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
						{
							if (SelectType != ESelectInfo::Type::OnMouseClick)
								return;

							FString *NewChoiceStr = NewChoice.Get();
							if (!NewChoiceStr)
								return;

							if (*NewChoiceStr == FHoudiniEngineEditorUtils::HoudiniLandscapeOutputBakeTypeToString(EHoudiniLandscapeOutputBakeType::Detachment))
							{
								LandscapePointer->SetLandscapeOutputBakeType(EHoudiniLandscapeOutputBakeType::Detachment);
							}
							else if (*NewChoiceStr == FHoudiniEngineEditorUtils::HoudiniLandscapeOutputBakeTypeToString(EHoudiniLandscapeOutputBakeType::BakeToImage))
							{
								LandscapePointer->SetLandscapeOutputBakeType(EHoudiniLandscapeOutputBakeType::BakeToImage);
							}
							else
							{
								LandscapePointer->SetLandscapeOutputBakeType(EHoudiniLandscapeOutputBakeType::BakeToWorld);
							}

							FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
						})
					[
						SNew(STextBlock)
						.Text_Lambda([LandscapePointer]()
						{
							FString BakeTypeString = FHoudiniEngineEditorUtils::HoudiniLandscapeOutputBakeTypeToString(LandscapePointer->GetLandscapeOutputBakeType());
							return FText::FromString(BakeTypeString);
						})
						.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]		
			]
		]
	];

	// Store thumbnail for this landscape.
	OutputObjectThumbnailBorders.Add(Landscape, LandscapeThumbnailBorder);

	// We need to add material box for each the landscape and landscape hole materials
	for (int32 MaterialIdx = 0; MaterialIdx < 2; ++MaterialIdx)
	{
		UMaterialInterface * MaterialInterface = MaterialIdx == 0 ? Landscape->GetLandscapeMaterial() : Landscape->GetLandscapeHoleMaterial();
		TSharedPtr<SBorder> MaterialThumbnailBorder;
		TSharedPtr<SHorizontalBox> HorizontalBox = NULL;

		FString MaterialName, MaterialPathName;
		if (MaterialInterface)
		{
			MaterialName = MaterialInterface->GetName();
			MaterialPathName = MaterialInterface->GetPathName();
		}

		// Create thumbnail for this material.
		TSharedPtr< FAssetThumbnail > MaterialInterfaceThumbnail =
			MakeShareable(new FAssetThumbnail(MaterialInterface, 64, 64, AssetThumbnailPool));

		VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
		[
			SNew(STextBlock)
			.Text(MaterialIdx == 0 ? LOCTEXT("LandscapeMaterial", "Landscape Material") : LOCTEXT("LandscapeHoleMaterial", "Landscape Hole Material"))
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
		];

		VerticalBox->AddSlot().Padding(0, 2)
		[
			SNew(SAssetDropTarget)
			.OnIsAssetAcceptableForDrop(this, &FHoudiniOutputDetails::OnMaterialInterfaceDraggedOver)
			.OnAssetDropped(this, &FHoudiniOutputDetails::OnMaterialInterfaceDropped, Landscape, InOutput, MaterialIdx)
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
			]
		];

		HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
		[
			SAssignNew(MaterialThumbnailBorder, SBorder)
			.Padding(5.0f)
			.BorderImage(this, &FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder, (UObject*)Landscape, MaterialIdx)
			.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (UObject *)MaterialInterface)
			[
				SNew(SBox)
				.WidthOverride(64)
				.HeightOverride(64)
				.ToolTipText(FText::FromString(MaterialPathName))
				[
					MaterialInterfaceThumbnail->MakeThumbnailWidget()
				]
			]
		];

		// Store thumbnail for this landscape and material index.
		{
			TPairInitializer<ALandscapeProxy *, int32> Pair(Landscape, MaterialIdx);
			MaterialInterfaceThumbnailBorders.Add(Pair, MaterialThumbnailBorder);
		}

		// Combox Box and Button Box
		TSharedPtr<SVerticalBox> ComboAndButtonBox;
		HorizontalBox->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ComboAndButtonBox, SVerticalBox)
		];

		// Combo row
		TSharedPtr< SComboButton > AssetComboButton;
		ComboAndButtonBox->AddSlot().FillHeight(1.0f)
		[
			SNew(SVerticalBox) + SVerticalBox::Slot().FillHeight(1.0f)
			[
				SAssignNew(AssetComboButton, SComboButton)
				//.ToolTipText( this, &FHoudiniAssetComponentDetails::OnGetToolTip )
				.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent(this, &FHoudiniOutputDetails::OnGetMaterialInterfaceMenuContent, MaterialInterface, (UObject*)Landscape, InOutput, MaterialIdx)
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(FText::FromString(MaterialName))
				]
			]
		];

		// Buttons row
		TSharedPtr<SHorizontalBox> ButtonBox;
		ComboAndButtonBox->AddSlot().FillHeight(1.0f)
		[
			SAssignNew(ButtonBox, SHorizontalBox)
		];

		// Add use Content Browser selection arrow
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnUseContentBrowserSelectedMaterialInterface,
					(UObject*)Landscape, InOutput, MaterialIdx),
				TAttribute< FText >(LOCTEXT("UseSelectedAssetFromContentBrowser", "Use Selected Asset from Content Browser")))
		];

		// Create tooltip.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Asset"), FText::FromString(MaterialName));
		FText MaterialTooltip = FText::Format(
			LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnBrowseTo, (UObject*)MaterialInterface),
				TAttribute< FText >(MaterialTooltip))
		];

		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("ResetToBaseMaterial", "Reset to base material"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked(	this, &FHoudiniOutputDetails::OnResetMaterialInterfaceClicked, Landscape, InOutput, MaterialIdx)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		];

		// Store combo button for this mesh and index.
		{
			TPairInitializer<ALandscapeProxy *, int32> Pair(Landscape, MaterialIdx);
			MaterialInterfaceComboButtons.Add(Pair, AssetComboButton);
		}
	}
}

void FHoudiniOutputDetails::CreateLandscapeEditLayerOutputWidget_Helper(IDetailCategoryBuilder& HouOutputCategory,
	UHoudiniOutput* InOutput, const FHoudiniGeoPartObject& HGPO, UHoudiniLandscapeEditLayer* LandscapeEditLayer,
	const FHoudiniOutputObjectIdentifier& OutputIdentifier)
{
	if (!LandscapeEditLayer || LandscapeEditLayer->IsPendingKill() || !LandscapeEditLayer->LandscapeSoftPtr.IsValid())
		return;

	if (!InOutput || InOutput->IsPendingKill())
		return;

	UHoudiniAssetComponent * HAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	if (!HAC || HAC->IsPendingKill())
		return;

	AActor * OwnerActor = HAC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return;

	ALandscapeProxy * Landscape = LandscapeEditLayer->LandscapeSoftPtr.Get();
	if (!Landscape || Landscape->IsPendingKill())
		return;

	const FString Label = Landscape->GetName();
	const FString LayerName = LandscapeEditLayer->LayerName;

	// Get thumbnail pool for this builder
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	// Create labels to display the edit layer name.
	IDetailGroup& LandscapeGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label));
	LandscapeGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("LandscapeEditLayerName", "Edit Layer Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(STextBlock)
		.Text(FText::AsCultureInvariant(LayerName))
		.Font(IDetailLayoutBuilder::GetDetailFont())

		// SNew(SHorizontalBox)
		// + SHorizontalBox::Slot()
		// .Padding(2.0f, 0.0f)
		// .VAlign(VAlign_Center)
		// .FillWidth(1)
		// [
		// 	SNew(SEditableTextBox)
		// 	.Text(FText::FromString(Label))
		// 	.Font(IDetailLayoutBuilder::GetDetailFont())
		// 	.ToolTipText(LOCTEXT("BakeNameTip", "The base name of the baked asset"))
		// 	.HintText(LOCTEXT("BakeNameHintText", "Input bake name to override default"))
		// 	.OnTextCommitted_Lambda([InOutput, OutputIdentifier](const FText& Val, ETextCommit::Type TextCommitType)
		// 	{
		// 		FHoudiniOutputDetails::OnBakeNameCommitted(Val, TextCommitType, InOutput, OutputIdentifier);
		// 		FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
		// 	})
		// ]
		// + SHorizontalBox::Slot()
		// .Padding(2.0f, 0.0f)
		// .VAlign(VAlign_Center)
		// .AutoWidth()
		// [
		// 	SNew(SButton)
		// 	.ToolTipText(LOCTEXT("RevertNameOverride", "Revert bake name override"))
		// 	.ButtonStyle(FEditorStyle::Get(), "NoBorder")
		// 	.ContentPadding(0)
		// 	.Visibility(EVisibility::Visible)
		// 	.OnClicked_Lambda([InOutput, OutputIdentifier]() 
		// 	{
		// 		FHoudiniOutputDetails::OnRevertBakeNameToDefault(InOutput, OutputIdentifier);
		// 		return FReply::Handled();
		// 	})
		// 	[
		// 		SNew(SImage)
		// 		.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
		// 	]
		// ]
	];

	// // Create the thumbnail for the landscape output object.
	// TSharedPtr< FAssetThumbnail > LandscapeThumbnail =
	// 	MakeShareable(new FAssetThumbnail(Landscape, 64, 64, AssetThumbnailPool));
	//
	// TSharedPtr< SBorder > LandscapeThumbnailBorder;
	// TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);
	//
	// LandscapeGrp.AddWidgetRow()
	// .NameContent()
	// [
	// 	SNew(SSpacer)
	// 	.Size(FVector2D(250, 64))
	// ]
	// .ValueContent()
	// .MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	// [
	// 	VerticalBox
	// ];
	//
	// VerticalBox->AddSlot().Padding(0, 2).AutoHeight()
	// [
	// 	SNew(SBox).WidthOverride(175)
	// 	[
	// 		SNew(SHorizontalBox)
	// 		+ SHorizontalBox::Slot()
	// 		.Padding(0.0f, 0.0f, 2.0f, 0.0f)
	// 		.AutoWidth()
	// 		[
	// 			SAssignNew(LandscapeThumbnailBorder, SBorder)
	// 			.Padding(5.0f)
	// 			.BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (UObject*)Landscape)
	// 			.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (UObject *)Landscape)
	// 			[
	// 				SNew(SBox)
	// 				.WidthOverride(64)
	// 				.HeightOverride(64)
	// 				.ToolTipText(FText::FromString(Landscape->GetPathName()))
	// 				[
	// 					LandscapeThumbnail->MakeThumbnailWidget()
	// 				]
	// 			]
	// 		]
	//
	// 		+ SHorizontalBox::Slot()
	// 		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
	// 		.VAlign(VAlign_Center)
	// 		[
	// 			SNew(SBox).WidthOverride(40.0f)
	// 			[
	// 				SNew(SButton)
	// 				.VAlign(VAlign_Center)
	// 				.HAlign(HAlign_Center)
	// 				.Text(LOCTEXT("Bake", "Bake"))
	// 				.IsEnabled(true)
	// 				.OnClicked_Lambda([InOutput, OutputIdentifier, HAC, OwnerActor, HGPO, Landscape, LandscapeOutputBakeType]()
	// 				{
	// 					FHoudiniOutputObject* FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
	// 					if (FoundOutputObject)
	// 					{
	// 						TArray<UHoudiniOutput*> AllOutputs;
	// 						AllOutputs.Reserve(HAC->GetNumOutputs());
	// 						HAC->GetOutputs(AllOutputs);
	// 						FHoudiniOutputDetails::OnBakeOutputObject(
	// 							FoundOutputObject->BakeName,
	// 							Landscape,
	// 							OutputIdentifier,
	// 							*FoundOutputObject,
	// 							HGPO,
	// 							HAC,
	// 							OwnerActor->GetName(),
	// 							HAC->BakeFolder.Path,
	// 							HAC->TemporaryCookFolder.Path,
	// 							InOutput->GetType(),
	// 							LandscapeOutputBakeType,
	// 							AllOutputs);
	// 					}
	//
	// 					// TODO: Remove the output landscape if the landscape bake type is Detachment?
	// 					return FReply::Handled();
	// 				})
	// 				.ToolTipText(LOCTEXT("HoudiniLandscapeBakeButton", "Bake this landscape"))	
	// 			]	
	// 		]
	// 		+ SHorizontalBox::Slot()
	// 		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
	// 		.VAlign(VAlign_Center)
	// 		[
	// 			SNew(SBox).WidthOverride(120.f)
	// 			[
	// 				SNew(SComboBox<TSharedPtr<FString>>)
	// 				.OptionsSource(FHoudiniEngineEditor::Get().GetHoudiniLandscapeOutputBakeOptionsLabels())
	// 				.InitiallySelectedItem((*FHoudiniEngineEditor::Get().GetHoudiniLandscapeOutputBakeOptionsLabels())[(uint8)LandscapeOutputBakeType])
	// 				.OnGenerateWidget_Lambda(
	// 					[](TSharedPtr< FString > InItem)
	// 				{
	// 					return SNew(STextBlock).Text(FText::FromString(*InItem));
	// 				})
	// 				.OnSelectionChanged_Lambda(
	// 					[LandscapePointer, InOutput](TSharedPtr<FString> NewChoice, ESelectInfo::Type SelectType)
	// 					{
	// 						if (SelectType != ESelectInfo::Type::OnMouseClick)
	// 							return;
	//
	// 						FString *NewChoiceStr = NewChoice.Get();
	// 						if (!NewChoiceStr)
	// 							return;
	//
	// 						if (*NewChoiceStr == FHoudiniEngineEditorUtils::HoudiniLandscapeOutputBakeTypeToString(EHoudiniLandscapeOutputBakeType::Detachment))
	// 						{
	// 							LandscapePointer->SetLandscapeOutputBakeType(EHoudiniLandscapeOutputBakeType::Detachment);
	// 						}
	// 						else if (*NewChoiceStr == FHoudiniEngineEditorUtils::HoudiniLandscapeOutputBakeTypeToString(EHoudiniLandscapeOutputBakeType::BakeToImage))
	// 						{
	// 							LandscapePointer->SetLandscapeOutputBakeType(EHoudiniLandscapeOutputBakeType::BakeToImage);
	// 						}
	// 						else
	// 						{
	// 							LandscapePointer->SetLandscapeOutputBakeType(EHoudiniLandscapeOutputBakeType::BakeToWorld);
	// 						}
	//
	// 						FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
	// 					})
	// 				[
	// 					SNew(STextBlock)
	// 					.Text_Lambda([LandscapePointer]()
	// 					{
	// 						FString BakeTypeString = FHoudiniEngineEditorUtils::HoudiniLandscapeOutputBakeTypeToString(LandscapePointer->GetLandscapeOutputBakeType());
	// 						return FText::FromString(BakeTypeString);
	// 					})
	// 					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	// 				]
	// 			]		
	// 		]
	// 	]
	// ];
	//
	// // Store thumbnail for this landscape.
	// OutputObjectThumbnailBorders.Add(Landscape, LandscapeThumbnailBorder);
	//
	// // We need to add material box for each the landscape and landscape hole materials
	// for (int32 MaterialIdx = 0; MaterialIdx < 2; ++MaterialIdx)
	// {
	// 	UMaterialInterface * MaterialInterface = MaterialIdx == 0 ? Landscape->GetLandscapeMaterial() : Landscape->GetLandscapeHoleMaterial();
	// 	TSharedPtr<SBorder> MaterialThumbnailBorder;
	// 	TSharedPtr<SHorizontalBox> HorizontalBox = NULL;
	//
	// 	FString MaterialName, MaterialPathName;
	// 	if (MaterialInterface)
	// 	{
	// 		MaterialName = MaterialInterface->GetName();
	// 		MaterialPathName = MaterialInterface->GetPathName();
	// 	}
	//
	// 	// Create thumbnail for this material.
	// 	TSharedPtr< FAssetThumbnail > MaterialInterfaceThumbnail =
	// 		MakeShareable(new FAssetThumbnail(MaterialInterface, 64, 64, AssetThumbnailPool));
	//
	// 	VerticalBox->AddSlot().Padding(2, 2, 5, 2).AutoHeight()
	// 	[
	// 		SNew(STextBlock)
	// 		.Text(MaterialIdx == 0 ? LOCTEXT("LandscapeMaterial", "Landscape Material") : LOCTEXT("LandscapeHoleMaterial", "Landscape Hole Material"))
	// 		.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
	// 	];
	//
	// 	VerticalBox->AddSlot().Padding(0, 2)
	// 	[
	// 		SNew(SAssetDropTarget)
	// 		.OnIsAssetAcceptableForDrop(this, &FHoudiniOutputDetails::OnMaterialInterfaceDraggedOver)
	// 		.OnAssetDropped(this, &FHoudiniOutputDetails::OnMaterialInterfaceDropped, Landscape, InOutput, MaterialIdx)
	// 		[
	// 			SAssignNew(HorizontalBox, SHorizontalBox)
	// 		]
	// 	];
	//
	// 	HorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
	// 	[
	// 		SAssignNew(MaterialThumbnailBorder, SBorder)
	// 		.Padding(5.0f)
	// 		.BorderImage(this, &FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder, (UObject*)Landscape, MaterialIdx)
	// 		.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (UObject *)MaterialInterface)
	// 		[
	// 			SNew(SBox)
	// 			.WidthOverride(64)
	// 			.HeightOverride(64)
	// 			.ToolTipText(FText::FromString(MaterialPathName))
	// 			[
	// 				MaterialInterfaceThumbnail->MakeThumbnailWidget()
	// 			]
	// 		]
	// 	];
	//
	// 	// Store thumbnail for this landscape and material index.
	// 	{
	// 		TPairInitializer<ALandscapeProxy *, int32> Pair(Landscape, MaterialIdx);
	// 		MaterialInterfaceThumbnailBorders.Add(Pair, MaterialThumbnailBorder);
	// 	}
	//
	// 	// Combox Box and Button Box
	// 	TSharedPtr<SVerticalBox> ComboAndButtonBox;
	// 	HorizontalBox->AddSlot()
	// 	.FillWidth(1.0f)
	// 	.Padding(0.0f, 4.0f, 4.0f, 4.0f)
	// 	.VAlign(VAlign_Center)
	// 	[
	// 		SAssignNew(ComboAndButtonBox, SVerticalBox)
	// 	];
	//
	// 	// Combo row
	// 	TSharedPtr< SComboButton > AssetComboButton;
	// 	ComboAndButtonBox->AddSlot().FillHeight(1.0f)
	// 	[
	// 		SNew(SVerticalBox) + SVerticalBox::Slot().FillHeight(1.0f)
	// 		[
	// 			SAssignNew(AssetComboButton, SComboButton)
	// 			//.ToolTipText( this, &FHoudiniAssetComponentDetails::OnGetToolTip )
	// 			.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
	// 			.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
	// 			.OnGetMenuContent(this, &FHoudiniOutputDetails::OnGetMaterialInterfaceMenuContent, MaterialInterface, (UObject*)Landscape, InOutput, MaterialIdx)
	// 			.ContentPadding(2.0f)
	// 			.ButtonContent()
	// 			[
	// 				SNew(STextBlock)
	// 				.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
	// 				.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
	// 				.Text(FText::FromString(MaterialName))
	// 			]
	// 		]
	// 	];
	//
	// 	// Buttons row
	// 	TSharedPtr<SHorizontalBox> ButtonBox;
	// 	ComboAndButtonBox->AddSlot().FillHeight(1.0f)
	// 	[
	// 		SAssignNew(ButtonBox, SHorizontalBox)
	// 	];
	//
	// 	// Add use Content Browser selection arrow
	// 	ButtonBox->AddSlot()
	// 	.AutoWidth()
	// 	.Padding(2.0f, 0.0f)
	// 	.VAlign(VAlign_Center)
	// 	[
	// 		PropertyCustomizationHelpers::MakeUseSelectedButton(
	// 			FSimpleDelegate::CreateSP(
	// 				this, &FHoudiniOutputDetails::OnUseContentBrowserSelectedMaterialInterface,
	// 				(UObject*)Landscape, InOutput, MaterialIdx),
	// 			TAttribute< FText >(LOCTEXT("UseSelectedAssetFromContentBrowser", "Use Selected Asset from Content Browser")))
	// 	];
	//
	// 	// Create tooltip.
	// 	FFormatNamedArguments Args;
	// 	Args.Add(TEXT("Asset"), FText::FromString(MaterialName));
	// 	FText MaterialTooltip = FText::Format(
	// 		LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);
	//
	// 	ButtonBox->AddSlot()
	// 	.AutoWidth()
	// 	.Padding(2.0f, 0.0f)
	// 	.VAlign(VAlign_Center)
	// 	[
	// 		PropertyCustomizationHelpers::MakeBrowseButton(
	// 			FSimpleDelegate::CreateSP(
	// 				this, &FHoudiniOutputDetails::OnBrowseTo, (UObject*)MaterialInterface),
	// 			TAttribute< FText >(MaterialTooltip))
	// 	];
	//
	// 	ButtonBox->AddSlot()
	// 	.AutoWidth()
	// 	.Padding(2.0f, 0.0f)
	// 	.VAlign(VAlign_Center)
	// 	[
	// 		SNew(SButton)
	// 		.ToolTipText(LOCTEXT("ResetToBaseMaterial", "Reset to base material"))
	// 		.ButtonStyle(FEditorStyle::Get(), "NoBorder")
	// 		.ContentPadding(0)
	// 		.Visibility(EVisibility::Visible)
	// 		.OnClicked(	this, &FHoudiniOutputDetails::OnResetMaterialInterfaceClicked, Landscape, InOutput, MaterialIdx)
	// 		[
	// 			SNew(SImage)
	// 			.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
	// 		]
	// 	];
	//
	// 	// Store combo button for this mesh and index.
	// 	{
	// 		TPairInitializer<ALandscapeProxy *, int32> Pair(Landscape, MaterialIdx);
	// 		MaterialInterfaceComboButtons.Add(Pair, AssetComboButton);
	// 	}
	// }
}

void
FHoudiniOutputDetails::CreateMeshOutputWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	UHoudiniOutput* InOutput)
{
	if (!InOutput || InOutput->IsPendingKill())
		return;

	UHoudiniAssetComponent* HAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	if (!HAC || HAC->IsPendingKill())
		return;

	FString HoudiniAssetName;
	if (HAC->GetOwner() && (HAC->GetOwner()->IsPendingKill()))
	{
		HoudiniAssetName = HAC->GetOwner()->GetName();
	}
	else if (HAC->GetHoudiniAsset())
	{
		HoudiniAssetName = HAC->GetHoudiniAsset()->GetName();
	}
	else
	{
		HoudiniAssetName = HAC->GetName();
	}

	// Go through this output's object
	int32 OutputObjIdx = 0;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	for (auto& IterObject : OutputObjects)
	{
		UStaticMesh* StaticMesh = Cast<UStaticMesh>(IterObject.Value.OutputObject);
		UHoudiniStaticMesh* ProxyMesh = Cast<UHoudiniStaticMesh>(IterObject.Value.ProxyObject);

		if ((!StaticMesh || StaticMesh->IsPendingKill())
			&& (!ProxyMesh || ProxyMesh->IsPendingKill()))
			continue;

		FHoudiniOutputObjectIdentifier & OutputIdentifier = IterObject.Key;

		// Find the corresponding HGPO in the output
		FHoudiniGeoPartObject HoudiniGeoPartObject;
		for (const auto& curHGPO : InOutput->GetHoudiniGeoPartObjects())
		{
			if (!OutputIdentifier.Matches(curHGPO))
				continue;

			HoudiniGeoPartObject = curHGPO;
			break;
		}

		if (StaticMesh && !StaticMesh->IsPendingKill())
		{
			bool bIsProxyMeshCurrent = IterObject.Value.bProxyIsCurrent;

			// If we have a static mesh, alway display its widget even if the proxy is more recent
			CreateStaticMeshAndMaterialWidgets(
				HouOutputCategory, InOutput, StaticMesh, OutputIdentifier, HoudiniAssetName, HAC->BakeFolder.Path, HoudiniGeoPartObject, bIsProxyMeshCurrent);
		}
		else
		{
			// If we only have a proxy mesh, then show the proxy widget
			CreateProxyMeshAndMaterialWidgets(
				HouOutputCategory, InOutput, ProxyMesh, OutputIdentifier, HoudiniAssetName, HAC->BakeFolder.Path, HoudiniGeoPartObject);
		}
	}
}

void 
FHoudiniOutputDetails::CreateCurveOutputWidget(IDetailCategoryBuilder& HouOutputCategory, UHoudiniOutput* InOutput) 
{
	if (!InOutput || InOutput->IsPendingKill())
		return;

	int32 OutputObjIdx = 0;
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	for (auto& IterObject : OutputObjects)
	{
		FHoudiniOutputObject& CurrentOutputObject = IterObject.Value;
		USceneComponent* SplineComponent = Cast<USceneComponent>(IterObject.Value.OutputComponent);
		if (!SplineComponent || SplineComponent->IsPendingKill())
			continue;

		FHoudiniOutputObjectIdentifier& OutputIdentifier = IterObject.Key;
		FHoudiniGeoPartObject HoudiniGeoPartObject;
		for (const auto& curHGPO : InOutput->GetHoudiniGeoPartObjects()) 
		{
			if (!OutputIdentifier.Matches(curHGPO))
				continue;

			HoudiniGeoPartObject = curHGPO;
			break;
		}

		CreateCurveWidgets(HouOutputCategory, InOutput, SplineComponent, CurrentOutputObject, OutputIdentifier, HoudiniGeoPartObject);
	}
}

void 
FHoudiniOutputDetails::CreateCurveWidgets(
	IDetailCategoryBuilder& HouOutputCategory,
	UHoudiniOutput* InOutput,
	USceneComponent* SplineComponent,
	FHoudiniOutputObject& OutputObject,
	FHoudiniOutputObjectIdentifier& OutputIdentifier,
	FHoudiniGeoPartObject& HoudiniGeoPartObject) 
{
	if (!InOutput || InOutput->IsPendingKill())
		return;

	// We support Unreal Spline out only for now
	USplineComponent* SplineOutput = Cast<USplineComponent>(SplineComponent);
	if (!SplineOutput || SplineOutput->IsPendingKill())
		return;

	UHoudiniAssetComponent * HAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	if (!HAC || HAC->IsPendingKill())
		return;

	AActor * OwnerActor = HAC->GetOwner();
	if (!OwnerActor || OwnerActor->IsPendingKill())
		return;

	FHoudiniCurveOutputProperties* OutputProperty = &(OutputObject.CurveOutputProperty);
	EHoudiniCurveType OutputCurveType = OutputObject.CurveOutputProperty.CurveType;

	FString Label = SplineComponent->GetName();
	if (HoudiniGeoPartObject.bHasCustomPartName)
		Label = HoudiniGeoPartObject.PartName;

	//Label += FString("_") + OutputIdentifier.SplitIdentifier;

	FString OutputCurveName = OutputObject.BakeName;
	if(OutputCurveName.IsEmpty())
		OutputCurveName = OwnerActor->GetName() + "_" + Label;

	const FText& LabelText = FText::FromString("Unreal Spline");

	IDetailGroup& CurveOutputGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label), false, false);

	// Bake name row UI
	CurveOutputGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BakeBaseName", "Bake Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(OutputObject.BakeName))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ToolTipText(LOCTEXT("BakeNameTip", "The base name of the baked asset"))
			.HintText(LOCTEXT("BakeNameHintText", "Input bake name to override default"))
			.OnTextCommitted_Lambda([InOutput, OutputIdentifier](const FText& Val, ETextCommit::Type TextCommitType)
			{
				FHoudiniOutputDetails::OnBakeNameCommitted(Val, TextCommitType, InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
			})
		]

		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RevertNameOverride", "Revert bake name override"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda([InOutput, OutputIdentifier]()
			{
				FHoudiniOutputDetails::OnRevertBakeNameToDefault(InOutput, OutputIdentifier);
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];

	CurveOutputGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("OutputCurveSplineType", "Spline Type"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ToolTipText_Lambda([SplineOutput, Label, OutputCurveType]()
		{
		FString ToolTipStr = FString::Printf(TEXT(" curve: %s\n Export type: Unreal Spline\n num points: %d\n curve type: %s\n closed: %s"),
			*Label,
			SplineOutput->GetNumberOfSplinePoints(),
			*FHoudiniEngineEditorUtils::HoudiniCurveTypeToString(OutputCurveType),
			SplineOutput->IsClosedLoop() ? *(FString("yes")) : *(FString("no")));

		return FText::FromString(ToolTipStr);
	})
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(STextBlock)
		// We support Unreal Spline output only for now...
		.Text(LOCTEXT("OutputCurveSplineTypeUnreal", "Unreal Spline"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	];

	//if (bIsUnrealSpline)
	//{
		USplineComponent* UnrealSpline = Cast<USplineComponent>(SplineComponent);

		// Curve type combo box UI
		auto InitialSelectionLambda = [OutputProperty]()
		{
			if (OutputProperty->CurveType == EHoudiniCurveType::Polygon)
			{
				return (*FHoudiniEngineEditor::Get().GetUnrealOutputCurveTypeLabels())[0];
			}
			else
			{
				return (*FHoudiniEngineEditor::Get().GetUnrealOutputCurveTypeLabels())[1];
			}
		};

		TSharedPtr<SComboBox<TSharedPtr<FString>>> UnrealCurveTypeComboBox;

		CurveOutputGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("OutputCurveUnrealSplinePointType", "Spline Point Type"))
		]
		.ValueContent()
		.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
		[
			SAssignNew(UnrealCurveTypeComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(FHoudiniEngineEditor::Get().GetUnrealOutputCurveTypeLabels())
			.InitiallySelectedItem(InitialSelectionLambda())
			.OnGenerateWidget_Lambda(
				[](TSharedPtr< FString > InItem)
		{
			return SNew(STextBlock).Text(FText::FromString(*InItem));
		})
		.OnSelectionChanged_Lambda(
			[OutputProperty, InOutput, SplineComponent](TSharedPtr< FString > NewChoice, ESelectInfo::Type SelectType)
		{
			// Set the curve point type locally
			USplineComponent* Spline = Cast<USplineComponent>(SplineComponent);
			if (!Spline || Spline->IsPendingKill())
				return;

			FString *NewChoiceStr = NewChoice.Get();
			if (!NewChoiceStr)
				return;

			if (*NewChoiceStr == "Linear")
			{
				if (OutputProperty->CurveType == EHoudiniCurveType::Polygon)
					return;

				OutputProperty->CurveType = EHoudiniCurveType::Polygon;

				for (int32 PtIdx = 0; PtIdx < Spline->GetNumberOfSplinePoints(); ++PtIdx)
				{
					Spline->SetSplinePointType(PtIdx, ESplinePointType::Linear);
				}

				FHoudiniEngineEditorUtils::ReselectSelectedActors();
				FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
			}
			else if (*NewChoiceStr == "Curve")
			{
				if (OutputProperty->CurveType != EHoudiniCurveType::Polygon)
					return;

				OutputProperty->CurveType = EHoudiniCurveType::Bezier;

				for (int32 PtIdx = 0; PtIdx < Spline->GetNumberOfSplinePoints(); ++PtIdx)
				{
					Spline->SetSplinePointType(PtIdx, ESplinePointType::Curve);
				}

				FHoudiniEngineEditorUtils::ReselectSelectedActors();
				FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
			}
		})
		[
			SNew(STextBlock)
			.Text_Lambda([OutputProperty]()
			{
				if (OutputProperty->CurveType == EHoudiniCurveType::Polygon)
					return FText::FromString(TEXT("Linear"));
				else
					return FText::FromString(TEXT("Curve"));
			})
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
		];

		// Add closed curve checkbox UI
		TSharedPtr<SCheckBox> ClosedCheckBox;
		CurveOutputGrp.AddWidgetRow()
		.NameContent()
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(LOCTEXT("OutputCurveUnrealSplineClosed", "Closed"))
		]
		.ValueContent()
		[
			SAssignNew(ClosedCheckBox, SCheckBox)
			.OnCheckStateChanged_Lambda([UnrealSpline, InOutput](ECheckBoxState NewState)
			{
				if (!UnrealSpline || UnrealSpline->IsPendingKill())
					return;

				UnrealSpline->SetClosedLoop(NewState == ECheckBoxState::Checked);
				FHoudiniEngineEditorUtils::ReselectSelectedActors();
				FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
			})
			.IsChecked_Lambda([UnrealSpline]()
			{
				if (!UnrealSpline || UnrealSpline->IsPendingKill())
					return ECheckBoxState::Unchecked;

				return UnrealSpline->IsClosedLoop() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
		];
	//}

	// Add Bake Button UI
	TSharedPtr<SButton> BakeButton;
	CurveOutputGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
	]
	.ValueContent()
	[
		SAssignNew(BakeButton, SButton)
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		.Text(LOCTEXT("OutputCurveBakeButtonText", "Bake"))
		.IsEnabled(true)
		.ToolTipText(LOCTEXT("OutputCurveBakeButtonUnrealSplineTooltipText", "Bake to Unreal spline"))
		.OnClicked_Lambda([InOutput, SplineComponent, OutputIdentifier, HoudiniGeoPartObject, HAC, OwnerActor, OutputCurveName, OutputObject]()
		{
			TArray<UHoudiniOutput*> AllOutputs;
			AllOutputs.Reserve(HAC->GetNumOutputs());
			HAC->GetOutputs(AllOutputs);
			FHoudiniOutputDetails::OnBakeOutputObject(
				OutputCurveName,
				SplineComponent,
				OutputIdentifier,
				OutputObject,
				HoudiniGeoPartObject,
				HAC,
				OwnerActor->GetName(),
				HAC->BakeFolder.Path,
				HAC->TemporaryCookFolder.Path,
				InOutput->GetType(),
				EHoudiniLandscapeOutputBakeType::InValid,
				AllOutputs);

			return FReply::Handled();
		})
	];
}

void
FHoudiniOutputDetails::CreateStaticMeshAndMaterialWidgets(
	IDetailCategoryBuilder& HouOutputCategory,
	UHoudiniOutput* InOutput,
	UStaticMesh * StaticMesh,
	FHoudiniOutputObjectIdentifier& OutputIdentifier,
	const FString HoudiniAssetName,
	const FString BakeFolder,
	FHoudiniGeoPartObject& HoudiniGeoPartObject,
	const bool& bIsProxyMeshCurrent)
{
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return;

	UHoudiniAssetComponent* OwningHAC = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	
	FHoudiniOutputObject* FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
	FString BakeName = FoundOutputObject ? FoundOutputObject->BakeName : FString();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	// TODO: GetBakingBaseName!
	FString Label = StaticMesh->GetName();
	if (HoudiniGeoPartObject.bHasCustomPartName)
		Label = HoudiniGeoPartObject.PartName;

	// Create thumbnail for this mesh.
	TSharedPtr< FAssetThumbnail > StaticMeshThumbnail =
		MakeShareable(new FAssetThumbnail(StaticMesh, 64, 64, AssetThumbnailPool));
	TSharedPtr<SBorder> StaticMeshThumbnailBorder;

	TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox);
	
	IDetailGroup& StaticMeshGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label));
	StaticMeshGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BakeBaseName", "Bake Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(BakeName))
			.HintText(LOCTEXT("BakeNameHintText", "Input bake name to override default"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OnTextCommitted_Lambda([OutputIdentifier, InOutput](const FText& Val, ETextCommit::Type TextCommitType)
			{
				FHoudiniOutputDetails::OnBakeNameCommitted(Val, TextCommitType, InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(InOutput->GetOuter(), true);
			})
			.ToolTipText( LOCTEXT( "BakeNameTip", "The base name of the baked asset") )
		]

		+SHorizontalBox::Slot()
		.Padding( 2.0f, 0.0f )
		.VAlign( VAlign_Center )
		.AutoWidth()
		[
			SNew( SButton )
			.ToolTipText( LOCTEXT( "RevertNameOverride", "Revert bake name override" ) )
			.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			.ContentPadding( 0 )
			.Visibility( EVisibility::Visible )
			.OnClicked_Lambda([InOutput, OutputIdentifier]() 
			{
				FHoudiniOutputDetails::OnRevertBakeNameToDefault(InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(InOutput->GetOuter(), true);
				return FReply::Handled();
			})
			[
				SNew( SImage )
				.Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
			]
		]
	];

	// Add details on the SM colliders
	EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(OutputIdentifier.SplitIdentifier);
	FString MeshLabel = TEXT( "Static Mesh" );

	// If the Proxy mesh is more recent, indicate it in the details
	if (bIsProxyMeshCurrent)
	{
		MeshLabel += TEXT("\n(unrefined)");
	}

	// Indicate that this mesh is instanced
	if (HoudiniGeoPartObject.bIsInstanced)
	{
		MeshLabel += TEXT("\n(instanced)");
	}

	if (HoudiniGeoPartObject.bIsTemplated)
	{
		MeshLabel += TEXT("\n(templated)");
	}

	int32 NumSimpleColliders = 0;
	if (StaticMesh->BodySetup && !StaticMesh->BodySetup->IsPendingKill())
		NumSimpleColliders = StaticMesh->BodySetup->AggGeom.GetElementCount();

	if(NumSimpleColliders > 0)
	{
		MeshLabel += TEXT( "\n(") + FString::FromInt(NumSimpleColliders) + TEXT(" Simple Collider" );
		if (NumSimpleColliders > 1 )
			MeshLabel += TEXT("s");
		MeshLabel += TEXT(")");
	}
	else if (SplitType == EHoudiniSplitType::RenderedComplexCollider)
	{
		MeshLabel += TEXT( "\n(Rendered Complex Collider)" );
	}
	else if(SplitType == EHoudiniSplitType::InvisibleComplexCollider )
	{
		MeshLabel += TEXT( "\n(Invisible Complex Collider)" );
	}

	if ( StaticMesh->GetNumLODs() > 1 )
		MeshLabel += TEXT("\n(") + FString::FromInt( StaticMesh->GetNumLODs() ) + TEXT(" LODs)");

	if (HoudiniGeoPartObject.AllMeshSockets.Num() > 0)
	{
		if (bIsProxyMeshCurrent)
		{
			// Proxy is current, show the number of sockets on the HGPO
			MeshLabel += TEXT("\n(") + FString::FromInt(HoudiniGeoPartObject.AllMeshSockets.Num()) + TEXT(" sockets)");
		}
		else
		{
			// Show the number of sockets on the SM
			MeshLabel += TEXT("\n(") + FString::FromInt(StaticMesh->Sockets.Num()) + TEXT(" sockets)");
		}
	}

	UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	StaticMeshGrp.AddWidgetRow()
	.NameContent()
	[
		SNew( STextBlock )
		.Text( FText::FromString(MeshLabel) )
		.Font( IDetailLayoutBuilder::GetDetailFont() )
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		VerticalBox
	];
			
	VerticalBox->AddSlot()
	.Padding( 0, 2 )
	.AutoHeight()
	[
		SNew( SHorizontalBox )
		+SHorizontalBox::Slot()
		.Padding( 0.0f, 0.0f, 2.0f, 0.0f )
		.AutoWidth()
		[
			SAssignNew( StaticMeshThumbnailBorder, SBorder )
			.Padding( 5.0f )
			.BorderImage( this, &FHoudiniOutputDetails::GetThumbnailBorder, (UObject*)StaticMesh )
			.OnMouseDoubleClick( this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (UObject *) StaticMesh )
			[
				SNew( SBox )
				.WidthOverride( 64 )
				.HeightOverride( 64 )
				.ToolTipText( FText::FromString( StaticMesh->GetPathName() ) )
				[
					StaticMeshThumbnail->MakeThumbnailWidget()
				]
			]
		]

		+SHorizontalBox::Slot()
		.FillWidth( 1.0f )
		.Padding( 0.0f, 4.0f, 4.0f, 4.0f )
		.VAlign( VAlign_Center )
		[
			SNew( SVerticalBox )
			+SVerticalBox::Slot()
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.MaxWidth( 80.0f )
				[
					SNew( SButton )
					.VAlign( VAlign_Center )
					.HAlign( HAlign_Center )
					.Text( LOCTEXT( "Bake", "Bake" ) )
					.IsEnabled(true)
					.OnClicked_Lambda([BakeName, StaticMesh, OutputIdentifier, HoudiniGeoPartObject, HoudiniAssetName, BakeFolder, InOutput, OwningHAC, FoundOutputObject]()
					{
						if (FoundOutputObject)
						{
							TArray<UHoudiniOutput*> AllOutputs;
							FString TempCookFolder;
							if (IsValid(OwningHAC))
							{
								AllOutputs.Reserve(OwningHAC->GetNumOutputs());
								OwningHAC->GetOutputs(AllOutputs);

								TempCookFolder = OwningHAC->TemporaryCookFolder.Path;
							}
							FHoudiniOutputDetails::OnBakeOutputObject(
								BakeName,
								StaticMesh,
								OutputIdentifier,
								*FoundOutputObject,
								HoudiniGeoPartObject,
								OwningHAC,
								HoudiniAssetName,
								BakeFolder,
								TempCookFolder,
								InOutput->GetType(),
								EHoudiniLandscapeOutputBakeType::InValid,
								AllOutputs);
						}

						return FReply::Handled();
					})
					.ToolTipText( LOCTEXT( "HoudiniStaticMeshBakeButton", "Bake this generated static mesh" ) )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeBrowseButton(
						FSimpleDelegate::CreateSP(
							this, &FHoudiniOutputDetails::OnBrowseTo, (UObject*)StaticMesh),
							TAttribute<FText>(LOCTEXT("HoudiniStaticMeshBrowseButton", "Browse to this generated static mesh in the content browser")))
				]
			]
		]
	];

	// Store thumbnail for this mesh.
	OutputObjectThumbnailBorders.Add((UObject*)StaticMesh, StaticMeshThumbnailBorder);

	// We need to add material box for each material present in this static mesh.
	auto & StaticMeshMaterials = StaticMesh->StaticMaterials;
	for ( int32 MaterialIdx = 0; MaterialIdx < StaticMeshMaterials.Num(); ++MaterialIdx )
	{
		UMaterialInterface * MaterialInterface = StaticMeshMaterials[ MaterialIdx ].MaterialInterface;
		TSharedPtr< SBorder > MaterialThumbnailBorder;
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;

		FString MaterialName, MaterialPathName;
		if ( MaterialInterface && !MaterialInterface->IsPendingKill()
			&& MaterialInterface->GetOuter() && !MaterialInterface->GetOuter()->IsPendingKill() )
		{
			MaterialName = MaterialInterface->GetName();
			MaterialPathName = MaterialInterface->GetPathName();
		}
		else
		{
			MaterialInterface = nullptr;
			MaterialName = TEXT("Material (invalid)") + FString::FromInt( MaterialIdx ) ;
			MaterialPathName = TEXT("Material (invalid)") + FString::FromInt(MaterialIdx);
		}

		// Create thumbnail for this material.
		TSharedPtr< FAssetThumbnail > MaterialInterfaceThumbnail =
			MakeShareable( new FAssetThumbnail( MaterialInterface, 64, 64, AssetThumbnailPool ) );

		VerticalBox->AddSlot().Padding( 0, 2 )
		[
			SNew( SAssetDropTarget )
			.OnIsAssetAcceptableForDrop( this, &FHoudiniOutputDetails::OnMaterialInterfaceDraggedOver )
			.OnAssetDropped(
				this, &FHoudiniOutputDetails::OnMaterialInterfaceDropped, StaticMesh, InOutput, MaterialIdx )
			[
				SAssignNew( HorizontalBox, SHorizontalBox )
			]
		];

		HorizontalBox->AddSlot().Padding( 0.0f, 0.0f, 2.0f, 0.0f ).AutoWidth()
		[
			SAssignNew( MaterialThumbnailBorder, SBorder )
			.Padding( 5.0f )
			.BorderImage(
				this, &FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder, (UObject *)StaticMesh, MaterialIdx )
			.OnMouseDoubleClick(
				this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (UObject *)MaterialInterface )
			[
				SNew( SBox )
				.WidthOverride( 64 )
				.HeightOverride( 64 )
				.ToolTipText( FText::FromString( MaterialPathName ) )
				[
					MaterialInterfaceThumbnail->MakeThumbnailWidget()
				]
			]
		];

		// Store thumbnail for this mesh and material index.
		{
			TPairInitializer<UStaticMesh *, int32> Pair( StaticMesh, MaterialIdx );
			MaterialInterfaceThumbnailBorders.Add( Pair, MaterialThumbnailBorder );
		}

		// ComboBox and buttons
		TSharedPtr<SVerticalBox> ComboAndButtonBox;
		HorizontalBox->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
		[
			SAssignNew(ComboAndButtonBox, SVerticalBox)
		];

		// Add Combo box
		TSharedPtr< SComboButton > AssetComboButton;
		ComboAndButtonBox->AddSlot().VAlign(VAlign_Center).FillHeight(1.0f)
		[
			SNew(SVerticalBox) + SVerticalBox::Slot().VAlign(VAlign_Center).FillHeight(1.0f)
			[
				SAssignNew(AssetComboButton, SComboButton)
				.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.OnGetMenuContent(this, &FHoudiniOutputDetails::OnGetMaterialInterfaceMenuContent,
				MaterialInterface, (UObject*)StaticMesh, InOutput, MaterialIdx)
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(FText::FromString(MaterialName))
				]
			]
		];


		// Create tooltip.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Asset"), FText::FromString(MaterialName));
		FText MaterialTooltip = FText::Format(
			LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);


		// Add buttons
		TSharedPtr< SHorizontalBox > ButtonBox;
		ComboAndButtonBox->AddSlot().FillHeight(1.0f)
		[
			SAssignNew(ButtonBox, SHorizontalBox)
		];

		// Use CB selection arrow button
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnUseContentBrowserSelectedMaterialInterface,
					(UObject*)StaticMesh, InOutput, MaterialIdx),
				TAttribute< FText >(LOCTEXT("UseSelectedAssetFromContentBrowser", "Use Selected Asset from Content Browser")))
		];

		// Browse CB button
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding( 2.0f, 0.0f )
		.VAlign( VAlign_Center )
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnBrowseTo, (UObject*)MaterialInterface ), TAttribute< FText >( MaterialTooltip ) )
		];

		// Reset button
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding( 2.0f, 0.0f )
		.VAlign( VAlign_Center )
		[
			SNew( SButton )
			.ToolTipText( LOCTEXT( "ResetToBaseMaterial", "Reset to base material" ) )
			.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
			.ContentPadding( 0 )
			.Visibility( EVisibility::Visible )
			.OnClicked(
				this, &FHoudiniOutputDetails::OnResetMaterialInterfaceClicked, StaticMesh, InOutput, MaterialIdx)
			[
				SNew( SImage )
				.Image( FEditorStyle::GetBrush( "PropertyWindow.DiffersFromDefault" ) )
			]
		];

		// Store combo button for this mesh and index.
		{
			TPairInitializer<UStaticMesh *, int32> Pair( StaticMesh, MaterialIdx );
			MaterialInterfaceComboButtons.Add( Pair, AssetComboButton );
		}
	}
}

void
FHoudiniOutputDetails::CreateProxyMeshAndMaterialWidgets(
	IDetailCategoryBuilder& HouOutputCategory,
	UHoudiniOutput* InOutput,
	UHoudiniStaticMesh * ProxyMesh,
	FHoudiniOutputObjectIdentifier& OutputIdentifier,
	const FString HoudiniAssetName,
	const FString BakeFolder,
	FHoudiniGeoPartObject& HoudiniGeoPartObject)
{
	if (!ProxyMesh || ProxyMesh->IsPendingKill())
		return;

	FHoudiniOutputObject* FoundOutputObject = InOutput->GetOutputObjects().Find(OutputIdentifier);
	FString BakeName = FoundOutputObject ? FoundOutputObject->BakeName : FString();

	// Get thumbnail pool for this builder.
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	// TODO: GetBakingBaseName!
	FString Label = ProxyMesh->GetName();
	if (HoudiniGeoPartObject.bHasCustomPartName)
		Label = HoudiniGeoPartObject.PartName;

	// Create thumbnail for this mesh.
	TSharedPtr<FAssetThumbnail> MeshThumbnail =	MakeShareable(new FAssetThumbnail(ProxyMesh, 64, 64, AssetThumbnailPool));
	TSharedPtr<SBorder> MeshThumbnailBorder;

	TSharedRef< SVerticalBox > VerticalBox = SNew(SVerticalBox);

	IDetailGroup& StaticMeshGrp = HouOutputCategory.AddGroup(FName(*Label), FText::FromString(Label));

	StaticMeshGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("BakeBaseName", "Bake Name"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.FillWidth(1)
		[
			SNew(SEditableTextBox)
			.Text(FText::FromString(BakeName))
			.HintText(LOCTEXT("BakeNameHintText", "Input bake name to override default"))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.OnTextCommitted_Lambda([OutputIdentifier, InOutput](const FText& Val, ETextCommit::Type TextCommitType)
			{
				FHoudiniOutputDetails::OnBakeNameCommitted(Val, TextCommitType, InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(InOutput->GetOuter(), true);
			})
			.ToolTipText(LOCTEXT("BakeNameTip", "The base name of the baked asset"))
		]
		+ SHorizontalBox::Slot()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("RevertNameOverride", "Revert bake name override"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked_Lambda([InOutput, OutputIdentifier]()
			{
				FHoudiniOutputDetails::OnRevertBakeNameToDefault(InOutput, OutputIdentifier);
				FHoudiniEngineUtils::UpdateEditorProperties(InOutput->GetOuter(), true);
				return FReply::Handled();
			})
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		]
	];

	// Add details on the Proxy Mesh
	EHoudiniSplitType SplitType = FHoudiniMeshTranslator::GetSplitTypeFromSplitName(OutputIdentifier.SplitIdentifier);
	FString MeshLabel = TEXT("Proxy Mesh");

	// Indicate that this mesh is instanced
	if (HoudiniGeoPartObject.bIsInstanced)
	{
		MeshLabel += TEXT("\n(instanced)");
	}

	if (HoudiniGeoPartObject.bIsTemplated)
	{
		MeshLabel += TEXT("\n(templated)");
	}

	if (HoudiniGeoPartObject.AllMeshSockets.Num() > 0)
	{
		MeshLabel += TEXT("\n(") + FString::FromInt(HoudiniGeoPartObject.AllMeshSockets.Num()) + TEXT(" sockets)");
	}

	UHoudiniAssetComponent* HoudiniAssetComponent = Cast<UHoudiniAssetComponent>(InOutput->GetOuter());
	StaticMeshGrp.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(FText::FromString(MeshLabel))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
	[
		VerticalBox
	];

	VerticalBox->AddSlot()
	.Padding(0, 2)
	.AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(0.0f, 0.0f, 2.0f, 0.0f)
		.AutoWidth()
		[
			SAssignNew(MeshThumbnailBorder, SBorder)
			.Padding(5.0f)
			.BorderImage(this, &FHoudiniOutputDetails::GetThumbnailBorder, (UObject*)ProxyMesh)
			.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (UObject *)ProxyMesh)
			[
				SNew(SBox)
				.WidthOverride(64)
				.HeightOverride(64)
				.ToolTipText(FText::FromString(ProxyMesh->GetPathName()))
				[
					MeshThumbnail->MakeThumbnailWidget()
				]
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.MaxWidth(80.0f)
				[
					SNew(SButton)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("Refine", "Refine"))					
					.IsEnabled(true)
					.OnClicked(this, &FHoudiniOutputDetails::OnRefineClicked, (UObject *)ProxyMesh, InOutput)
					.ToolTipText(LOCTEXT("RefineTooltip", "Refine this Proxy Mesh to a Static Mesh"))
				]
			]
		]
	];

	// Store thumbnail for this mesh.
	OutputObjectThumbnailBorders.Add(ProxyMesh, MeshThumbnailBorder);

	// We need to add material box for each material present in this static mesh.
	auto & ProxyMeshMaterials = ProxyMesh->GetStaticMaterials();
	for (int32 MaterialIdx = 0; MaterialIdx < ProxyMeshMaterials.Num(); ++MaterialIdx)
	{
		UMaterialInterface * MaterialInterface = ProxyMeshMaterials[MaterialIdx].MaterialInterface;
		TSharedPtr< SBorder > MaterialThumbnailBorder;
		TSharedPtr< SHorizontalBox > HorizontalBox = NULL;

		FString MaterialName, MaterialPathName;
		if (MaterialInterface && !MaterialInterface->IsPendingKill()
			&& MaterialInterface->GetOuter() && !MaterialInterface->GetOuter()->IsPendingKill())
		{
			MaterialName = MaterialInterface->GetName();
			MaterialPathName = MaterialInterface->GetPathName();
		}
		else
		{
			MaterialInterface = nullptr;
			MaterialName = TEXT("Material (invalid)") + FString::FromInt(MaterialIdx);
			MaterialPathName = TEXT("Material (invalid)") + FString::FromInt(MaterialIdx);
		}

		// Create thumbnail for this material.
		TSharedPtr<FAssetThumbnail> MaterialInterfaceThumbnail =
			MakeShareable(new FAssetThumbnail(MaterialInterface, 64, 64, AssetThumbnailPool));

		// No drop target
		VerticalBox->AddSlot()
		.Padding(0, 2)
		[
			SNew(SAssetDropTarget)
			//.OnIsAssetAcceptableForDrop(false)
			//.OnAssetDropped(
			//	this, &FHoudiniOutputDetails::OnMaterialInterfaceDropped, StaticMesh, InOutput, MaterialIdx)
			[
				SAssignNew(HorizontalBox, SHorizontalBox)
			]
		];

		HorizontalBox->AddSlot()
		.Padding(0.0f, 0.0f, 2.0f, 0.0f)
		.AutoWidth()
		[
			SAssignNew(MaterialThumbnailBorder, SBorder)
			.Padding(5.0f)
			.BorderImage(
				this, &FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder, (UObject*)ProxyMesh, MaterialIdx)
			.OnMouseDoubleClick(
				this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, (UObject *)MaterialInterface)
			[
				SNew(SBox)
				.WidthOverride(64)
				.HeightOverride(64)
				.ToolTipText(FText::FromString(MaterialPathName))
				[
					MaterialInterfaceThumbnail->MakeThumbnailWidget()
				]
			]
		];

		// Store thumbnail for this mesh and material index.
		{
			TPairInitializer<UObject*, int32> Pair((UObject*)ProxyMesh, MaterialIdx);
			MaterialInterfaceThumbnailBorders.Add(Pair, MaterialThumbnailBorder);
		}
				
		// Combo box and buttons
		TSharedPtr<SVerticalBox> ComboAndButtonBox;
		HorizontalBox->AddSlot()
		.FillWidth(1.0f)
		.Padding(0.0f, 4.0f, 4.0f, 4.0f)
		.VAlign(VAlign_Center)
		[
			SAssignNew(ComboAndButtonBox, SVerticalBox)
		];

		// Add combo box
		TSharedPtr<SComboButton> AssetComboButton;
		ComboAndButtonBox->AddSlot().FillHeight(1.0f).VAlign(VAlign_Center)
		[
			SNew(SVerticalBox) + SVerticalBox::Slot().FillHeight(1.0f).VAlign(VAlign_Center)
			[
				SAssignNew(AssetComboButton, SComboButton)
				.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(FEditorStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				/*.OnGetMenuContent(this, &FHoudiniOutputDetails::OnGetMaterialInterfaceMenuContent,
				MaterialInterface, StaticMesh, InOutput, MaterialIdx)*/
				.ContentPadding(2.0f)
				.ButtonContent()
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(FText::FromString(MaterialName))
				]
			]
		];


		TSharedPtr<SHorizontalBox> ButtonBox;
		ComboAndButtonBox->AddSlot().FillHeight(1.0f)
		[
			SAssignNew(ButtonBox, SHorizontalBox)
		];
		
		// Disable the combobutton for proxies
		AssetComboButton->SetEnabled(false);

		// Add use selection form content browser array
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeUseSelectedButton(
				/*FSimpleDelegate::CreateSP(
					this, &FHoudiniOutputDetails::OnUseContentBrowserSelectedMaterialInterface,
					(UObject*)ProxyMesh, InOutput, MaterialIdx),*/
					FSimpleDelegate::CreateLambda([]() {}), // Do nothing for proxies
					TAttribute< FText >(LOCTEXT("UseSelectedAssetFromContentBrowser", "Use Selected Asset from Content Browser")), false)
			// Disable the use CB selection button for proxies
		];
		

		// Create tooltip.
		FFormatNamedArguments Args;
		Args.Add(TEXT("Asset"), FText::FromString(MaterialName));
		FText MaterialTooltip = FText::Format(
			LOCTEXT("BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser"), Args);

		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			PropertyCustomizationHelpers::MakeBrowseButton(
				FSimpleDelegate::CreateSP(this, &FHoudiniOutputDetails::OnBrowseTo, (UObject*)MaterialInterface), TAttribute<FText>(MaterialTooltip))
		];

		/*
		ButtonBox->AddSlot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ToolTipText(LOCTEXT("ResetToBaseMaterial", "Reset to base material"))
			.ButtonStyle(FEditorStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility(EVisibility::Visible)
			.OnClicked(
				this, &FHoudiniOutputDetails::OnResetMaterialInterfaceClicked, StaticMesh, InOutput, MaterialIdx)
			[
				SNew(SImage)
				.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
			]
		];
		*/

		// Store combo button for this mesh and index.
		{
			TPairInitializer<UObject*, int32> Pair(ProxyMesh, MaterialIdx);
			MaterialInterfaceComboButtons.Add(Pair, AssetComboButton);
		}
	}
}

FText
FHoudiniOutputDetails::GetOutputDebugName(UHoudiniOutput* InOutput)
{
	// Get the name and type
	FString OutputNameStr = InOutput->GetName() + TEXT(" ") + UHoudiniOutput::OutputTypeToString(InOutput->GetType());

	// Then add the number of parts		
	OutputNameStr += TEXT(" (") + FString::FromInt(InOutput->GetHoudiniGeoPartObjects().Num()) + TEXT(" Part(s))\n");

	return FText::FromString(OutputNameStr);
}
FText
FHoudiniOutputDetails::GetOutputDebugDescription(UHoudiniOutput* InOutput)
{	
	const TArray<FHoudiniGeoPartObject>& HGPOs = InOutput->GetHoudiniGeoPartObjects();
	
	FString OutputValStr;
	OutputValStr += TEXT("HGPOs:\n");
	for (auto& HGPO : HGPOs)
	{
		OutputValStr += TEXT(" - ") + HGPO.PartName + TEXT(" (") + FHoudiniGeoPartObject::HoudiniPartTypeToString(HGPO.Type) + TEXT(")");

		if (HGPO.SplitGroups.Num() > 0)
		{
			OutputValStr += TEXT("( ") + FString::FromInt(HGPO.SplitGroups.Num()) + TEXT(" splits:");
			for (auto& split : HGPO.SplitGroups)
			{
				OutputValStr += TEXT(" ") + split;
			}
			OutputValStr += TEXT(")");
		}

		if (!HGPO.VolumeName.IsEmpty())
		{
			OutputValStr += TEXT("( ") + HGPO.VolumeName;
			if (HGPO.VolumeTileIndex >= 0)
				OutputValStr += TEXT(" tile ") + FString::FromInt(HGPO.VolumeTileIndex);
			OutputValStr += TEXT(" )");
		}

		OutputValStr += TEXT("\n");
	}

	// Add output objects if any
	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject> AllOutputObj = InOutput->GetOutputObjects();
	if (AllOutputObj.Num() > 0)
	{
		bool TitleAdded = false;
		for (const auto& Iter : AllOutputObj)
		{
			UObject* OutObject = Iter.Value.OutputObject;
			if (OutObject)
			{
				OutputValStr += OutObject->GetFullName() + TEXT(" (obj)\n");
			}
			
			UObject* OutComp = Iter.Value.OutputComponent;
			if (OutComp)
			{
				OutputValStr += OutObject->GetFullName() + TEXT(" (comp)\n");
			}
		}
	}

	return FText::FromString(OutputValStr);
}

FText
FHoudiniOutputDetails::GetOutputTooltip(UHoudiniOutput* InOutput)
{
	// TODO
	return FText();
}


const FSlateBrush *
FHoudiniOutputDetails::GetThumbnailBorder(UObject* Mesh) const
{
	TSharedPtr<SBorder> ThumbnailBorder = OutputObjectThumbnailBorders[Mesh];
	if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	else
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
}


const FSlateBrush *
FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder(UObject* OutputObject, int32 MaterialIdx) const
{
	if (!OutputObject)
		return nullptr;

	TPairInitializer<UObject*, int32> Pair(OutputObject, MaterialIdx);
	TSharedPtr<SBorder> ThumbnailBorder = MaterialInterfaceThumbnailBorders[Pair];

	if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	else
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
}

/*
const FSlateBrush *
FHoudiniOutputDetails::GetMaterialInterfaceThumbnailBorder(ALandscapeProxy * Landscape, int32 MaterialIdx) const
{
	if (!Landscape)
		return nullptr;

	TPairInitializer< ALandscapeProxy *, int32 > Pair(Landscape, MaterialIdx);
	TSharedPtr< SBorder > ThumbnailBorder = LandscapeMaterialInterfaceThumbnailBorders[Pair];

	if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
	else
		return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
}
*/

FReply
FHoudiniOutputDetails::OnThumbnailDoubleClick(
	const FGeometry & InMyGeometry,
	const FPointerEvent & InMouseEvent, UObject * Object)
{
	if (Object && GEditor)
		GEditor->EditObject(Object);

	return FReply::Handled();
}

/*
FReply
FHoudiniOutputDetails::OnBakeStaticMesh(UStaticMesh * StaticMesh, UHoudiniAssetComponent * HoudiniAssetComponent, FHoudiniGeoPartObject& GeoPartObject)
{
	if (HoudiniAssetComponent && StaticMesh && !HoudiniAssetComponent->IsPendingKill() && !StaticMesh->IsPendingKill())
	{
		FHoudiniPackageParams PackageParms;


		FHoudiniEngineBakeUtils::BakeStaticMesh(HoudiniAssetComponent, GeoPartObject, StaticMesh, PackageParms);
		// TODO: Bake the SM

		
		// We need to locate corresponding geo part object in component.
		const FHoudiniGeoPartObject& HoudiniGeoPartObject = HoudiniAssetComponent->LocateGeoPartObject(StaticMesh);

		// (void)FHoudiniEngineBakeUtils::DuplicateStaticMeshAndCreatePackage(
		//	StaticMesh, HoudiniAssetComponent, HoudiniGeoPartObject, EBakeMode::ReplaceExisitingAssets);
		
	}

	return FReply::Handled();
}
*/

bool
FHoudiniOutputDetails::OnMaterialInterfaceDraggedOver(const UObject * InObject) const
{
	return (InObject && InObject->IsA(UMaterialInterface::StaticClass()));
}


FReply
FHoudiniOutputDetails::OnResetMaterialInterfaceClicked(
	UStaticMesh * StaticMesh,
	UHoudiniOutput * HoudiniOutput,
	int32 MaterialIdx)
{
	FReply RetValue = FReply::Handled();
	if (!StaticMesh || StaticMesh->IsPendingKill())
		return RetValue;

	if (!StaticMesh->StaticMaterials.IsValidIndex(MaterialIdx))
		return RetValue;

	// Retrieve material interface which is being replaced.
	UMaterialInterface * MaterialInterface = StaticMesh->StaticMaterials[MaterialIdx].MaterialInterface;
	if (!MaterialInterface)
		return RetValue;

	// Find the string corresponding to the material that is being replaced	
	const FString* FoundString = HoudiniOutput->GetReplacementMaterials().FindKey(MaterialInterface);
	if (!FoundString )
	{
		// This material was not replaced, no need to reset it
		return RetValue;
	}

	// This material has been replaced previously.
	FString MaterialString = *FoundString;

	// Record a transaction for undo/redo
	FScopedTransaction Transaction(
		TEXT(HOUDINI_MODULE_EDITOR),
		LOCTEXT("HoudiniMaterialReplacement", "Houdini Material Reset"), HoudiniOutput);

	// Remove the replacement
	HoudiniOutput->Modify();
	HoudiniOutput->GetReplacementMaterials().Remove(MaterialString);

	bool bViewportNeedsUpdate = true;

	// Try to find the original assignment, if not, we'll use the default material
	UMaterialInterface * AssignMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
	UMaterialInterface * const * FoundMat = HoudiniOutput->GetAssignementMaterials().Find(MaterialString);
	if (FoundMat && (*FoundMat))
		AssignMaterial = *FoundMat;

	// Replace material on static mesh.
	StaticMesh->Modify();
	StaticMesh->StaticMaterials[MaterialIdx].MaterialInterface = AssignMaterial;

	// Replace the material on any component (SMC/ISMC) that uses the above SM
	// TODO: ?? Replace for all?
	for (auto& OutputObject : HoudiniOutput->GetOutputObjects())
	{
		// Only look at MeshComponents
		UStaticMeshComponent * SMC = Cast<UStaticMeshComponent>(OutputObject.Value.OutputComponent);
		if (!SMC)
			continue;

		if (SMC->GetStaticMesh() != StaticMesh)
			continue;

		SMC->Modify();
		SMC->SetMaterial(MaterialIdx, AssignMaterial);
	}

	FHoudiniEngineUtils::UpdateEditorProperties(HoudiniOutput->GetOuter(), true);

	if (GEditor)
		GEditor->RedrawAllViewports();

	return RetValue;
}

FReply
FHoudiniOutputDetails::OnResetMaterialInterfaceClicked(
	ALandscapeProxy* InLandscape,
	UHoudiniOutput * InHoudiniOutput,
	int32 InMaterialIdx)
{
	FReply RetValue = FReply::Handled();
	if (!InLandscape || InLandscape->IsPendingKill())
		return RetValue;
	
	// Retrieve the material interface which is being replaced.
	UMaterialInterface * MaterialInterface = InMaterialIdx == 0 ? InLandscape->GetLandscapeMaterial() : InLandscape->GetLandscapeHoleMaterial();
	UMaterialInterface * MaterialInterfaceReplacement = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());

	// Find the string corresponding to the material that is being replaced	
	const FString* FoundString = InHoudiniOutput->GetReplacementMaterials().FindKey(MaterialInterface);
	if (!FoundString)
	{
		// This material was not replaced, no need to reset it
		return RetValue;
	}

	// This material has been replaced previously.
	FString MaterialString = *FoundString;

	// Record a transaction for undo/redo
	FScopedTransaction Transaction(
		TEXT(HOUDINI_MODULE_EDITOR),
		LOCTEXT("HoudiniMaterialReplacement", "Houdini Material Reset"), InHoudiniOutput);

	// Remove the replacement
	InHoudiniOutput->Modify();
	InHoudiniOutput->GetReplacementMaterials().Remove(MaterialString);

	bool bViewportNeedsUpdate = true;

	// Try to find the original assignment, if not, we'll use the default material
	UMaterialInterface * AssignMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
	UMaterialInterface * const * FoundMat = InHoudiniOutput->GetAssignementMaterials().Find(MaterialString);
	if (FoundMat && (*FoundMat))
		AssignMaterial = *FoundMat;

	// Replace material on Landscape
	InLandscape->Modify();
	if (InMaterialIdx == 0)
		InLandscape->LandscapeMaterial = AssignMaterial;
	else
		InLandscape->LandscapeHoleMaterial = AssignMaterial;
	
	InLandscape->UpdateAllComponentMaterialInstances();

	/*
	// As UpdateAllComponentMaterialInstances() is not accessible to us, we'll try to access the Material's UProperty 
	// to trigger a fake Property change event that will call the Update function...
	UProperty* FoundProperty = FindField< UProperty >(Landscape->GetClass(), (MaterialIdx == 0) ? TEXT("LandscapeMaterial") : TEXT("LandscapeHoleMaterial"));
	if (FoundProperty)
	{
		FPropertyChangedEvent PropChanged(FoundProperty, EPropertyChangeType::ValueSet);
		Landscape->PostEditChangeProperty(PropChanged);
	}
	else
	{
		// The only way to update the material for now is to recook/recreate the landscape...
		HoudiniAssetComponent->StartTaskAssetCookingManual();
	}
	*/

	FHoudiniEngineUtils::UpdateEditorProperties(InHoudiniOutput->GetOuter(), true);

	if (GEditor)
		GEditor->RedrawAllViewports();

	return RetValue;
}
/*
FReply
FHoudiniOutputDetails::OnResetMaterialInterfaceClicked(
	ALandscapeProxy * Landscape, UHoudiniOutput * InOutput, int32 MaterialIdx)
{
	bool bViewportNeedsUpdate = false;

	// TODO: Handle me!
	for (TArray< UHoudiniAssetComponent * >::TIterator
		IterComponents(HoudiniAssetComponents); IterComponents; ++IterComponents)
	{
		UHoudiniAssetComponent * HoudiniAssetComponent = *IterComponents;
		if (!HoudiniAssetComponent)
			continue;

		TWeakObjectPtr<ALandscapeProxy>* FoundLandscapePtr = HoudiniAssetComponent->LandscapeComponents.Find(*HoudiniGeoPartObject);
		if (!FoundLandscapePtr)
			continue;

		ALandscapeProxy* FoundLandscape = FoundLandscapePtr->Get();
		if (!FoundLandscape || !FoundLandscape->IsValidLowLevel())
			continue;

		if (FoundLandscape != Landscape)
			continue;

		// Retrieve the material interface which is being replaced.
		UMaterialInterface * MaterialInterface = MaterialIdx == 0 ? Landscape->GetLandscapeMaterial() : Landscape->GetLandscapeHoleMaterial();
		UMaterialInterface * MaterialInterfaceReplacement = Cast<UMaterialInterface>(FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get());

		bool bMaterialRestored = false;
		FString MaterialShopName;
		if (!HoudiniAssetComponent->GetReplacementMaterialShopName(*HoudiniGeoPartObject, MaterialInterface, MaterialShopName))
		{
			// This material was not replaced so there's no need to reset it
			continue;
		}

		// Remove the replacement
		HoudiniAssetComponent->RemoveReplacementMaterial(*HoudiniGeoPartObject, MaterialShopName);

		// Try to find the original assignment, if not, we'll use the default material
		UMaterialInterface * AssignedMaterial = HoudiniAssetComponent->GetAssignmentMaterial(MaterialShopName);
		if (AssignedMaterial)
			MaterialInterfaceReplacement = AssignedMaterial;

		// Replace material on the landscape
		Landscape->Modify();

		if (MaterialIdx == 0)
			Landscape->LandscapeMaterial = MaterialInterfaceReplacement;
		else
			Landscape->LandscapeHoleMaterial = MaterialInterfaceReplacement;

		//Landscape->UpdateAllComponentMaterialInstances();

		// As UpdateAllComponentMaterialInstances() is not accessible to us, we'll try to access the Material's UProperty 
		// to trigger a fake Property change event that will call the Update function...
		UProperty* FoundProperty = FindField< UProperty >(Landscape->GetClass(), (MaterialIdx == 0) ? TEXT("LandscapeMaterial") : TEXT("LandscapeHoleMaterial"));
		if (FoundProperty)
		{
			FPropertyChangedEvent PropChanged(FoundProperty, EPropertyChangeType::ValueSet);
			Landscape->PostEditChangeProperty(PropChanged);
		}
		else
		{
			// The only way to update the material for now is to recook/recreate the landscape...
			HoudiniAssetComponent->StartTaskAssetCookingManual();
		}

		HoudiniAssetComponent->UpdateEditorProperties(false);
		bViewportNeedsUpdate = true;
	}

	if (GEditor && bViewportNeedsUpdate)
	{
		GEditor->RedrawAllViewports();
	}

	return FReply::Handled();
}
*/

void
FHoudiniOutputDetails::OnBrowseTo(UObject* InObject)
{
	if (GEditor)
	{
		TArray<UObject *> Objects;
		Objects.Add(InObject);
		GEditor->SyncBrowserToObjects(Objects);
	}
}

TSharedRef<SWidget>
FHoudiniOutputDetails::OnGetMaterialInterfaceMenuContent(
	UMaterialInterface* MaterialInterface,
	UObject* OutputObject,
	UHoudiniOutput* InOutput,
	int32 MaterialIdx)
{
	TArray<const UClass *> AllowedClasses;
	AllowedClasses.Add(UMaterialInterface::StaticClass());

	TArray<UFactory *> NewAssetFactories;

	return PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
		FAssetData(MaterialInterface),
		true,
		AllowedClasses,
		NewAssetFactories,
		OnShouldFilterMaterialInterface,
		FOnAssetSelected::CreateSP(
			this, &FHoudiniOutputDetails::OnMaterialInterfaceSelected, OutputObject, InOutput, MaterialIdx),
		FSimpleDelegate::CreateSP(
			this, &FHoudiniOutputDetails::CloseMaterialInterfaceComboButton));
}


void
FHoudiniOutputDetails::CloseMaterialInterfaceComboButton()
{

}

void
FHoudiniOutputDetails::OnMaterialInterfaceDropped(
	UObject * InObject,
	UStaticMesh * StaticMesh,
	UHoudiniOutput * HoudiniOutput,
	int32 MaterialIdx)
{
	UMaterialInterface * MaterialInterface = Cast<UMaterialInterface>(InObject);
	if (!MaterialInterface || MaterialInterface->IsPendingKill())
		return;

	if (!StaticMesh || StaticMesh->IsPendingKill())
		return;

	if (!StaticMesh->StaticMaterials.IsValidIndex(MaterialIdx))
		return;

	bool bViewportNeedsUpdate = false;

	// Retrieve material interface which is being replaced.
	UMaterialInterface * OldMaterialInterface = StaticMesh->StaticMaterials[MaterialIdx].MaterialInterface;
	if (OldMaterialInterface == MaterialInterface)
		return;

	// Find the string corresponding to the material that is being replaced
	FString MaterialString = FString();
	const FString* FoundString = HoudiniOutput->GetReplacementMaterials().FindKey(OldMaterialInterface);
	if (FoundString)
	{
		// This material has been replaced previously.
		MaterialString = *FoundString;
	}
	else
	{
		// We have no previous replacement for this material,
		// see if we can find it the material assignment list.
		FoundString = HoudiniOutput->GetAssignementMaterials().FindKey(OldMaterialInterface);
		if (FoundString)
		{
			// This material has been assigned previously.
			MaterialString = *FoundString;
		}
		else
		{
			UMaterialInterface * DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
			if (OldMaterialInterface == DefaultMaterial)
			{
				// This is replacement for default material.
				MaterialString = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
			}
			else
			{
				// External Material?
				MaterialString = OldMaterialInterface->GetName();
			}
		}
	}

	if (MaterialString.IsEmpty())
		return;

	// Record a transaction for undo/redo
	FScopedTransaction Transaction(
		TEXT(HOUDINI_MODULE_EDITOR),
		LOCTEXT("HoudiniMaterialReplacement", "Houdini Material Replacement"), HoudiniOutput);

	// Add a new material replacement entry.
	HoudiniOutput->Modify(); 
	HoudiniOutput->GetReplacementMaterials().Add(MaterialString, MaterialInterface);	

	// Replace material on static mesh.
	StaticMesh->Modify();
	StaticMesh->StaticMaterials[MaterialIdx].MaterialInterface = MaterialInterface;

	// Replace the material on any component (SMC/ISMC) that uses the above SM
	for (auto& OutputObject : HoudiniOutput->GetOutputObjects())
	{
		// Only look at MeshComponents
		UStaticMeshComponent * SMC = Cast<UStaticMeshComponent>(OutputObject.Value.OutputComponent);
		if (SMC && !SMC->IsPendingKill())
		{
			if (SMC->GetStaticMesh() == StaticMesh)
			{
				SMC->Modify();
				SMC->SetMaterial(MaterialIdx, MaterialInterface);
			}
		}
		else 
		{
			UStaticMesh* SM = Cast<UStaticMesh>(OutputObject.Value.OutputObject);
			if (SM && !SM->IsPendingKill()) 
			{
				SM->Modify();
				SM->SetMaterial(MaterialIdx, MaterialInterface);
			}
		}



	}

	FHoudiniEngineUtils::UpdateEditorProperties(HoudiniOutput->GetOuter(), true);

	/*
	if(GUnrealEd)
		GUnrealEd->UpdateFloatingPropertyWindows();
*/
	if (GEditor)
		GEditor->RedrawAllViewports();
}

// Delegate used when a valid material has been drag and dropped on a landscape.
void
FHoudiniOutputDetails::OnMaterialInterfaceDropped(
	UObject* InDroppedObject,
	ALandscapeProxy* InLandscape,
	UHoudiniOutput* InOutput,
	int32 MaterialIdx)
{
	UMaterialInterface * MaterialInterface = Cast< UMaterialInterface >(InDroppedObject);
	if (!MaterialInterface || MaterialInterface->IsPendingKill())
		return;

	if (!InLandscape || InLandscape->IsPendingKill())
		return;

	bool bViewportNeedsUpdate = false;

	// Retrieve the material interface which is being replaced.
	UMaterialInterface * OldMaterialInterface = MaterialIdx == 0 ? InLandscape->GetLandscapeMaterial() : InLandscape->GetLandscapeHoleMaterial();
	if (OldMaterialInterface == MaterialInterface)
		return;

	// Find the string corresponding to the material that is being replaced
	FString MaterialString = FString();
	const FString* FoundString = InOutput->GetReplacementMaterials().FindKey(OldMaterialInterface);
	if (FoundString)
	{
		// This material has been replaced previously.
		MaterialString = *FoundString;
	}
	else
	{
		// We have no previous replacement for this material,
		// see if we can find it the material assignment list.
		FoundString = InOutput->GetAssignementMaterials().FindKey(OldMaterialInterface);
		if (FoundString)
		{
			// This material has been assigned previously.
			MaterialString = *FoundString;
		}
		else
		{
			UMaterialInterface * DefaultMaterial = FHoudiniEngine::Get().GetHoudiniDefaultMaterial().Get();
			if (OldMaterialInterface == DefaultMaterial)
			{
				// This is replacement for default material.
				MaterialString = HAPI_UNREAL_DEFAULT_MATERIAL_NAME;
			}
			else
			{
				// External Material?
				if (OldMaterialInterface && !OldMaterialInterface->IsPendingKill())
					MaterialString = OldMaterialInterface->GetName();
			}
		}
	}

	if (MaterialString.IsEmpty())
		return;

	// Record a transaction for undo/redo
	FScopedTransaction Transaction(
		TEXT(HOUDINI_MODULE_EDITOR),
		LOCTEXT("HoudiniMaterialReplacement", "Houdini Material Replacement"), InOutput);

	// Add a new material replacement entry.
	InOutput->Modify();
	InOutput->GetReplacementMaterials().Add(MaterialString, MaterialInterface);

	// Replace material on the landscape
	InLandscape->Modify();

	if (MaterialIdx == 0)
		InLandscape->LandscapeMaterial = MaterialInterface;
	else
		InLandscape->LandscapeHoleMaterial = MaterialInterface;

	// Update the landscape components Material instances
	InLandscape->UpdateAllComponentMaterialInstances();
	
	/*
	// As UpdateAllComponentMaterialInstances() is not accessible to us, we'll try to access the Material's UProperty 
	// to trigger a fake Property change event that will call the Update function...
	UProperty* FoundProperty = FindField< UProperty >(InLandscape->GetClass(), (MaterialIdx == 0) ? TEXT("LandscapeMaterial") : TEXT("LandscapeHoleMaterial"));
	if (FoundProperty)
	{
		FPropertyChangedEvent PropChanged(FoundProperty, EPropertyChangeType::ValueSet);
		InLandscape->PostEditChangeProperty(PropChanged);
	}
	else
	{
		// The only way to update the material for now is to recook/recreate the landscape...
		HoudiniAssetComponent->StartTaskAssetCookingManual();
	}
	*/

	FHoudiniEngineUtils::UpdateEditorProperties(InOutput->GetOuter(), true);

	if (GEditor)
		GEditor->RedrawAllViewports();
}

void
FHoudiniOutputDetails::OnMaterialInterfaceSelected(
	const FAssetData & AssetData,
	UObject* OutputObject,
	UHoudiniOutput * InOutput,
	int32 MaterialIdx)
{
	TPairInitializer<UObject*, int32> Pair(OutputObject, MaterialIdx);
	TSharedPtr<SComboButton> AssetComboButton = MaterialInterfaceComboButtons[Pair];
	if (AssetComboButton.IsValid())
	{
		AssetComboButton->SetIsOpen(false);

		UObject * Object = AssetData.GetAsset();

		UStaticMesh* SM = Cast<UStaticMesh>(OutputObject);
		if (SM && !SM->IsPendingKill())
		{
			return OnMaterialInterfaceDropped(Object, SM, InOutput, MaterialIdx);
		}

		ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(OutputObject);
		if (Landscape && !Landscape->IsPendingKill())
		{
			return OnMaterialInterfaceDropped(Object, Landscape, InOutput, MaterialIdx);
		}		
	}
}

void 
FHoudiniOutputDetails::OnUseContentBrowserSelectedMaterialInterface(
	UObject* OutputObject,
	UHoudiniOutput * InOutput,
	int32 MaterialIdx) 
{
	if (!OutputObject || OutputObject->IsPendingKill())
		return;

	if (!InOutput || InOutput->IsPendingKill())
		return;

	if (GEditor)
	{
		TArray<FAssetData> CBSelections;
		GEditor->GetContentBrowserSelections(CBSelections);

		// Get the first selected material object
		UObject* Object = nullptr;
		for (auto & CurAssetData : CBSelections)
		{
			if (CurAssetData.AssetClass != UMaterial::StaticClass()->GetFName() &&
				CurAssetData.AssetClass != UMaterialInstance::StaticClass()->GetFName() &&
				CurAssetData.AssetClass != UMaterialInstanceConstant::StaticClass()->GetFName())
				continue;

			Object = CurAssetData.GetAsset();
			break;
		}

		if (Object && !Object->IsPendingKill())
		{
			UStaticMesh* SM = Cast<UStaticMesh>(OutputObject);
			if (SM && !SM->IsPendingKill())
			{
				return OnMaterialInterfaceDropped(Object, SM, InOutput, MaterialIdx);
			}

			ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(OutputObject);
			if (Landscape && !Landscape->IsPendingKill())
			{
				return OnMaterialInterfaceDropped(Object, Landscape, InOutput, MaterialIdx);
			}
		}
	}
}

void
FHoudiniOutputDetails::CreateInstancerOutputWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	UHoudiniOutput* InOutput)
{
	if (!InOutput || InOutput->IsPendingKill())
		return;

	// Do not display instancer UI for one-instance instancers
	bool OnlyOneInstanceInstancers = true;
	for (auto& Iter : InOutput->GetInstancedOutputs())
	{		
		FHoudiniInstancedOutput& CurInstanceOutput = (Iter.Value);
		if (CurInstanceOutput.OriginalTransforms.Num() <= 1)
			continue;
		
		OnlyOneInstanceInstancers = false;
		break;
	}

	// This output only has one-instance instancers (SMC), no need to display the instancer UI.
	if (OnlyOneInstanceInstancers)
		return;

	// Classes allowed for instance variations.
	const TArray<const UClass *> AllowedClasses = 
	{
		UStaticMesh::StaticClass(), USkeletalMesh::StaticClass(),
		AActor::StaticClass(), UBlueprint::StaticClass(),
		UFXSystemAsset::StaticClass(), USoundBase::StaticClass()
	};

	// Classes not allowed for instances variations (useless?)
	TArray<const UClass *> DisallowedClasses =
	{
		UClass::StaticClass(), ULevel::StaticClass(), 
		UMaterial::StaticClass(), UTexture::StaticClass()
	};
	
	IDetailLayoutBuilder & DetailLayoutBuilder = HouOutputCategory.GetParentLayout();
	TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = DetailLayoutBuilder.GetThumbnailPool();

	// Lambda for adding new variation objects
	auto AddObjectAt = [InOutput](FHoudiniInstancedOutput& InOutputToUpdate, const int32& AtIndex, UObject* InObject)
	{	
		// TODO: undo/redo?
		InOutputToUpdate.VariationObjects.Insert(InObject, AtIndex);
		InOutputToUpdate.VariationTransformOffsets.Insert(FTransform::Identity, AtIndex);
		FHoudiniInstanceTranslator::UpdateVariationAssignements(InOutputToUpdate);

		InOutputToUpdate.MarkChanged(true);

		FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
	};

	// Lambda for adding new geometry input objects
	auto RemoveObjectAt = [InOutput](FHoudiniInstancedOutput& InOutputToUpdate, const int32& AtIndex)
	{
		// Also keep one instance object
		if (AtIndex < 0 || AtIndex >= InOutputToUpdate.VariationObjects.Num())
			return;

		if (InOutputToUpdate.VariationObjects.Num() == 1)
			return;

		// TODO: undo/redo?
		InOutputToUpdate.VariationObjects.RemoveAt(AtIndex);
		InOutputToUpdate.VariationTransformOffsets.RemoveAt( AtIndex);
		FHoudiniInstanceTranslator::UpdateVariationAssignements(InOutputToUpdate);

		InOutputToUpdate.MarkChanged(true);

		FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
	};

	// Lambda for updating a variation
	auto SetObjectAt = [InOutput](FHoudiniInstancedOutput& InOutputToUpdate, const int32& AtIndex, UObject* InObject)
	{
		if (!InOutputToUpdate.VariationObjects.IsValidIndex(AtIndex))
			return;

		InOutputToUpdate.VariationObjects[AtIndex] = InObject;

		InOutputToUpdate.MarkChanged(true);

		FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
	};

	// Lambda for changing the transform offset values
	auto ChangeTransformOffsetAt = [InOutput](
		FHoudiniInstancedOutput& InOutputToUpdate, const int32& AtIndex, 
		const float& Value,  const int32& PosRotScaleIndex, const int32& XYZIndex)
	{
		bool bChanged = InOutputToUpdate.SetTransformOffsetAt(Value, AtIndex, PosRotScaleIndex, XYZIndex);
		if (!bChanged)
			return;

		InOutputToUpdate.MarkChanged(true);

		if (GEditor)
			GEditor->RedrawAllViewports();

		FHoudiniEngineUtils::UpdateEditorProperties(InOutput, true);
	};

	// Get this output's OutputObject
	const TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();

	// Iterate on all of the output's HGPO
	for (const FHoudiniGeoPartObject& CurHGPO : InOutput->GetHoudiniGeoPartObjects())
	{
		// Not an instancer, skip
		if (CurHGPO.Type != EHoudiniPartType::Instancer)
			continue;

		// Get the label for that instancer
		FString InstancerLabel = InOutput->GetName() + TEXT(" ") + UHoudiniOutput::OutputTypeToString(InOutput->GetType());
		if (CurHGPO.bHasCustomPartName)
			InstancerLabel = CurHGPO.PartName;

		TSharedRef<SVerticalBox> InstancerVerticalBox = SNew(SVerticalBox);
		TSharedPtr<SHorizontalBox> InstancerHorizontalBox = nullptr;

		// Create a new Group for that instancer
		IDetailGroup& InstancerGroup = HouOutputCategory.AddGroup(FName(*InstancerLabel), FText::FromString(InstancerLabel));

		// Now iterate and display the instance outputs that matches this HGPO
		for (auto& Iter : InOutput->GetInstancedOutputs())
		{
			FHoudiniOutputObjectIdentifier& CurOutputObjectIdentifier = Iter.Key;
			if (!CurOutputObjectIdentifier.Matches(CurHGPO))
				continue;

			FHoudiniInstancedOutput& CurInstanceOutput = (Iter.Value);
			
			// Dont display instancer UI for one-instance instancers (SMC)
			if (CurInstanceOutput.OriginalTransforms.Num() <= 1)
				continue;

			for( int32 VariationIdx = 0; VariationIdx < CurInstanceOutput.VariationObjects.Num(); VariationIdx++ )
			{
				UObject * InstancedObject = CurInstanceOutput.VariationObjects[VariationIdx].LoadSynchronous();
				if ( !InstancedObject || InstancedObject->IsPendingKill() )
				{
					HOUDINI_LOG_WARNING( TEXT("Null Object found for instance variation %d"), VariationIdx );
					continue;
				}

				// Create thumbnail for this object.
				TSharedPtr<FAssetThumbnail> VariationThumbnail =
					MakeShareable(new FAssetThumbnail(InstancedObject, 64, 64, AssetThumbnailPool));
				TSharedRef<SVerticalBox> PickerVerticalBox = SNew(SVerticalBox);
				TSharedPtr<SHorizontalBox> PickerHorizontalBox = nullptr;
				TSharedPtr<SBorder> VariationThumbnailBorder;

				// For the variation name, reuse the instancer label and append the variation index if we have more than one variation
				FString InstanceOutputLabel = InstancerLabel;
				if(CurInstanceOutput.VariationObjects.Num() > 1)
					InstanceOutputLabel += TEXT(" [") + FString::FromInt(VariationIdx) + TEXT("]");

				IDetailGroup* DetailGroup = &InstancerGroup;
				if (CurInstanceOutput.VariationObjects.Num() > 1)
				{
					// If we have more than one variation, add a new group for each variation
					DetailGroup = &InstancerGroup.AddGroup(FName(*InstanceOutputLabel), FText::FromString(InstanceOutputLabel), true);
				}

				// See if we can find the corresponding component to get its type
				FString InstancerType = TEXT("(Instancer)"); 
				FHoudiniOutputObjectIdentifier CurVariationIdentifier = CurOutputObjectIdentifier;
				CurVariationIdentifier.SplitIdentifier += TEXT("_") + FString::FromInt(VariationIdx);
				const FHoudiniOutputObject* VariationOutputObject = OutputObjects.Find(CurVariationIdentifier);
				if(VariationOutputObject)
					InstancerType = FHoudiniInstanceTranslator::GetInstancerTypeFromComponent(VariationOutputObject->OutputComponent);

				DetailGroup->AddWidgetRow()
				.NameContent()
				[
					//SNew(SSpacer)
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
					.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
					.Text(FText::FromString(InstancerType))
					//.Size(FVector2D(250, 64))
				]
				.ValueContent()
				.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				[
					PickerVerticalBox
				];

				// Add an asset drop target
				PickerVerticalBox->AddSlot()
				.Padding(0, 2)
				.AutoHeight()
				[
					SNew(SAssetDropTarget)
					.OnIsAssetAcceptableForDrop(SAssetDropTarget::FIsAssetAcceptableForDrop::CreateLambda( 
						[DisallowedClasses](const UObject* Obj) 
						{
							for (auto Klass : DisallowedClasses)
							{
								if (Obj && Obj->IsA(Klass))
									return false;
							}
							return true;
						})
					)
					.OnAssetDropped_Lambda([&CurInstanceOutput, VariationIdx, SetObjectAt](UObject* InObject)
					{
						return SetObjectAt(CurInstanceOutput, VariationIdx, InObject);
					})
					[
						SAssignNew(PickerHorizontalBox, SHorizontalBox)
					]
				];

				PickerHorizontalBox->AddSlot().Padding(0.0f, 0.0f, 2.0f, 0.0f).AutoWidth()
				[
					SAssignNew(VariationThumbnailBorder, SBorder)
					.Padding( 5.0f )
					.OnMouseDoubleClick(this, &FHoudiniOutputDetails::OnThumbnailDoubleClick, InstancedObject)
					[
						SNew(SBox)
						.WidthOverride(64)
						.HeightOverride(64)
						.ToolTipText(FText::FromString(InstancedObject->GetPathName()))
						[
							VariationThumbnail->MakeThumbnailWidget()
						]
					]
				];

				TWeakPtr<SBorder> WeakVariationThumbnailBorder(VariationThumbnailBorder);
				VariationThumbnailBorder->SetBorderImage(TAttribute< const FSlateBrush *>::Create(
					TAttribute<const FSlateBrush *>::FGetter::CreateLambda([WeakVariationThumbnailBorder]()
					{
						TSharedPtr<SBorder> ThumbnailBorder = WeakVariationThumbnailBorder.Pin();
						if (ThumbnailBorder.IsValid() && ThumbnailBorder->IsHovered())
							return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailLight");
						else
							return FEditorStyle::GetBrush("PropertyEditor.AssetThumbnailShadow");
					}
				)));

				PickerHorizontalBox->AddSlot().AutoWidth().Padding(0.0f, 28.0f, 0.0f, 28.0f)
				[
					PropertyCustomizationHelpers::MakeAddButton(
						FSimpleDelegate::CreateLambda([&CurInstanceOutput, VariationIdx, AddObjectAt]()
						{				
							UObject* ObjToAdd = CurInstanceOutput.VariationObjects.IsValidIndex(VariationIdx) ?
								CurInstanceOutput.VariationObjects[VariationIdx].LoadSynchronous()
								: nullptr;

							return AddObjectAt(CurInstanceOutput, VariationIdx, ObjToAdd);
						}),
						LOCTEXT("AddAnotherInstanceToolTip", "Add Another Instance"))
				];

				PickerHorizontalBox->AddSlot().AutoWidth().Padding( 2.0f, 28.0f, 4.0f, 28.0f )
				[
					PropertyCustomizationHelpers::MakeRemoveButton(
						FSimpleDelegate::CreateLambda([&CurInstanceOutput, VariationIdx, RemoveObjectAt]()
						{
							return RemoveObjectAt(CurInstanceOutput, VariationIdx);
						}),
						LOCTEXT("RemoveLastInstanceToolTip", "Remove Last Instance"))
				];

				TSharedPtr<SComboButton> AssetComboButton;
				TSharedPtr<SHorizontalBox> ButtonBox;
				PickerHorizontalBox->AddSlot()
				.FillWidth(1.0f)
				.Padding(0.0f, 4.0f, 4.0f, 4.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					[
						SAssignNew(ButtonBox, SHorizontalBox)
						+SHorizontalBox::Slot()
						[
							SAssignNew(AssetComboButton, SComboButton)
							//.ToolTipText( this, &FHoudiniAssetComponentDetails::OnGetToolTip )
							.ButtonStyle(FEditorStyle::Get(), "PropertyEditor.AssetComboStyle")
							.ForegroundColor( FEditorStyle::GetColor( "PropertyEditor.AssetName.ColorAndOpacity" ) )
							/* TODO: Update UI
							.OnMenuOpenChanged( FOnIsOpenChanged::CreateUObject(
								&InParam, &UHoudiniAssetInstanceInput::ChangedStaticMeshComboButton,
								CurInstanceOutput, InstOutIdx, VariationIdx ) )
								*/
							.ContentPadding(2.0f)
							.ButtonContent()
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "PropertyEditor.AssetClass")
								.Font(FEditorStyle::GetFontStyle(FName(TEXT("PropertyWindow.NormalFont"))))
								.Text(FText::FromString(InstancedObject->GetName()))
							]
						]
					]
				];

				// Create asset picker for this combo button.
				{
					TWeakPtr<SComboButton> WeakAssetComboButton(AssetComboButton);
					TArray<UFactory *> NewAssetFactories;
					TSharedRef<SWidget> PropertyMenuAssetPicker = PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
						FAssetData(InstancedObject),
						true,
						AllowedClasses,
						DisallowedClasses,
						NewAssetFactories,
						FOnShouldFilterAsset(),
						FOnAssetSelected::CreateLambda(
							[&CurInstanceOutput, VariationIdx, SetObjectAt, WeakAssetComboButton](const FAssetData& AssetData)
							{
								TSharedPtr<SComboButton> AssetComboButtonPtr = WeakAssetComboButton.Pin();
								if (AssetComboButtonPtr.IsValid())
								{
									AssetComboButtonPtr->SetIsOpen(false);
									UObject * Object = AssetData.GetAsset();
									SetObjectAt(CurInstanceOutput, VariationIdx, Object);
								}
							}
						),
						// Nothing to do on close
						FSimpleDelegate::CreateLambda([](){})
					);

					AssetComboButton->SetMenuContent(PropertyMenuAssetPicker);
				}

				// Create tooltip.
				FFormatNamedArguments Args;
				Args.Add(TEXT("Asset"), FText::FromString(InstancedObject->GetName()));
				FText StaticMeshTooltip =
					FText::Format(LOCTEXT( "BrowseToSpecificAssetInContentBrowser", "Browse to '{Asset}' in Content Browser" ), Args);

				ButtonBox->AddSlot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeBrowseButton(
						FSimpleDelegate::CreateLambda([&CurInstanceOutput, VariationIdx]()
						{
							UObject* InputObject = CurInstanceOutput.VariationObjects.IsValidIndex(VariationIdx) ?
								CurInstanceOutput.VariationObjects[VariationIdx].LoadSynchronous()
								: nullptr;

							if (GEditor && InputObject)
							{
								TArray<UObject*> Objects;
								Objects.Add(InputObject);
								GEditor->SyncBrowserToObjects(Objects);
							}
						}),
						TAttribute< FText >( StaticMeshTooltip ) )
				];

				ButtonBox->AddSlot()
				.AutoWidth()
				.Padding(2.0f, 0.0f )
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ToolTipText(LOCTEXT( "ResetToBase", "Reset to default static mesh"))
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.ContentPadding(0)
					.Visibility(EVisibility::Visible)
					.OnClicked_Lambda([SetObjectAt, &CurInstanceOutput, VariationIdx]()
					{
						SetObjectAt(CurInstanceOutput, VariationIdx, CurInstanceOutput.OriginalObject.LoadSynchronous());
						return FReply::Handled();
					})
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
					]
				];


				// Get Visibility of reset buttons
				bool bResetButtonVisiblePosition = false;
				bool bResetButtonVisibleRotation = false;
				bool bResetButtonVisibleScale = false;

				FTransform CurTransform = CurInstanceOutput.VariationTransformOffsets[VariationIdx];

				if (CurTransform.GetLocation() != FVector::ZeroVector)
					bResetButtonVisiblePosition = true;

				FRotator Rotator = CurTransform.Rotator();
				if (Rotator.Roll != 0 || Rotator.Pitch != 0 || Rotator.Yaw != 0)
					bResetButtonVisibleRotation = true;

				if (CurTransform.GetScale3D() != FVector::OneVector)
					bResetButtonVisibleScale = true;
				
				auto ChangeTransformOffsetUniformlyAt = [ChangeTransformOffsetAt, VariationIdx, &CurInstanceOutput](const float& Val, const int32& PosRotScaleIndex)
				{
					ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, PosRotScaleIndex, 0);
					ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, PosRotScaleIndex, 1);
					ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, PosRotScaleIndex, 2);
				};

				TSharedRef<SVerticalBox> OffsetVerticalBox = SNew(SVerticalBox);
				FText LabelPositionText = LOCTEXT("HoudiniPositionOffset", "Position Offset");
				DetailGroup->AddWidgetRow()
				.NameContent()
				[
					SNew(STextBlock)
					.Text(LabelPositionText)
					.ToolTipText(LabelPositionText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.ValueContent()
				.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SNew(SVectorInputBox)
						.bColorAxisLabels(true)
						.AllowSpin(true)
						.X(TAttribute<TOptional<float>>::Create(
							TAttribute<TOptional<float>>::FGetter::CreateLambda([&CurInstanceOutput, VariationIdx]()
								{ return CurInstanceOutput.GetTransformOffsetAt(VariationIdx, 0, 0); }
						)))
						.Y(TAttribute<TOptional<float>>::Create(
							TAttribute<TOptional<float>>::FGetter::CreateLambda([&CurInstanceOutput, VariationIdx]()
								{ return CurInstanceOutput.GetTransformOffsetAt(VariationIdx, 0, 1); }
						)))
						.Z(TAttribute<TOptional<float>>::Create(
							TAttribute<TOptional<float>>::FGetter::CreateLambda([&CurInstanceOutput, VariationIdx]()
								{ return CurInstanceOutput.GetTransformOffsetAt(VariationIdx, 0, 2); }
						)))
						.OnXCommitted_Lambda([&CurInstanceOutput, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{ ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, 0, 0); })	
						.OnYCommitted_Lambda([&CurInstanceOutput, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{ ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, 0, 1); })
						.OnZCommitted_Lambda([&CurInstanceOutput, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{ ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, 0, 2); })
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					[
						// Lock Button (not visible)
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.Visibility(EVisibility::Hidden)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("GenericLock"))
							]
						]
						// Reset Button
						+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.ToolTipText(LOCTEXT("InstancerOutputResetButtonToolTip", "Reset To Default"))
							.Visibility(bResetButtonVisiblePosition ? EVisibility::Visible : EVisibility::Hidden)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
							]
							.OnClicked_Lambda([ChangeTransformOffsetUniformlyAt, CurInstanceOutput, InOutput]()
							{
								ChangeTransformOffsetUniformlyAt(0.0f, 0);
								FHoudiniEngineUtils::UpdateEditorProperties(InOutput->GetOuter(), true);
								return FReply::Handled();
							})
						]
					]
				];

				FText LabelRotationText = LOCTEXT("HoudiniRotationOffset", "Rotation Offset");
				DetailGroup->AddWidgetRow()
				.NameContent()
				[
					SNew(STextBlock)
					.Text(LabelRotationText)
					.ToolTipText(LabelRotationText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.ValueContent()
				.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SNew(SRotatorInputBox)
						.AllowSpin(true)
						.bColorAxisLabels(true)                    
						.Roll(TAttribute<TOptional<float>>::Create(
							TAttribute<TOptional<float>>::FGetter::CreateLambda([&CurInstanceOutput, VariationIdx]()
								{ return CurInstanceOutput.GetTransformOffsetAt(VariationIdx, 1, 0); }
						)))
						.Pitch(TAttribute<TOptional<float>>::Create(
							TAttribute<TOptional<float>>::FGetter::CreateLambda([&CurInstanceOutput, VariationIdx]()
								{ return CurInstanceOutput.GetTransformOffsetAt(VariationIdx, 1, 1); }
						)))
						.Yaw(TAttribute<TOptional<float>>::Create(
							TAttribute<TOptional<float>>::FGetter::CreateLambda([&CurInstanceOutput, VariationIdx]()
								{ return CurInstanceOutput.GetTransformOffsetAt(VariationIdx, 1, 2); }
						)))
						.OnRollCommitted_Lambda([&CurInstanceOutput, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{ ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, 1, 0); })	
						.OnPitchCommitted_Lambda([&CurInstanceOutput, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{ ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, 1, 1); })
						.OnYawCommitted_Lambda([&CurInstanceOutput, VariationIdx, ChangeTransformOffsetAt](float Val, ETextCommit::Type TextCommitType)
							{ ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, 1, 2); })
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					[
						// Lock Button (not visible)
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.Visibility(EVisibility::Hidden)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("GenericLock"))
							]
						]
						// Reset Button
						+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
							.Visibility(bResetButtonVisibleRotation ? EVisibility::Visible : EVisibility::Hidden)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
							]
							.OnClicked_Lambda([ChangeTransformOffsetUniformlyAt, InOutput]()
							{
								ChangeTransformOffsetUniformlyAt(0.0f, 1);
								FHoudiniEngineUtils::UpdateEditorProperties(InOutput->GetOuter(), true);
								return FReply::Handled();
							})
						]
					]
				];

				FText LabelScaleText = LOCTEXT("HoudiniScaleOffset", "Scale Offset");
				DetailGroup->AddWidgetRow()
				.NameContent()
				[
					SNew(STextBlock)
					.Text(LabelScaleText)
					.ToolTipText(LabelScaleText)
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.ValueContent()
				.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().MaxWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH)
					[
						SNew(SVectorInputBox)
						.bColorAxisLabels(true)
						.X(TAttribute<TOptional<float>>::Create(
							TAttribute<TOptional<float>>::FGetter::CreateLambda([&CurInstanceOutput, VariationIdx]()
								{ return CurInstanceOutput.GetTransformOffsetAt(VariationIdx, 2, 0); }
						)))
						.Y(TAttribute<TOptional<float>>::Create(
							TAttribute<TOptional<float>>::FGetter::CreateLambda([&CurInstanceOutput, VariationIdx]()
								{ return CurInstanceOutput.GetTransformOffsetAt(VariationIdx, 2, 1); }
						)))
						.Z(TAttribute<TOptional<float>>::Create(
							TAttribute<TOptional<float>>::FGetter::CreateLambda([&CurInstanceOutput, VariationIdx]()
								{ return CurInstanceOutput.GetTransformOffsetAt(VariationIdx, 2, 2); }
						)))
						.OnXCommitted_Lambda([&CurInstanceOutput, VariationIdx, ChangeTransformOffsetAt, ChangeTransformOffsetUniformlyAt](float Val, ETextCommit::Type TextCommitType)
						{
							if (CurInstanceOutput.IsUnformScaleLocked())
								ChangeTransformOffsetUniformlyAt(Val, 2);
							else
								ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, 2, 0); 
						})	
						.OnYCommitted_Lambda([&CurInstanceOutput, VariationIdx, ChangeTransformOffsetAt, ChangeTransformOffsetUniformlyAt](float Val, ETextCommit::Type TextCommitType)
						{
							if (CurInstanceOutput.IsUnformScaleLocked())
								ChangeTransformOffsetUniformlyAt(Val, 2);
							else
								ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, 2, 1); 
						})
						.OnZCommitted_Lambda([&CurInstanceOutput, VariationIdx, ChangeTransformOffsetAt, ChangeTransformOffsetUniformlyAt](float Val, ETextCommit::Type TextCommitType)
						{
							if (CurInstanceOutput.IsUnformScaleLocked())
								ChangeTransformOffsetUniformlyAt(Val, 2);
							else
								ChangeTransformOffsetAt(CurInstanceOutput, VariationIdx, Val, 2, 2);
						})
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Right)
					[
						// Lock Button
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Right).VAlign(VAlign_Center).Padding(0.0f)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.ToolTipText(LOCTEXT("InstancerOutputLockButtonToolTip", "When locked, scales uniformly based on the current xyz scale values so the output object maintains its shape in each direction when scaled"))
							.Visibility(EVisibility::Visible)
							[
								SNew(SImage)
								.Image(CurInstanceOutput.IsUnformScaleLocked() ? FEditorStyle::GetBrush("GenericLock") : FEditorStyle::GetBrush("GenericUnlock"))
							]
							.OnClicked_Lambda([&CurInstanceOutput, InOutput]() 
							{
								CurInstanceOutput.SwitchUniformScaleLock();
								FHoudiniEngineUtils::UpdateEditorProperties(InOutput->GetOuter(), true);
								return FReply::Handled();
							})
						]
						// Reset Button
						+ SHorizontalBox::Slot().AutoWidth().HAlign(HAlign_Left).VAlign(VAlign_Center).Padding(0.0f)
						[
							SNew(SButton)
							.ButtonStyle(FEditorStyle::Get(), "NoBorder")
							.ClickMethod(EButtonClickMethod::MouseDown)
							.ToolTipText(LOCTEXT("GeoInputResetButtonToolTip", "Reset To Default"))
							.Visibility(bResetButtonVisibleScale ? EVisibility::Visible : EVisibility::Hidden)
							[
								SNew(SImage)
								.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
							]
							.OnClicked_Lambda([ChangeTransformOffsetUniformlyAt, InOutput]()
							{
								ChangeTransformOffsetUniformlyAt(1.0f, 2);
								FHoudiniEngineUtils::UpdateEditorProperties(InOutput->GetOuter(), true);
								return FReply::Handled();
							})
						]
					]
					/*
					// TODO: Add support for this back
					+ SHorizontalBox::Slot().AutoWidth()
					[
						// Add a checkbox to toggle between preserving the ratio of x,y,z components of scale when a value is entered
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "TransparentCheckBox")
						.ToolTipText(LOCTEXT("PreserveScaleToolTip", "When locked, scales uniformly based on the current xyz scale values so the object maintains its shape in each direction when scaled"))
						*//*
						.OnCheckStateChanged(FOnCheckStateChanged::CreateLambda([=](ECheckBoxState NewState)
						{
							if ( MyParam.IsValid() && InputFieldPtr.IsValid() )
								MyParam->CheckStateChanged( NewState == ECheckBoxState::Checked, InputFieldPtr.Get(), VariationIdx );
						}))
						.IsChecked( TAttribute< ECheckBoxState >::Create(
							TAttribute<ECheckBoxState>::FGetter::CreateLambda( [=]() 
							{
								if (InputFieldPtr.IsValid() && InputFieldPtr->AreOffsetsScaledLinearly(VariationIdx))
									return ECheckBoxState::Checked;
								return ECheckBoxState::Unchecked;
							}
						)))
						*//*
						[
							SNew(SImage)
							*//*.Image(TAttribute<const FSlateBrush*>::Create(
								TAttribute<const FSlateBrush*>::FGetter::CreateLambda( [=]() 
								{
									if ( InputFieldPtr.IsValid() && InputFieldPtr->AreOffsetsScaledLinearly( VariationIdx ) )
									{
										return FEditorStyle::GetBrush( TEXT( "GenericLock" ) );
									}
									return FEditorStyle::GetBrush( TEXT( "GenericUnlock" ) );
								}
							)))
							*//*
							.ColorAndOpacity( FSlateColor::UseForeground() )
						]
					]
					*/
				];
			}
		}
	}
}

/*
void
FHoudiniOutputDetails::OnMaterialInterfaceSelected(
	const FAssetData & AssetData, 
	ALandscapeProxy* Landscape,
	UHoudiniOutput * InOutput,
	int32 MaterialIdx)
{
	TPairInitializer< ALandscapeProxy *, int32 > Pair(Landscape, MaterialIdx);
	TSharedPtr< SComboButton > AssetComboButton = LandscapeMaterialInterfaceComboButtons[Pair];
	if (AssetComboButton.IsValid())
	{
		AssetComboButton->SetIsOpen(false);

		UObject * Object = AssetData.GetAsset();
		OnMaterialInterfaceDropped(Object, Landscape, InOutput, MaterialIdx);
	}
}
*/

void
FHoudiniOutputDetails::CreateDefaultOutputWidget(
	IDetailCategoryBuilder& HouOutputCategory,
	UHoudiniOutput* InOutput)
{
	if (!InOutput)
		return;

	// Get thumbnail pool for this builder.
	TSharedPtr< FAssetThumbnailPool > AssetThumbnailPool = HouOutputCategory.GetParentLayout().GetThumbnailPool();

	// TODO
	// This is just a temporary placeholder displaying name/output type
	{
		FString OutputNameStr = InOutput->GetName();
		FText OutputTooltip = GetOutputTooltip(InOutput);

		// Create a new detail row
		// Name 
		FText OutputNameTxt = GetOutputDebugName(InOutput);
		FDetailWidgetRow & Row = HouOutputCategory.AddCustomRow(FText::GetEmpty());
		Row.NameWidget.Widget =
			SNew(STextBlock)
			.Text(OutputNameTxt)
			.ToolTipText(OutputTooltip)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

		// Value
		FText OutputTypeTxt = GetOutputDebugDescription(InOutput);
		Row.ValueWidget.Widget =
			SNew(STextBlock)
			.Text(OutputTypeTxt)
			.ToolTipText(OutputTooltip)
			.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")));

		Row.ValueWidget.MinDesiredWidth(HAPI_UNREAL_DESIRED_ROW_VALUE_WIDGET_WIDTH);
	}
}

void
FHoudiniOutputDetails::OnBakeOutputObject(
	const FString& InBakeName,
	UObject * BakedOutputObject, 
	const FHoudiniOutputObjectIdentifier & OutputIdentifier,
	const FHoudiniOutputObject& InOutputObject,
	const FHoudiniGeoPartObject & HGPO,
	const UObject* OutputOwner,
	const FString & HoudiniAssetName,
	const FString & BakeFolder,
	const FString & TempCookFolder,
	const EHoudiniOutputType & Type,
	const EHoudiniLandscapeOutputBakeType & LandscapeBakeType,
	const TArray<UHoudiniOutput*>& InAllOutputs)
{
	if (!BakedOutputObject || BakedOutputObject->IsPendingKill())
		return;

	// Fill in the package params
	FHoudiniPackageParams PackageParams;
	// Configure FHoudiniAttributeResolver and fill the package params with resolved object name and bake folder.
	// The resolver is then also configured with the package params for subsequent resolving (level_path etc)
	FHoudiniAttributeResolver Resolver;
	// Determine the relevant WorldContext based on the output owner
	UWorld* WorldContext = OutputOwner ? OutputOwner->GetWorld() : GWorld;
	const UHoudiniAssetComponent* HAC = FHoudiniEngineUtils::GetOuterHoudiniAssetComponent(OutputOwner);
	check(IsValid(HAC));
	const bool bAutomaticallySetAttemptToLoadMissingPackages = true;
	const bool bSkipObjectNameResolutionAndUseDefault = !InBakeName.IsEmpty();  // If InBakeName is set use it as is for the object name
	const bool bSkipBakeFolderResolutionAndUseDefault = false;
	FHoudiniEngineUtils::FillInPackageParamsForBakingOutputWithResolver(
		WorldContext, HAC, OutputIdentifier, InOutputObject, BakedOutputObject->GetName(),
		HoudiniAssetName, PackageParams, Resolver,
		BakeFolder, EPackageReplaceMode::ReplaceExistingAssets,
		bAutomaticallySetAttemptToLoadMissingPackages, bSkipObjectNameResolutionAndUseDefault,
		bSkipBakeFolderResolutionAndUseDefault);

	switch (Type) 
	{
		case EHoudiniOutputType::Mesh:
		{
			UStaticMesh* StaticMesh = Cast<UStaticMesh>(BakedOutputObject);
			if (StaticMesh)
			{
				FDirectoryPath TempCookFolderPath;
				TempCookFolderPath.Path = TempCookFolder;
				UStaticMesh* DuplicatedMesh = FHoudiniEngineBakeUtils::BakeStaticMesh(
					StaticMesh, PackageParams, InAllOutputs, TempCookFolderPath);
			}
		}
		break;
		case EHoudiniOutputType::Curve:
		{
			USplineComponent* SplineComponent = Cast<USplineComponent>(BakedOutputObject);
			if (SplineComponent)
			{
				AActor* BakedActor;
				USplineComponent* BakedSplineComponent;
				FHoudiniEngineBakeUtils::BakeCurve(SplineComponent, GWorld->GetCurrentLevel(), PackageParams, BakedActor, BakedSplineComponent);
			}
		}
		break;
		case EHoudiniOutputType::Landscape:
		{
			ALandscapeProxy* Landscape = Cast<ALandscapeProxy>(BakedOutputObject);
			if (Landscape)
			{
				FHoudiniEngineBakeUtils::BakeHeightfield(Landscape, PackageParams, LandscapeBakeType);
			}
		}
		break;
	}
}

FReply
FHoudiniOutputDetails::OnRefineClicked(UObject* ObjectToRefine, UHoudiniOutput* InOutput)
{	
	// TODO: Actually refine only the selected ProxyMesh
	// For now, refine all the selection
	FHoudiniEngineCommands::RefineHoudiniProxyMeshesToStaticMeshes(true, true);

	FHoudiniEngineUtils::UpdateEditorProperties(InOutput->GetOuter(), true);
	return FReply::Handled();
}

void
FHoudiniOutputDetails::OnBakeNameCommitted(
	const FText& Val, ETextCommit::Type TextCommitType,
	UHoudiniOutput * InOutput, const FHoudiniOutputObjectIdentifier& InIdentifier) 
{
	if (!InOutput)
		return;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	FHoudiniOutputObject* FoundOutputObject = OutputObjects.Find(InIdentifier);

	if (!FoundOutputObject)
		return;

	FoundOutputObject->BakeName = Val.ToString();
}

void
FHoudiniOutputDetails::OnRevertBakeNameToDefault(UHoudiniOutput * InOutput, const FHoudiniOutputObjectIdentifier & InIdentifier) 
{
	if (!InOutput)
		return;

	TMap<FHoudiniOutputObjectIdentifier, FHoudiniOutputObject>& OutputObjects = InOutput->GetOutputObjects();
	FHoudiniOutputObject* FoundOutputObject = OutputObjects.Find(InIdentifier);

	if (!FoundOutputObject)
		return;

	FoundOutputObject->BakeName = FString();
}
#undef LOCTEXT_NAMESPACE