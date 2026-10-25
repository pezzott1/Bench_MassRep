/**
 * @file MassRepBenchmarkActor.h
 * @brief Declares benchmark fragments, representation modes, and actor configuration.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "MassEntityHandle.h"
#include "MassEntityTypes.h"

#include "MassRepBenchmarkActor.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class UNiagaraDataChannelAsset;
class UInstancedStaticMeshComponent;

/**
 * @brief Maps a Mass entity to a fixed custom ISM component instance.
 * @note Assigned once; Mass storage reordering does not change this mapping.
 * @warning Component instance removal or reordering requires rebuilding the mapping.
 */
USTRUCT()
struct FBenchmarkISMInstanceFragment : public FMassFragment
{
	GENERATED_BODY()

	/** @brief Component index assigned during entity creation. */
	UPROPERTY()
	int32 InstanceIndex = INDEX_NONE;
};

/** @brief Stable visual slot retained when Mass storage or the entity array changes order. */
USTRUCT()
struct FBenchmarkNiagaraVisualFragment : public FMassFragment
{
	GENERATED_BODY()

	/** @brief Index of this entity's record in the complete NDC slot pool. */
	UPROPERTY()
	int32 VisualSlot = INDEX_NONE;
};

/** @brief Immutable wave settings and origin used by the movement processor. */
USTRUCT()
struct FBenchmarkMovementFragment : public FMassFragment
{
	GENERATED_BODY()

	/** @brief World-space center of the entity's wave motion. */
	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	/** @brief Maximum vertical displacement in centimeters. */
	UPROPERTY()
	float Amplitude = 300.0f;

	/** @brief Wave cycles per second. */
	UPROPERTY()
	float Frequency = 0.5f;

	/** @brief Per-entity phase offset in radians. */
	UPROPERTY()
	float Phase = 0.0f;
};
/** @brief Available visual representations for the benchmark entities. */
UENUM(BlueprintType)
enum class EBenchmarkRepresentation : uint8
{
	None,      ///< Simulation without visual representation.
	ISM,       ///< Native Mass Representation ISM updates.
	Niagara,   ///< Niagara particles driven by Data Channel slots.
	CustomISM  ///< Standard ISM component driven by one bulk update.
};

/**
 * @brief Creates a deterministic grid of Mass entities for representation benchmarking.
 */
UCLASS()
class BENCH_MASSREP_API AMassRepBenchmarkActor : public AActor
{
	GENERATED_BODY()
	
public:	
	/** @brief Creates the representation components; Mass processors perform all frame updates. */
	AMassRepBenchmarkActor();
	
public:
	/**
	 * @brief Changes the visual representation while preserving entities and movement.
	 * @param NewRepresentation Requested native ISM, Niagara, custom ISM, or invisible mode.
	 * @return True when selected; false if the Mass subsystem or benchmark entities are unavailable.
	 * @pre NewRepresentation is valid; Niagara mode requires its system, channel, and component.
	 * @note Entering CustomISM rebuilds instances and resets previous transform history.
	 */
	UFUNCTION(BlueprintCallable, Category = "Benchmark")
	bool SetRepresentation(EBenchmarkRepresentation NewRepresentation);

	/**
	 * @brief Returns the currently selected visual representation.
	 * @return Active mode used to ignore redundant selection inputs.
	 */
	EBenchmarkRepresentation GetCurrentRepresentation() const { return CurrentRepresentation; }
	
	/**
	 * @brief Enables native ISM representation without recreating the entities.
	 * @return True when benchmark entities are available.
	 */
	bool EnableISM();

	/**
	 * @brief Enables bulk ISM submission for the existing fixed population.
	 * @return True when entities are available and the mode is selected.
	 * @see UBenchmarkCustomISMSyncProcessor
	 */
	bool EnableCustomISM();
	
	/**
	 * @brief Disables all visual representations without destroying benchmark entities.
	 * @return True when benchmark entities are available.
	 */
	bool DisableRepresentation();

protected:
	/** @brief Registers the mesh, creates the fixed entity grid, and configures both sync subsystems. */
	virtual void BeginPlay() override;

	/**
	 * @brief Disables visual output, destroys owned entities, and releases subsystem state.
	 * @param EndPlayReason Reason supplied by Unreal when the actor leaves play.
	 */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** @brief Number of entities created when play begins. */
	UPROPERTY(EditAnywhere, Category = "Benchmark", meta = (ClampMin = "1"))
	int32 EntityCount = 2000;

	/** @brief Number of entities in each grid row. */
	UPROPERTY(EditAnywhere, Category = "Benchmark", meta = (ClampMin = "1"))
	int32 GridColumns = 50;

	/** @brief Distance between entity centers, in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Benchmark", meta = (ClampMin = "1.0"))
	float GridSpacing = 200.0f;

	/** @brief Maximum vertical wave displacement; zero disables movement. */
	UPROPERTY(EditAnywhere, Category = "Benchmark|Movement", meta = (ClampMin = "0.0"))
	float MovementAmplitude = 300.0f;

	/** @brief Wave cycles per second. */
	UPROPERTY(EditAnywhere, Category = "Benchmark|Movement", meta = (ClampMin = "0.0"))
	float MovementFrequency = 0.5f;

	/** @brief Mesh shared by the ISM and Niagara benchmark modes. */
	UPROPERTY(EditAnywhere, Category = "Benchmark|Representation")
	TObjectPtr<UStaticMesh> BenchmarkMesh;

	/** @brief Optional override for the mesh's first material slot. */
	UPROPERTY(EditAnywhere, Category = "Benchmark|Representation")
	TObjectPtr<UMaterialInterface> BenchmarkMaterial;

private:
	/** @brief Handles belonging exclusively to this benchmark actor. */
	TArray<FMassEntityHandle> Entities;
	
public:
	/**
	 * @brief Enables Niagara representation for the existing Mass entities.
	 * @return True when the required assets and entities are available.
	 */
	bool EnableNiagara();
	
protected:
	/** @brief Niagara system containing one persistent particle per entity. */
	UPROPERTY(EditAnywhere, Category = "Benchmark|Niagara")
	TObjectPtr<UNiagaraSystem> BenchmarkNiagaraSystem;

	/** @brief Global Data Channel receiving the entity positions. */
	UPROPERTY(EditAnywhere, Category = "Benchmark|Niagara")
	TObjectPtr<UNiagaraDataChannelAsset> BenchmarkDataChannel;

	/** @brief Single component rendering all Niagara benchmark particles. */
	UPROPERTY(VisibleAnywhere, Category = "Benchmark|Niagara")
	TObjectPtr<UNiagaraComponent> BenchmarkNiagaraComponent;

	/** @brief Standard ISM component updated by the bulk sync processor in mode 3. */
	UPROPERTY(VisibleAnywhere, Category = "Benchmark|CustomISM")
	TObjectPtr<UInstancedStaticMeshComponent> BenchmarkISMComponent;
	
private:

	/** @brief Selected mode; all transitions go through SetRepresentation. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Benchmark", meta = (AllowPrivateAccess = "true"))
	EBenchmarkRepresentation CurrentRepresentation = EBenchmarkRepresentation::None;
};


