/**
 * @file BenchmarkISMSettingsSubsystem.h
 * @brief World-local component and transform buffers for custom bulk ISM updates.
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BenchmarkISMSettingsSubsystem.generated.h"

class UInstancedStaticMeshComponent;

/**
 * @brief Shares custom ISM configuration and transform buffers without actor searches.
 *
 * The actor seeds both buffers on mode entry. The processor fills CurrentTransforms,
 * submits both arrays, then swaps them to retain the last submitted frame.
 * Leaving the mode disables updates, clears instances, and resets the buffers.
 * @note Game-thread access only; supports one benchmark actor per world.
 */
UCLASS()
class BENCH_MASSREP_API UBenchmarkISMSettingsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	/** @brief Actor-owned component; avoids actor searches during processor execution. */
	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> Component;

	/** @brief Whether custom ISM mode is selected. */
	bool bUpdatesEnabled = false;

	/**
	 * @brief Reusable write buffer indexed by FBenchmarkISMInstanceFragment::InstanceIndex.
	 * @note World transforms are also local transforms because the component stays at identity.
	 */
	TArray<FTransform> CurrentTransforms;

	/**
	 * @brief Previous submitted transforms used for per-instance motion vectors.
	 * @note Initialized from current transforms on mode entry to avoid stale motion history.
	 */
	TArray<FTransform> PreviousTransforms;
};


