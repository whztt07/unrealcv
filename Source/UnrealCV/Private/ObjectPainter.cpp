#include "UnrealCVPrivate.h"
#include "ObjectPainter.h"
#include "StaticMeshResources.h"
#include "UE4CVServer.h"
#include "SceneViewport.h"
#include "Version.h"

// Utility function to generate color map
int32 GetChannelValue(uint32 Index)
{
	static int32 Values[256] = { 0 };
	static bool Init = false;
	if (!Init)
	{
		float Step = 256;
		uint32 Iter = 0;
		Values[0] = 0;
		while (Step >= 1)
		{
			for (uint32 Value = Step-1; Value <= 256; Value += Step * 2)
			{
				Iter++;
				Values[Iter] = Value;
			}
			Step /= 2;
		}
		Init = true;
	}
	if (Index >= 0 && Index <= 255)
	{
		return Values[Index];
	}
	else
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Invalid channel index"));
		check(false);
		return -1;
	}
}

void GetColors(int32 MaxVal, bool Fix1, bool Fix2, bool Fix3, TArray<FColor>& ColorMap)
{
	for (int32 I = 0; I <= (Fix1 ? 0 : MaxVal-1); I++)
	{
		for (int32 J = 0; J <= (Fix2 ? 0 : MaxVal-1); J++)
		{
			for (int32 K = 0; K <= (Fix3 ? 0 : MaxVal-1); K++)
			{
				uint8 R = GetChannelValue(Fix1 ? MaxVal : I);
				uint8 G = GetChannelValue(Fix2 ? MaxVal : J);
				uint8 B = GetChannelValue(Fix3 ? MaxVal : K);
				FColor Color(R, G, B, 255);
				ColorMap.Add(Color);
			}
		}
	}
}

FColor GetColorFromColorMap(int32 ObjectIndex)
{
	static TArray<FColor> ColorMap;
	int NumPerChannel = 32;
	if (ColorMap.Num() == 0)
	{
		// 32 ^ 3
		for (int32 MaxChannelIndex = 0; MaxChannelIndex < NumPerChannel; MaxChannelIndex++) // Get color map for 1000 objects
		{
			// GetColors(MaxChannelIndex, false, false, false, ColorMap);
			GetColors(MaxChannelIndex, false, false, true , ColorMap);
			GetColors(MaxChannelIndex, false, true , false, ColorMap);
			GetColors(MaxChannelIndex, false, true , true , ColorMap);
			GetColors(MaxChannelIndex, true , false, false, ColorMap);
			GetColors(MaxChannelIndex, true , false, true , ColorMap);
			GetColors(MaxChannelIndex, true , true , false, ColorMap);
			GetColors(MaxChannelIndex, true , true , true , ColorMap);
		}
	}
	if (ObjectIndex < 0 || ObjectIndex >= pow(NumPerChannel, 3))
	{
		UE_LOG(LogUnrealCV, Error, TEXT("Object index %d is out of the color map boundary [%d, %d]"), ObjectIndex, 0, pow(NumPerChannel, 3));
	}
	return ColorMap[ObjectIndex];
}

FObjectPainter& FObjectPainter::Get()
{
	static FObjectPainter Singleton(NULL);
	return Singleton;
}

FObjectPainter::FObjectPainter(ULevel* InLevel)
{
	this->Level = InLevel;
}

FExecStatus FObjectPainter::SetActorColor(FString ObjectName, FColor Color)
{
	auto ObjectsMapping = GetObjectsMapping();
	if (ObjectsMapping.Contains(ObjectName))
	{
		AActor* Actor = ObjectsMapping[ObjectName];
		if (PaintObject(Actor, Color))
		{
			ObjectsColorMapping.Emplace(ObjectName, Color);
			return FExecStatus::OK();
		}
		else
		{
			return FExecStatus::Error(FString::Printf(TEXT("Failed to paint object %s"), *ObjectName));
		}
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Object %s not exist"), *ObjectName));
	}
}

FExecStatus FObjectPainter::GetActorColor(FString ObjectName)
{
	if (ObjectsColorMapping.Contains(ObjectName))
	{
		FColor ObjectColor = ObjectsColorMapping[ObjectName]; // Make sure the object exist
		FString Message = ObjectColor.ToString();
		// FString Message = "%.3f %.3f %.3f %.3f";
		return FExecStatus::OK(Message);
	}
	else
	{
		return FExecStatus::Error(FString::Printf(TEXT("Object %s not exist"), *ObjectName));
	}
}

FExecStatus FObjectPainter::GetObjectList()
{
	TArray<FString> Keys;
	GetObjectsMapping().GetKeys(Keys);
	FString Message = "";
	for (auto ObjectName : Keys)
	{
		Message += ObjectName + " ";
	}
	Message = Message.LeftChop(1);
	return FExecStatus::OK(Message);
}

