/**
 * @file BenchmarkNiagaraSettingsSubsystem.h
 * @brief World-local Niagara publication configuration shared with the benchmark actor.
 */

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BenchmarkNiagaraSettingsSubsystem.generated.h"

class UNiagaraDataChannelAsset;

/**
 * @brief Shares Niagara publication settings between the benchmark actor and sync processor.
 * The actor supplies the channel, capacity, and enabled flag; the processor owns slot storage.
 * @note Supports one benchmark actor per world. Configuration is accessed on the game thread.
 */
UCLASS()
class BENCH_MASSREP_API UBenchmarkNiagaraSettingsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	/** @brief Channel selected by the benchmark actor during setup. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraDataChannelAsset> Channel;

	/** @brief Fixed particle/slot capacity for this benchmark session. */
	int32 SlotCapacity = 0;

	/** @brief Whether the actor has selected Niagara representation. */
	bool bPublicationEnabled = false;

	/** @brief Incremented on Niagara mode entry to reset slot mapping and transform history. */
	uint32 PublicationRevision = 0;
};

