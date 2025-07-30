// Copyright Voxel Plugin SAS. All Rights Reserved.

#include "VoxelMessageTokens.h"
#include "Misc/UObjectToken.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Logging/TokenizedMessage.h"

uint32 FVoxelMessageToken_Text::GetHash() const
{
	return GetTypeHash(Text);
}

FString FVoxelMessageToken_Text::ToString() const
{
	return Text;
}

TSharedRef<IMessageToken> FVoxelMessageToken_Text::GetMessageToken() const
{
	return FTextToken::Create(FText::FromString(Text));
}

bool FVoxelMessageToken_Text::TryMerge(const FVoxelMessageToken& Other)
{
	const FVoxelMessageToken_Text* OtherText = CastStruct<FVoxelMessageToken_Text>(Other);
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

UObject* FVoxelMessageToken_Object::GetObject() const
{
	UObject* Object = WeakObject.Resolve();
	if (!Object)
	{
		return nullptr;
	}

	// If Object is a blueprint-generated class, select the blueprint instead
#if WITH_EDITOR
	if (const UClass* Class = Cast<UClass>(Object))
	{
		if (const UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(Class))
		{
			if (UBlueprint* Blueprint = Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy))
			{
				return Blueprint;
			}
		}
	}
#endif

	return Object;
}

uint32 FVoxelMessageToken_Object::GetHash() const
{
	return GetTypeHash(WeakObject);
}

FString FVoxelMessageToken_Object::ToString() const
{
	return WeakObject.GetReadableName();
}

TSharedRef<IMessageToken> FVoxelMessageToken_Object::GetMessageToken() const
{
	ensure(IsInGameThread());

#if WITH_EDITOR
	const UObject* Object = GetObject();

	return FActionToken::Create(
		FText::FromString(ToString()),
		FText::FromString(Object ? Object->GetPathName() : "null"),
		MakeWeakObjectPtrDelegate(Object, [Object]
		{
			FVoxelUtilities::FocusObject(Object);
		}));
#else
	return Super::GetMessageToken();
#endif
}

void FVoxelMessageToken_Object::GetObjects(TSet<const UObject*>& OutObjects) const
{
	ensure(IsInGameThread());
	OutObjects.Add(GetObject());
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

TSharedRef<IMessageToken> FVoxelMessageToken_Pin::GetMessageToken() const
{
	ensure(IsInGameThread());

#if WITH_EDITOR
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

			FVoxelUtilities::FocusObject(Node);
		}));
#else
	return Super::GetMessageToken();
#endif
}

void FVoxelMessageToken_Pin::GetObjects(TSet<const UObject*>& OutObjects) const
{
	const UEdGraphPin* Pin = PinReference.Get();
	if (!Pin)
	{
		return;
	}

	OutObjects.Add(Pin->GetOwningNodeUnchecked());
}