TMap<FString, AActor*>& FObjectPainter::GetObjectsMapping()
{
	static TMap<FString, AActor*> ObjectsMapping;
	if (!this->Level) return ObjectsMapping;
	check(Level);
	if (ObjectsMapping.Num() == 0)
	{
		for (AActor* Actor : Level->Actors)
		{
			if (Actor && Actor->IsA(AStaticMeshActor::StaticClass())) // Only StaticMeshActor is interesting
			{
				// FString ActorLabel = Actor->GetActorLabel();
				FString ActorLabel = Actor->GetHumanReadableName();
				ObjectsMapping.Emplace(ActorLabel, Actor);
			}
		}
	}
	return ObjectsMapping;
}

bool FObjectPainter::PaintRandomColors()
{
	UWorld* World = FUE4CVServer::Get().GetGameWorld();
	FSceneViewport* SceneViewport = World->GetGameViewport()->GetGameViewport();
	/*
	float Gamma = SceneViewport->GetDisplayGamma();
	check(Gamma != 0);
	if (Gamma == 0) Gamma = 1;
	float InvGamma = 1 / Gamma;
	BuildDecodeColorValue(InvGamma);
	*/
	// Iterate over all actors
	// ULevel* Level = GetLevel();

	// Get a random color
	check(Level);
	uint32 ObjectIndex = 0;
	for (auto Actor : Level->Actors)
	{
		if (Actor && Actor->IsA(AStaticMeshActor::StaticClass())) // Only StaticMeshActor is interesting
		{
			// FString ActorLabel = Actor->GetActorLabel();
			FString ActorLabel = Actor->GetHumanReadableName();
			// FColor NewColor = FColor(FMath::RandRange(0, 255), FMath::RandRange(0, 255), FMath::RandRange(0, 255), 255);
			// FColor NewColor = FColor(1, 1, 1, 255);
			/*
			FColor NewColor;
			TArray<uint8> ValidVals;
			DecodeColorValue.GenerateKeyArray(ValidVals);

			NewColor.R = ValidVals[FMath::RandRange(0, ValidVals.Num()-1)];
			NewColor.G = ValidVals[FMath::RandRange(0, ValidVals.Num()-1)];
			NewColor.B = ValidVals[FMath::RandRange(0, ValidVals.Num()-1)];
			NewColor.A = 255;
			*/
			// FColor NewColor = FColor(128, 128, 128, 255);
			// FColor NewColor = FColor::MakeRandomColor();
			FColor NewColor = GetColorFromColorMap(ObjectIndex);

			ObjectsColorMapping.Emplace(ActorLabel, NewColor);
			check(PaintObject(Actor, NewColor));
			ObjectIndex++;
		}
	}

	// Paint actor using floodfill.
	// return FExecStatus::OK();
	return true;
}

