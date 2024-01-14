// Copyright Voxel Plugin, Inc. All Rights Reserved.

#include "VoxelMessageTokens.h"
#include "Misc/UObjectToken.h"
#include "Logging/TokenizedMessage.h"

uint32 FVoxelMessageToken_Text::GetHash() const
{
	return GetTypeHash(Text);
}

FString FVoxelMessageToken_Text::ToString() const
{
	return Text;
}

#if WITH_EDITOR
TSharedRef<IMessageToken> FVoxelMessageToken_Text::GetMessageToken() const
{
	return FTextToken::Create(FText::FromString(Text));
}
#endif

bool FVoxelMessageToken_Text::TryMerge(const FVoxelMessageToken& Other)
{
	const FVoxelMessageToken_Text* OtherText = Cast<FVoxelMessageToken_Text>(Other);
	if (!OtherText)
	{
		return false;
	}

	Text += OtherText->Text;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint32 FVoxelMessageToken_Object::GetHash() const
{
	return GetTypeHash(WeakObject);
}

FString FVoxelMessageToken_Object::ToString() const
{
	ensure(IsInGameThread());

	const UObject* Object = WeakObject.Get();
	if (!Object)
	{
		return "<null>";
	}

	if (const UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
	{
#if WITH_EDITOR
		FString Result = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
		if (Result.TrimStartAndEnd().IsEmpty())
		{
			Result = "<empty>";
		}
		return Result;
#else
		return Node->GetName();
#endif
	}

	if (const AActor* Actor = Cast<AActor>(Object))
	{
		return Actor->GetActorNameOrLabel();
	}

	if (const UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		FString ActorName;
		if (const AActor* Owner = Component->GetOwner())
		{
			ActorName = Owner->GetActorNameOrLabel();
		}
		else
		{
			ActorName = "<null>";
		}

		return ActorName + "." + Component->GetName();
	}

	if (!Object->HasAnyFlags(RF_Transient))
	{
		// If we are a subobject of an asset, use the asset
		Object = Object->GetOutermostObject();
	}

	return Object->GetName();
}

#if WITH_EDITOR
TSharedRef<IMessageToken> FVoxelMessageToken_Object::GetMessageToken() const
{
	ensure(IsInGameThread());

	return FActionToken::Create(
			FText::FromString(ToString()),
			FText::FromString(WeakObject.IsValid() ? WeakObject->GetPathName() : "null"),
			MakeLambdaDelegate([WeakObject = WeakObject]
			{
				const UObject* Object = WeakObject.Get();
				if (!Object)
				{
					return;
				}

				FVoxelObjectUtilities::FocusObject(Object);
			}));
}
#endif

void FVoxelMessageToken_Object::GetObjects(TSet<const UObject*>& OutObjects) const
{
	ensure(IsInGameThread());
	OutObjects.Add(WeakObject.Get());
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

uint32 FVoxelMessageToken_Pin::GetHash() const
{
	ensure(IsInGameThread());
	return GetTypeHash(PinReference.Get());
}

FString FVoxelMessageToken_Pin::ToString() const
{
	ensure(IsInGameThread());

	UEdGraphPin* Pin = PinReference.Get();
	if (!Pin)
	{
		return "<null>";
	}

	FString Result;
	if (const UEdGraphNode* Node = Pin->GetOwningNode())
	{
#if WITH_EDITOR
		Result = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();

		if (Result.TrimStartAndEnd().IsEmpty())
		{
			Result = "<empty>";
		}
#else
		Result = Node->GetName();
#endif
	}
	else
	{
		Result = "<null>";
	}

	Result += ".";
#if WITH_EDITOR
	Result += Pin->GetDisplayName().ToString();
#else
	Result += Pin->GetName();
#endif
	return Result;
}

#if WITH_EDITOR
TSharedRef<IMessageToken> FVoxelMessageToken_Pin::GetMessageToken() const
{
	ensure(IsInGameThread());

	return FActionToken::Create(
		FText::FromString(ToString()),
		FText::FromString("Go to pin " + ToString()),
		MakeLambdaDelegate([PinReference = PinReference]
		{
			const UEdGraphPin* Pin = PinReference.Get();
			if (!Pin)
			{
				return;
			}

			const UEdGraphNode* Node = Pin->GetOwningNodeUnchecked();
			if (!Node)
			{
				return;
			}

			FVoxelObjectUtilities::FocusObject(Node);
		}));
}
#endif

void FVoxelMessageToken_Pin::GetObjects(TSet<const UObject*>& OutObjects) const
{
	const UEdGraphPin* Pin = PinReference.Get();
	if (!Pin)
	{
		return;
	}

	OutObjects.Add(Pin->GetOwningNodeUnchecked());
}