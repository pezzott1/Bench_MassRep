/**
 * @file BenchmarkCustomISMSyncProcessor.cpp
 * @brief Collects fixed-slot transforms and submits a single ISM batch update.
 */

#include "BenchmarkCustomISMSyncProcessor.h"
#include "BenchmarkISMSettingsSubsystem.h"
#include "BenchmarkMovementProcessor.h"
#include "MassRepBenchmarkActor.h"
#include "Mass/EntityFragments.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

/**
 * @brief Orders game-thread bulk ISM updates after benchmark movement.
 */
UBenchmarkCustomISMSyncProcessor::UBenchmarkCustomISMSyncProcessor() : EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	bRequiresGameThreadExecution = true;
	ExecutionOrder.ExecuteAfter.Add(UBenchmarkMovementProcessor::StaticClass()->GetFName());
}

/**
 * @brief Declares transform and fixed-instance-index reads.
 * @param EntityManager Mass entity manager providing world access or query configuration.
 */
void UBenchmarkCustomISMSyncProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FBenchmarkISMInstanceFragment>(EMassFragmentAccess::ReadOnly);
}

/**
 * @brief Collects and submits the fixed population using separate profiling scopes.
 * @param EntityManager Manager used to resolve the world's custom ISM settings.
 * @param Context Mass context passed to the serial entity-chunk iteration.
 * @note ISM still performs downstream bounds, buffer, and rendering updates.
 */
void UBenchmarkCustomISMSyncProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UBenchmarkISMSettingsSubsystem* Settings = World ? World->GetSubsystem<UBenchmarkISMSettingsSubsystem>() : nullptr;
	if (!Settings || !Settings->bUpdatesEnabled || !IsValid(Settings->Component))
	{
		return;
	}

	const int32 Count = Settings->CurrentTransforms.Num();
	if (Count == 0 || !ensure(Settings->PreviousTransforms.Num() == Count
		&& Settings->Component->GetInstanceCount() == Count))
	{
		return;
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BenchmarkCustomISM_CollectTransforms);
		EntityQuery.ForEachEntityChunk(Context, [Settings](FMassExecutionContext& ChunkContext)
		{
			const auto Transforms = ChunkContext.GetFragmentView<FTransformFragment>();
			const auto Instances = ChunkContext.GetFragmentView<FBenchmarkISMInstanceFragment>();
			for (int32 Index = 0; Index < ChunkContext.GetNumEntities(); ++Index)
			{
				const int32 Slot = Instances[Index].InstanceIndex;
				if (ensure(Settings->CurrentTransforms.IsValidIndex(Slot)))
				{
					Settings->CurrentTransforms[Slot] = Transforms[Index].GetTransform();
				}
			}
		});
	}

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BenchmarkCustomISM_BatchUpdate);
		// The component is fixed at identity, so world transforms are also local transforms.
		// Match native Mass: preserve previous transforms and use incremental change tracking.
		if (Settings->Component->BatchUpdateInstancesTransforms(0, Settings->CurrentTransforms,
			Settings->PreviousTransforms, /*bWorldSpace=*/false, /*bMarkRenderStateDirty=*/false, /*bTeleport=*/false))
		{
			/** @brief Retain the submitted frame as history without copying the transform array. */
			Swap(Settings->CurrentTransforms, Settings->PreviousTransforms);
		}
	}
}


