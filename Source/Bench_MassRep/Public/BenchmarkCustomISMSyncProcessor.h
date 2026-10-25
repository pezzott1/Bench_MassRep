/**
 * @file BenchmarkCustomISMSyncProcessor.h
 * @brief Declares custom bulk ISM submission after Mass movement.
 */

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "BenchmarkCustomISMSyncProcessor.generated.h"

/**
 * @brief Displays a fixed Mass population using one bulk ISM update per frame.
 *
 * Runs after movement in PrePhysics on the game thread. Stable fragment indices replace
 * native Mass Representation's entity-to-instance lookup. Both current and previous
 * transforms are submitted for motion vectors.
 * @note Supports one benchmark actor per world.
 * @warning Individual instance removal or reordering requires rebuilding the index mapping.
 * @see UBenchmarkISMSettingsSubsystem
 */
UCLASS()
class BENCH_MASSREP_API UBenchmarkCustomISMSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	/** @brief Runs on the game thread after benchmark movement. */
	UBenchmarkCustomISMSyncProcessor();
protected:
	/**
	 * @brief Declares read-only access to transforms and stable instance indices.
	 * @param EntityManager Entity manager supplied by the Mass framework.
	 */
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;

	/**
	 * @brief Collects transforms by stable index and submits one bulk component update.
	 * @param EntityManager Manager providing the world and entity storage.
	 * @param Context Execution context used to iterate matching chunks.
	 * @pre Component transform is identity; both buffers match the instance count.
	 * @note Returns outside CustomISM mode. Swaps buffers only after successful submission.
	 * Collection and batch submission have separate Unreal Insights CPU scopes.
	 */
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	/** @brief Query restricted to entities carrying custom ISM indices. */
	FMassEntityQuery EntityQuery;
};


