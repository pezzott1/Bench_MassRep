/**
 * @file BenchmarkNiagaraSyncProcessor.cpp
 * @brief Publishes complete Niagara slot records after Mass movement.
 */

#include "BenchmarkNiagaraSyncProcessor.h"
#include "BenchmarkMovementProcessor.h"
#include "BenchmarkNiagaraSettingsSubsystem.h"
#include "MassRepBenchmarkActor.h"
#include "Mass/EntityFragments.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Engine/World.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelFunctionLibrary.h"

/**
 * @brief Orders game-thread Niagara publication after movement and labels its persistent writer.
 */
UBenchmarkNiagaraSyncProcessor::UBenchmarkNiagaraSyncProcessor() : EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteAfter.Add(UBenchmarkMovementProcessor::StaticClass()->GetFName());
	bRequiresGameThreadExecution = true;
	TransformWriter.DebugSource = TEXT("BenchmarkNiagaraSyncProcessor");
}

/**
 * @brief Declares transform reads and visual-slot allocation writes.
 * @param EntityManager Mass entity manager providing world access or query configuration.
 */
void UBenchmarkNiagaraSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBenchmarkNiagaraVisualFragment>(EMassFragmentAccess::ReadWrite);
}

/**
 * @brief Resets presence flags and active slots, then prepares reusable free indices.
 * @param Capacity Number of slots to initialize in the reusable visual pool.
 */
void UBenchmarkNiagaraSyncProcessor::InitializePool(int32 Capacity)
{
	SeenSlots.Init(0, Capacity);
	PreviousTransforms.Init(FTransform::Identity, Capacity);
	FreeSlots.Reset();
	ActiveSlots.Reset();
	FreeSlots.Reserve(Capacity);
	ActiveSlots.Reserve(Capacity);
	for (int32 Slot = Capacity - 1; Slot >= 0; --Slot)
	{
		FreeSlots.Add(Slot);
	}
}

/**
 * @brief Publishes the full GPU slot pool, allocating active slots and clearing inactive records.
 * @param EntityManager Mass entity manager providing world access or query configuration.
 * @param Context Mass execution context passed to entity-chunk iteration.
 */
void UBenchmarkNiagaraSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	check(IsInGameThread());
	UWorld* World = EntityManager.GetWorld();
	const UBenchmarkNiagaraSettingsSubsystem* Settings = World ? World->GetSubsystem<UBenchmarkNiagaraSettingsSubsystem>() : nullptr;
	if (!Settings || Settings->SlotCapacity <= 0)
	{
		if (!SeenSlots.IsEmpty())
		{
			InitializePool(0);
			TransformChannel = nullptr;
		}
		return;
	}
	if (!Settings->bPublicationEnabled || !Settings->Channel)
	{
		return;
	}

	TransformChannel = Settings->Channel;
	const bool bResetPool = SeenSlots.Num() != Settings->SlotCapacity
		|| LastPublicationRevision != Settings->PublicationRevision;

	// Begin once per frame; the persistent writer refreshes cached offsets if the layout changes.
	const UNiagaraDataChannel* Channel = TransformChannel->Get();
	if (!Channel)
	{
		return;
	}
	FNDCAccessContextInst& AccessContext = UNiagaraDataChannelLibrary::GetUsableAccessContextFromNDC(TransformChannel);
	FNDCScopedWriter<FBenchmarkTransformNDCWriter> Writer(TransformWriter);
	if (!Writer->BeginWrite(World, Channel, AccessContext, Settings->SlotCapacity, false, false, true))
	{
		return;
	}

	// Commit mapping/history reset only when a write session is available.
	if (bResetPool)
	{
		InitializePool(Settings->SlotCapacity);
		LastPublicationRevision = Settings->PublicationRevision;
	}

	for (const int32 Slot : ActiveSlots)
	{
		SeenSlots[Slot] = 0;
	}

	EntityQuery.ForEachEntityChunk(Context, [this, bResetPool, &Writer](FMassExecutionContext& ChunkContext)
	{
		const auto Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
		auto Visuals = ChunkContext.GetMutableFragmentView<FBenchmarkNiagaraVisualFragment>();
		for (int32 Index = 0; Index < ChunkContext.GetNumEntities(); ++Index)
		{
			FBenchmarkNiagaraVisualFragment& Visual = Visuals[Index];
			if (bResetPool)
			{
				Visual.VisualSlot = INDEX_NONE;
			}
			const bool bNewSlot = Visual.VisualSlot == INDEX_NONE;
			if (bNewSlot)
			{
				if (FreeSlots.IsEmpty())
				{
					continue;
				}
				Visual.VisualSlot = FreeSlots.Pop(EAllowShrinking::No);
				ActiveSlots.Add(Visual.VisualSlot);
			}
			check(SeenSlots.IsValidIndex(Visual.VisualSlot));
			const FTransform& Transform = Transforms[Index].GetTransform();
			const FTransform& Previous = bNewSlot ? Transform : PreviousTransforms[Visual.VisualSlot];
			Writer->WritePosition(Visual.VisualSlot, Transform.GetLocation());
			Writer->WriteRotation(Visual.VisualSlot, Transform.GetRotation());
			Writer->WriteScale(Visual.VisualSlot, Transform.GetScale3D());
			Writer->WritePreviousPosition(Visual.VisualSlot, Previous.GetLocation());
			Writer->WritePreviousRotation(Visual.VisualSlot, Previous.GetRotation());
			Writer->WritePreviousScale(Visual.VisualSlot, Previous.GetScale3D());
			PreviousTransforms[Visual.VisualSlot] = Transform;
			Writer->WriteActive(Visual.VisualSlot, true);
			SeenSlots[Visual.VisualSlot] = 1;
		}
	});

	// Reclaim slots whose entities no longer occur in the Mass query.
	for (int32 Index = ActiveSlots.Num() - 1; Index >= 0; --Index)
	{
		const int32 Slot = ActiveSlots[Index];
		if (SeenSlots[Slot] != 0)
		{
			continue;
		}
		ActiveSlots.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		FreeSlots.Add(Slot);
	}

	for (int32 Slot = 0; Slot < SeenSlots.Num(); ++Slot)
	{
		if (SeenSlots[Slot] == 0)
		{
			Writer->WritePosition(Slot, FVector::ZeroVector);
			Writer->WriteRotation(Slot, FQuat::Identity);
			Writer->WriteScale(Slot, FVector::ZeroVector);
			Writer->WritePreviousPosition(Slot, FVector::ZeroVector);
			Writer->WritePreviousRotation(Slot, FQuat::Identity);
			Writer->WritePreviousScale(Slot, FVector::ZeroVector);
			Writer->WriteActive(Slot, false);
		}
	}
	// Scoped writer calls EndWrite, including on early return.
}

