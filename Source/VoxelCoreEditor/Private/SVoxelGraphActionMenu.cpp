// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "SVoxelGraphActionMenu.h"
#include "GraphActionNode.h"
#include "Widgets/Layout/SScrollBorder.h"

void SVoxelGraphActionMenu::Construct(
	const FArguments& InArgs,
	const bool bReadOnly,
	const TSharedPtr<SScrollBar>& ExternalScrollBar)
{
	SGraphActionMenu::Construct(InArgs, bReadOnly);

	if (!ExternalScrollBar)
	{
		return;
	}

	TreeView =
		SNew(STreeView<TSharedPtr<FGraphActionNode>>)
		.TreeItemsSource(&(this->FilteredRootAction->Children))
		.OnGenerateRow(this, &SVoxelGraphActionMenu::MakeWidget, bReadOnly)
		.OnSelectionChanged(this, &SVoxelGraphActionMenu::OnItemSelected)
		.OnMouseButtonDoubleClick(this, &SVoxelGraphActionMenu::OnItemDoubleClicked)
		.OnContextMenuOpening(InArgs._OnContextMenuOpening)
		.OnGetChildren(this, &SVoxelGraphActionMenu::OnGetChildrenForCategory)
		.SelectionMode(ESelectionMode::Single)
		.OnItemScrolledIntoView(this, &SVoxelGraphActionMenu::OnItemScrolledIntoView)
		.OnSetExpansionRecursive(this, &SVoxelGraphActionMenu::OnSetExpansionRecursive)
		.HighlightParentNodesForSelection(true)
		.ExternalScrollbar(ExternalScrollBar);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(FilterTextBox, SSearchBox)
			.Visibility(InArgs._OnGetFilterText.IsBound() ? EVisibility::Collapsed : EVisibility::Visible)
			.OnTextChanged( this, &SVoxelGraphActionMenu::OnFilterTextChanged )
			.OnTextCommitted( this, &SVoxelGraphActionMenu::OnFilterTextCommitted )
			.DelayChangeNotificationsWhileTyping(false)
		]
		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
		.FillHeight(1.f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(SScrollBorder, TreeView.ToSharedRef())
				[
					TreeView.ToSharedRef()
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				ExternalScrollBar.ToSharedRef()
			]
		]
	];

	RefreshAllActions(false);

	if (bAutomaticallySelectSingleAction)
	{
		TArray<TSharedPtr<FGraphActionNode>> ActionNodes;
		FilteredRootAction->GetAllActionNodes(ActionNodes);

		if (ActionNodes.Num() == 1)
		{
			OnItemSelected(ActionNodes[0], ESelectInfo::Direct);
		}
	}
}

TSharedPtr<FGraphActionNode> SVoxelGraphActionMenu::FindItemByName(
	const FName ItemName,
	const int32 SectionId) const
{
	if (ItemName == NAME_None)
	{
		return nullptr;
	}

	TSharedPtr<FGraphActionNode> SelectionNode;

	TArray<TSharedPtr<FGraphActionNode>> GraphNodes;
	FilteredRootAction->GetAllNodes(GraphNodes);

	for (int32 Index = 0; Index < GraphNodes.Num() && !SelectionNode.IsValid(); ++Index)
	{
		const TSharedPtr<FGraphActionNode> CurrentGraphNode = GraphNodes[Index];
		FEdGraphSchemaAction* GraphAction = CurrentGraphNode->GetPrimaryAction().Get();

		// If the user is attempting to select a category, make sure it's a category
		if (!CurrentGraphNode->IsCategoryNode())
		{
			if (SectionId == -1 ||
				CurrentGraphNode->SectionID == SectionId)
			{
				if (GraphAction)
				{
					if (OnActionMatchesName.IsBound() &&
						OnActionMatchesName.Execute(GraphAction, ItemName))
					{
						SelectionNode = GraphNodes[Index];
						break;
					}
				}

				if (CurrentGraphNode->GetDisplayName().ToString() == FName::NameToDisplayString(ItemName.ToString(), false))
				{
					SelectionNode = CurrentGraphNode;
					break;
				}
			}
		}

		// One of the children may match
		for (int32 ChildIdx = 0; ChildIdx < CurrentGraphNode->Children.Num() && !SelectionNode.IsValid(); ++ChildIdx)
		{
			const TSharedPtr<FGraphActionNode> CurrentChildNode = CurrentGraphNode->Children[ChildIdx];
			if (const TSharedPtr<FEdGraphSchemaAction> ChildGraphAction = CurrentChildNode->GetPrimaryAction())
			{
				if (!CurrentChildNode->IsCategoryNode())
				{
					if (SectionId == -1 ||
						CurrentChildNode->SectionID == SectionId)
					{
						if (ChildGraphAction)
						{
							if (OnActionMatchesName.IsBound() &&
								OnActionMatchesName.Execute(ChildGraphAction.Get(), ItemName))
							{
								SelectionNode = GraphNodes[Index]->Children[ChildIdx];
							}
						}
						else if (CurrentChildNode->GetDisplayName().ToString() == FName::NameToDisplayString(ItemName.ToString(), false))
						{
							SelectionNode = CurrentChildNode;
						}
					}
				}
			}
		}
	}

	if (SelectionNode)
	{
		return SelectionNode;
	}

	return nullptr;
}

