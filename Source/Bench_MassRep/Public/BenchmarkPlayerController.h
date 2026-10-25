/**
 * @file BenchmarkPlayerController.h
 * @brief Declares representation selection actions and benchmark feedback.
 */

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BenchmarkPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * @brief Handles input for the Mass representation benchmark.
 */
UCLASS()
class BENCH_MASSREP_API ABenchmarkPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:
	/** @brief Registers the configured benchmark context and runtime key-3 context for the local player. */
	virtual void BeginPlay() override;

	/** @brief Binds benchmark actions to their handlers. */
	virtual void SetupInputComponent() override;

	/** @brief Mapping context containing the benchmark key assignments. */
	UPROPERTY(EditDefaultsOnly, Category = "Benchmark|Input")
	TObjectPtr<UInputMappingContext> BenchmarkMappingContext;

	/** @brief Requests ISM representation. */
	UPROPERTY(EditDefaultsOnly, Category = "Benchmark|Input")
	TObjectPtr<UInputAction> ISMAction;

	/** @brief Requests Niagara representation. */
	UPROPERTY(EditDefaultsOnly, Category = "Benchmark|Input")
	TObjectPtr<UInputAction> NiagaraAction;

	/** @brief Requests simulation without representation. */
	UPROPERTY(EditDefaultsOnly, Category = "Benchmark|Input")
	TObjectPtr<UInputAction> NoneAction;

	/** @brief Requests a reset of the benchmark. */
	UPROPERTY(EditDefaultsOnly, Category = "Benchmark|Input")
	TObjectPtr<UInputAction> ResetAction;

private:
	/**
	 * @brief Creates the Boolean custom ISM action and key-3 context on first use.
	 * @note Idempotent; supports either input setup or BeginPlay running first.
	 * Transient reflected references retain both runtime assets.
	 */
	void InitializeCustomISMInput();

	/** @brief Runtime action retained for the lifetime of this controller. */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CustomISMAction;

	/** @brief Runtime context mapping key 3 to the custom ISM action. */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> CustomISMMappingContext;

	/** @brief Handles the ISM selection action. */
	void SelectISM();

	/**
	 * @brief Selects custom ISM on the first benchmark actor in the level.
	 * @note Emits CUSTOM_ISM only after successful selection.
	 */
	void SelectCustomISM();

	/** @brief Handles the Niagara selection action. */
	void SelectNiagara();

	/** @brief Handles the representation disable action. */
	void SelectNone();

	/** @brief Reports a reset request; entity reset is not implemented. */
	void ResetBenchmark();

	/**
	 * @brief Logs benchmark feedback and displays it on screen when the engine is available.
	 * @param Message User-facing status or validation message.
	 */
	void ShowBenchmarkMessage(const FString& Message) const;
};



