/**
 * @file BenchmarkNiagaraSyncProcessor.h
 * @brief Declares the cached typed NDC writer and reusable visual-slot pool.
 */

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "MassEntityQuery.h"
#include "NiagaraDataChannelAccessor.h"
#include "BenchmarkNiagaraSyncProcessor.generated.h"

class UNiagaraDataChannelAsset;
/**
 * @brief Native NDC writer caching typed current/previous transform and activity offsets.
 * @note Stored directly in the processor to reuse layout information across frames.
 * FNDCScopedWriter ends each write scope; no manual allocation or custom deleter is needed.
 */
struct FBenchmarkTransformNDCWriter : public FNDCWriterBase
{
	/** @brief Typed accessor for each slot's world-space position. */
	NDCVarWriter(FNiagaraPosition, Position);
	/** @brief Current world-space orientation quaternion. */
	NDCVarWriter(FQuat, Rotation);
	/** @brief Current world-space mesh scale. */
	NDCVarWriter(FVector, Scale);
	/** @brief Previous submitted world position for motion vectors. */
	NDCVarWriter(FNiagaraPosition, PreviousPosition);
	/** @brief Previous submitted world orientation for motion vectors. */
	NDCVarWriter(FQuat, PreviousRotation);
	/** @brief Previous submitted world scale for motion vectors. */
	NDCVarWriter(FVector, PreviousScale);
	/** @brief Typed accessor indicating whether a slot currently represents an entity. */
	NDCVarWriter(bool, Active);
};
/**
 * @brief Publishes current/previous entity transforms and active flags by stable slot after movement.
 * Allocates reusable slots, reclaims slots missing from the query, and writes inactive holes.
 * @note Serial game-thread execution; the allocation lists are not safe for parallel chunk writes.
 * Niagara particles retain their spawn-assigned BenchmarkIndex and read the matching slot.
 * @warning The channel must expose Position/PreviousPosition (Position), Rotation/PreviousRotation (Quat), Scale/PreviousScale (Vector), and Active (Bool).
 */
UCLASS()
class BENCH_MASSREP_API UBenchmarkNiagaraSyncProcessor : public UMassProcessor
{
	GENERATED_BODY()
public:
	/** @brief Configures game-thread NDC publication after movement. */
	UBenchmarkNiagaraSyncProcessor();
	/**
	 * @brief Keeps execution available to clear slots even after the last entity is destroyed.
	 * @param bRuntimeMode Whether Mass is evaluating pruning for runtime execution.
	 * @return False to prevent query-based pruning.
	 */
	virtual bool ShouldAllowQueryBasedPruning(const bool bRuntimeMode) const override { return false; }
protected:
	/**
	 * @brief Requires read-only transforms and mutable visual slots for pool allocation.
	 * @param EntityManager Manager supplied by the Mass framework.
	 */
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	/**
	 * @brief Writes active entities and inactive holes into the full slot pool.
	 * @param EntityManager Manager providing the world and benchmark entities.
	 * @param Context Mass execution context used for serial chunk iteration.
	 * @note Begins one GPU-only NDC write per frame; scoped cleanup ends the write.
	 * Entities without a free slot are skipped until capacity becomes available.
	 */
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
private:
	/** @brief Matches transforms and visual-slot fragments used by publication. */
	FMassEntityQuery EntityQuery;

	/** @brief Persistent typed writer caches the channel's variable offsets across frames. */
	FBenchmarkTransformNDCWriter TransformWriter;

	/** @brief Cached channel; refreshed from world settings when publication is enabled. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraDataChannelAsset> TransformChannel;

	/** @brief Per-slot presence flags rebuilt during each publication, following MPF. */
	TArray<uint8> SeenSlots;
	/** @brief Available slot indices; popped without shrinking when assigning an entity. */
	TArray<int32> FreeSlots;
	/** @brief Assigned slots scanned for entities no longer present in the query. */
	TArray<int32> ActiveSlots;

	/** @brief Last published transform for each slot; new occupants start without motion history. */
	TArray<FTransform> PreviousTransforms;

	/** @brief Last actor publication revision, used to reset history when entering Niagara mode. */
	uint32 LastPublicationRevision = 0;

	/**
	 * @brief Clears allocation state and fills the free list in reverse index order.
	 * @param Capacity Number of available visual slots; zero releases the active population.
	 * @note Reverse initialization assigns the lowest index first.
	 */
	void InitializePool(int32 Capacity);
};