FSlateRect SVoxelGraphActionMenu::GetPaintSpaceEntryBounds(
	const TSharedPtr<FGraphActionNode>& Node,
	const bool bIncludeChildren) const
{
	FSlateRect Result{ -1, -1, -1, -1 };
	if (const TSharedPtr<ITableRow> Row = TreeView->WidgetFromItem(Node))
	{
		if (const TSharedPtr<SWidget> Widget = Row->AsWidget())
		{
			Result = Widget->GetPaintSpaceGeometry().GetLayoutBoundingRect();
		}
	}

	if (!bIncludeChildren ||
		Node->Children.Num() == 0 ||
		!TreeView->IsItemExpanded(Node))
	{
		return Result;
	}

	TArray<TSharedPtr<FGraphActionNode>> Children;
	Node->GetAllNodes(Children);

	for (const TSharedPtr<FGraphActionNode>& ChildNode : Children)
	{
		if (const TSharedPtr<ITableRow> Row = TreeView->WidgetFromItem(ChildNode))
		{
			if (const TSharedPtr<SWidget> Widget = Row->AsWidget())
			{
				FSlateRect ChildRect = Widget->GetPaintSpaceGeometry().GetLayoutBoundingRect();
				Result = Result.IsValid() ? Result.Expand(ChildRect) : ChildRect;
			}
		}
	}

	return Result;
}

FSlateRect SVoxelGraphActionMenu::GetTickSpaceEntryBounds(
	const TSharedPtr<FGraphActionNode>& Node,
	const bool bIncludeChildren) const
{
	FSlateRect Result{ -1, -1, -1, -1 };
	if (const TSharedPtr<ITableRow> Row = TreeView->WidgetFromItem(Node))
	{
		if (const TSharedPtr<SWidget> Widget = Row->AsWidget())
		{
			Result = Widget->GetTickSpaceGeometry().GetLayoutBoundingRect();
		}
	}

	if (!bIncludeChildren ||
		Node->Children.Num() == 0 ||
		!TreeView->IsItemExpanded(Node))
	{
		return Result;
	}

	TArray<TSharedPtr<FGraphActionNode>> Children;
	Node->GetAllNodes(Children);

	for (const TSharedPtr<FGraphActionNode>& ChildNode : Children)
	{
		if (const TSharedPtr<ITableRow> Row = TreeView->WidgetFromItem(ChildNode))
		{
			if (const TSharedPtr<SWidget> Widget = Row->AsWidget())
			{
				FSlateRect ChildRect = Widget->GetTickSpaceGeometry().GetLayoutBoundingRect();
				Result = Result.IsValid() ? Result.Expand(ChildRect) : ChildRect;
			}
		}
	}

	return Result;
}