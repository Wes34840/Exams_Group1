#include "StencilMatrixLUTSubsystem.h"
#include "Engine/Texture2D.h"
#include "Rendering/Texture2DResource.h"
#include "PrimitiveSceneProxy.h"
#include "SceneViewExtension.h"

FRHITexture* UStencilMatrixLUTSubsystem::GetLUTTextureRHI_RenderThread()
{
	if (MatrixLUTTexture && MatrixLUTTexture->GetResource())
	{
		FTexture2DResource* TexResource =
			static_cast<FTexture2DResource*>(MatrixLUTTexture->GetResource());
		if (TexResource)
		{
			return TexResource->GetTextureRHI();
		}
	}
	return nullptr;
}

void UStencilMatrixLUTSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Create the texture
	EnsureTextureCreated();
	UE_LOG(LogTemp, Warning, TEXT("StencilMatrixLUTSubsystem texture created"));
	// Register the view extension
	ViewExtension = FSceneViewExtensions::NewExtension<FStencilLUTViewExtension>(this);
	UE_LOG(LogTemp, Warning, TEXT("StencilMatrixLUTSubsystem texture initialization completed successfully"));
	CandidatePrimitives.Reserve(256);
	//TrackedPrimitives.Reserve(256);
	IdSlots.Reserve(255);
	for (uint16 i = 1; i < 256; i++)
	{
		IdSlots.Add((uint8)i);
	}
}

void UStencilMatrixLUTSubsystem::Deinitialize()
{
	MatrixLUTTexture = nullptr;
	bTextureInitialized = false;
	UE_LOG(LogTemp, Warning, TEXT("StencilMatrixLUTSubsystem deinitialized"));
	Super::Deinitialize();
	ViewExtension.Reset();
}

void UStencilMatrixLUTSubsystem::Tick(float DeltaTime)
{
	TrackRelevantPrimitives();
}

void UStencilMatrixLUTSubsystem::EnsureTextureCreated()
{
	if (bTextureInitialized && MatrixLUTTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("Matrix LUT texture already initialized"));
		return;
	}
	// Create a 256x4 float texture: each column = one stencil ID, each row = one matrix row
	MatrixLUTTexture = UTexture2D::CreateTransient(LUT_Width, LUT_Height, PF_A32B32G32R32F);


	if (!MatrixLUTTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to create Matrix LUT texture - skill issue"));
		return;
	}
	else {
		UE_LOG(LogTemp, Warning, TEXT("Created Matrix LUT texture: %s"), *MatrixLUTTexture->GetName());
	}

	MatrixLUTTexture->AddressX = TA_Clamp;
	MatrixLUTTexture->AddressY = TA_Clamp;
	MatrixLUTTexture->Filter = TF_Nearest; // important: no interpolation between IDs
	MatrixLUTTexture->SRGB = false;
	MatrixLUTTexture->NeverStream = true; // ensure it's always resident
	MatrixLUTTexture->LODGroup = TEXTUREGROUP_Pixels2D;
	MatrixLUTTexture->UpdateResource();
	bTextureInitialized = true;
}

// Call this method from any actor that needs to be tracked
void UStencilMatrixLUTSubsystem::RegisterPrimitiveCandidate(UPrimitiveComponent* Primitive)
{
	if (Primitive && !CandidatePrimitives.Contains(Primitive))
	{
		CandidatePrimitives.Add(Primitive);
		UE_LOG(LogTemp, Warning, TEXT("Registered actor %s for stencil matrix LUT tracking"), *Primitive->GetName());
	}
}

// Call this method to stop tracking an actor
void UStencilMatrixLUTSubsystem::UnregisterPrimitiveCandidate(UPrimitiveComponent* Primitive)
{
	for (int i = 0; i < CandidatePrimitives.Num(); i++) {
		if (CandidatePrimitives[i] == Primitive)
		{
			CandidatePrimitives.RemoveAtSwap(i);
			UE_LOG(LogTemp, Warning, TEXT("Unregistered actor %s from stencil matrix LUT tracking"), *Primitive->GetName());
			return;
		}
	}
}

void UStencilMatrixLUTSubsystem::TrackRelevantPrimitives()
{
	for (UPrimitiveComponent* Primitive : CandidatePrimitives)
	{
		// Ideally, we would perform those checks only every few frames, but for simplicity we do it every frame here
		// Depending on the scope of the game world, we could also check distance to the camera, chunk etc.
		if (Primitive->WasRecentlyRendered())
		{
			if (Primitive->CustomDepthStencilValue == 0)
			{
				AssignIdToPrimitive(Primitive);
			}
		}
		// If the primitive is no longer visible on screen, free its ID slot
		else {
			if (Primitive->CustomDepthStencilValue != 0)
			{
				FreeStencilIdSlot(Primitive->CustomDepthStencilValue);
				Primitive->SetCustomDepthStencilValue(0); // reset
			}
		}
	}
}

bool UStencilMatrixLUTSubsystem::AllocateId(uint8& OutId)
{
	if (IdSlots.Num() == 0)
	{
		return false; // no IDs available
	}
	OutId = IdSlots.Pop(); //O(1)
	return true;
}

void UStencilMatrixLUTSubsystem::AssignIdToPrimitive(UPrimitiveComponent* Primitive)
{
	uint8 StencilId;
	if (AllocateId(StencilId)) {
		Primitive->SetCustomDepthStencilValue(StencilId);
	}
	else {
		Primitive->SetCustomDepthStencilValue(0); // no ID available
	}
}

void UStencilMatrixLUTSubsystem::FreeStencilIdSlot(uint8 Slot)
{
	IdSlots.Add(Slot); //O(1)
}




