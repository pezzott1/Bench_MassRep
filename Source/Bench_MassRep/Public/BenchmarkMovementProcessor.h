/**
 * @file BenchmarkMovementProcessor.h
 * @brief Declares shared deterministic movement used by all representation modes.
 */

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "BenchmarkMovementProcessor.generated.h"

/**
 * @brief Updates entity transforms with representation-independent vertical wave motion.
 * Evaluates position from immutable origin, frequency, amplitude, and per-entity phase.
 * @note Runs in PrePhysics before native ISM submission; both custom sync processors run after it.
 */
UCLASS()
class BENCH_MASSREP_API UBenchmarkMovementProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	/** @brief Orders movement before native ISM submission. */
	UBenchmarkMovementProcessor();
protected:
	/**
	 * @brief Requires mutable transforms and immutable wave settings.
	 * @param EntityManager Manager supplied by the Mass processor framework.
	 */
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	/**
	 * @brief Evaluates the wave from world time without accumulating position error.
	 * @param EntityManager Manager providing the current world and entity data.
	 * @param Context Mass context used to iterate benchmark entities.
	 * @note Zero amplitude keeps entities at their original positions.
	 */
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	/** @brief Matches benchmark entities with a transform and wave settings. */
	FMassEntityQuery EntityQuery;
};