/** DisplayColor is the color that the screen will show
	If DisplayColor.R = 128, the display will show 0.5 voltage
	To achieve this, UnrealEngine will do gamma correction.
	The value on image will be 187.
	https://en.wikipedia.org/wiki/Gamma_correction#Methods_to_perform_display_gamma_correction_in_computing
*/
bool FObjectPainter::PaintObject(AActor* Actor, const FColor& Color, bool IsColorGammaEncoded)
{
	if (!Actor) return false;

	FColor NewColor;
	if (IsColorGammaEncoded)
	{
		/*
		FSceneViewport* SceneViewport = FUE4CVServer::Get().GetPawn()->GetWorld()->GetGameViewport()->GetGameViewport();
		float Gamma = SceneViewport->GetDisplayGamma();
		check(Gamma != 0);
		if (Gamma == 0) Gamma = 1;
		float InvGamma = 1 / Gamma;

		bool Converted = true;
		Converted &= GetDisplayValue(Color.R, NewColor.R, InvGamma);
		Converted &= GetDisplayValue(Color.G, NewColor.G, InvGamma);
		Converted &= GetDisplayValue(Color.B, NewColor.B, InvGamma);
		if (!Converted)
		{
			UE_LOG(LogUnrealCV, Error, TEXT("Can not convert encoded color %d %d %d"), Color.R, Color.G, Color.B);
			return false;
		}

		// See UnrealEngine/Engine/Shaders/GammaCorrection.usf
		// This is the real calculation, but due to numerical issue, we need to use table lookup
		// NewColor.R = FMath::RoundToInt(FMath::Pow(Color.R / 255.0, Gamma) * 255.0);
		// NewColor.G = FMath::RoundToInt(FMath::Pow(Color.G / 255.0, Gamma) * 255.0);
		// NewColor.B = FMath::RoundToInt(FMath::Pow(Color.B / 255.0, Gamma) * 255.0);
		*/
		FLinearColor LinearColor = FLinearColor::FromPow22Color(Color);
		NewColor = LinearColor.ToFColor(false);
		// NewColor = LinearColor.ToFColor(true); // this is incorrect, not sRGB, just pow22
	}
	else
	{
		NewColor = Color;
	}

	TArray<UMeshComponent*> PaintableComponents;
	// TInlineComponentArray<UMeshComponent*> MeshComponents;
	// Actor->GetComponents<UMeshComponent>(MeshComponents);
	Actor->GetComponents<UMeshComponent>(PaintableComponents);


	for (auto MeshComponent : PaintableComponents)
	{
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(MeshComponent))
		{
			UStaticMesh* StaticMesh;
#if ENGINE_MINOR_VERSION >= 14  // Assume major version is 4
			StaticMesh = StaticMeshComponent->GetStaticMesh(); // This is a new function introduced in 4.14
#else
			StaticMesh = StaticMeshComponent->StaticMesh; // This is deprecated in 4.14, add here for backward compatibility
#endif
			if (StaticMesh)
			{
				uint32 PaintingMeshLODIndex = 0;
				uint32 NumLODLevel = StaticMesh->RenderData->LODResources.Num();
				check(NumLODLevel == 1);
				FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[PaintingMeshLODIndex];
				FStaticMeshComponentLODInfo* InstanceMeshLODInfo = NULL;

				// PaintingMeshLODIndex + 1 is the minimum requirement, enlarge if not satisfied
				StaticMeshComponent->SetLODDataCount(PaintingMeshLODIndex + 1, StaticMeshComponent->LODData.Num());
				InstanceMeshLODInfo = &StaticMeshComponent->LODData[PaintingMeshLODIndex];

				// Setup OverrideVertexColors
				// if (!InstanceMeshLODInfo->OverrideVertexColors) // TODO: Check this
				{
					InstanceMeshLODInfo->OverrideVertexColors = new FColorVertexBuffer;

					FColor FillColor = FColor(255, 255, 255, 255);
					InstanceMeshLODInfo->OverrideVertexColors->InitFromSingleColor(FColor::White, LODModel.GetNumVertices());
				}

				uint32 NumVertices = LODModel.GetNumVertices();
				check(InstanceMeshLODInfo->OverrideVertexColors);
				check(NumVertices <= InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices());
				// StaticMeshComponent->CachePaintedDataIfNecessary();

				for (uint32 ColorIndex = 0; ColorIndex < NumVertices; ++ColorIndex)
				{
					// FColor NewColor = FColor(FMath::RandRange(0, 255), FMath::RandRange(0, 255), FMath::RandRange(0, 255), 255);
					// LODModel.ColorVertexBuffer.VertexColor(ColorIndex) = NewColor;  // This is vertex level
					// Need to initialize the vertex buffer first
					uint32 NumOverrideVertexColors = InstanceMeshLODInfo->OverrideVertexColors->GetNumVertices();
					uint32 NumPaintedVertices = InstanceMeshLODInfo->PaintedVertices.Num();
					// check(NumOverrideVertexColors == NumPaintedVertices);
					InstanceMeshLODInfo->OverrideVertexColors->VertexColor(ColorIndex) = NewColor;
					// InstanceMeshLODInfo->PaintedVertices[ColorIndex].Color = NewColor;
				}
				BeginInitResource(InstanceMeshLODInfo->OverrideVertexColors);
				StaticMeshComponent->MarkRenderStateDirty();
				// BeginUpdateResourceRHI(InstanceMeshLODInfo->OverrideVertexColors);


				/*
				// TODO: Need to check other LOD levels
				// Use flood fill to paint mesh vertices
				UE_LOG(LogUnrealCV, Warning, TEXT("%s:%s has %d vertices"), *Actor->GetActorLabel(), *StaticMeshComponent->GetName(), NumVertices);

				if (LODModel.ColorVertexBuffer.GetNumVertices() == 0)
				{
					// Mesh doesn't have a color vertex buffer yet!  We'll create one now.
					LODModel.ColorVertexBuffer.InitFromSingleColor(FColor(255, 255, 255, 255), LODModel.GetNumVertices());
				}

				*/
			}
		}
	}
	return true;
}

void FObjectPainter::SetLevel(ULevel* InLevel)
{
	Level = InLevel;
}

AActor* FObjectPainter::GetObject(FString ObjectName)
{
	/** Return the pointer of an object, return NULL if object not found */
	auto ObjectsMapping = GetObjectsMapping();
	if (ObjectsMapping.Contains(ObjectName))
	{
		return ObjectsMapping[ObjectName];
	}
	else
	{
		return NULL;
	}
}
