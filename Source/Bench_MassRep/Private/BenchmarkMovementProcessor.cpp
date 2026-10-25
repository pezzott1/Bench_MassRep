/**
 * @file BenchmarkMovementProcessor.cpp
 * @brief Evaluates vertical wave movement directly from world time.
 */

#include "BenchmarkMovementProcessor.h"
#include "MassRepBenchmarkActor.h"
#include "Mass/EntityFragments.h"
#include "MassEntityManager.h"
#include "MassExecutionContext.h"
#include "MassUpdateISMProcessor.h"
#include "Engine/World.h"

/**
 * @brief Configures shared movement in PrePhysics before native ISM submission.
 */
UBenchmarkMovementProcessor::UBenchmarkMovementProcessor() : EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = (int32)(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteBefore.Add(UMassUpdateISMProcessor::StaticClass()->GetFName());
}

/**
 * @brief Declares transform writes and immutable wave-setting reads.
 * @param EntityManager Mass entity manager providing world access or query configuration.
 */
void UBenchmarkMovementProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FBenchmarkMovementFragment>(EMassFragmentAccess::ReadOnly);
}

/**
 * @brief Sets vertical positions from origin and world-time sine waves.
 * @param EntityManager Mass entity manager providing world access or query configuration.
 * @param Context Mass execution context passed to entity-chunk iteration.
 */
void UBenchmarkMovementProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	const UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}
	const double Time = World->GetTimeSeconds();
	EntityQuery.ForEachEntityChunk(Context, [Time](FMassExecutionContext& ChunkContext)
	{
		auto Transforms = ChunkContext.GetMutableFragmentView<FTransformFragment>();
		const auto Movements = ChunkContext.GetFragmentView<FBenchmarkMovementFragment>();
		for (int32 Index = 0; Index < ChunkContext.GetNumEntities(); ++Index)
		{
			const FBenchmarkMovementFragment& Movement = Movements[Index];
			FVector Position = Movement.Origin;
			Position.Z += Movement.Amplitude * FMath::Sin(Time * 2.0 * PI * Movement.Frequency + Movement.Phase);
			Transforms[Index].GetMutableTransform().SetLocation(Position);
		}
	});
}

