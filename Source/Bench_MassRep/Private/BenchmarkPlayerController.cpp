/**
 * @file BenchmarkPlayerController.cpp
 * @brief Registers Enhanced Input actions, changes modes, and emits trace bookmarks.
 */

#include "BenchmarkPlayerController.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"

#include "EngineUtils.h"
#include "MassRepBenchmarkActor.h"

/**
 * @brief Registers configured and runtime input contexts for the local controller.
 */
void ABenchmarkPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (!IsLocalController())
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InitializeCustomISMInput();
			InputSubsystem->AddMappingContext(CustomISMMappingContext, 0);

			if (BenchmarkMappingContext)
			{
				InputSubsystem->AddMappingContext(BenchmarkMappingContext, 0);
			}
		}
	}
}

/**
 * @brief Binds representation selection to Enhanced Input Started events.
 */
void ABenchmarkPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);

	if (!ensureMsgf(EnhancedInput, TEXT("Benchmark requires an EnhancedInputComponent.")))
	{
		return;
	}

	// Create and bind a runtime action without requiring a new Blueprint asset.
	InitializeCustomISMInput();
	EnhancedInput->BindAction(CustomISMAction, ETriggerEvent::Started, this, &ThisClass::SelectCustomISM);

	// Started executes once when each action begins.
	if (ISMAction)
	{
		EnhancedInput->BindAction(ISMAction, ETriggerEvent::Started, this, &ThisClass::SelectISM);
	}

	if (NiagaraAction)
	{
		EnhancedInput->BindAction(NiagaraAction, ETriggerEvent::Started, this, &ThisClass::SelectNiagara);
	}

	if (NoneAction)
	{
		EnhancedInput->BindAction(NoneAction, ETriggerEvent::Started, this, &ThisClass::SelectNone);
	}

	if (ResetAction)
	{
		EnhancedInput->BindAction(ResetAction, ETriggerEvent::Started, this, &ThisClass::ResetBenchmark);
	}
}

/**
 * @brief Selects native ISM and emits the existing ISN bookmark on success.
 * @note Does nothing when native ISM is already active.
 */
void ABenchmarkPlayerController::SelectISM()
{
	// The benchmark level contains one benchmark actor.
	for (TActorIterator<AMassRepBenchmarkActor> It(GetWorld()); It; ++It)
	{
		if (It->GetCurrentRepresentation() == EBenchmarkRepresentation::ISM)
		{
			return;
		}

		if (It->EnableISM())
		{
			ConsoleCommand(TEXT("trace.bookmark ISN"));
			ShowBenchmarkMessage(TEXT("ISM enabled"));
		}
		else
		{
			ShowBenchmarkMessage(TEXT("Benchmark entities unavailable"));
		}

		return;
	}

	ShowBenchmarkMessage(TEXT("Benchmark actor not found"));
}

/**
 * @brief Selects Niagara and emits the NDC bookmark on success.
 * @note Does nothing when Niagara is already active.
 */
void ABenchmarkPlayerController::SelectNiagara()
{
	// The level contains one benchmark actor.
	for (TActorIterator<AMassRepBenchmarkActor> It(GetWorld()); It; ++It)
	{
		if (It->GetCurrentRepresentation() == EBenchmarkRepresentation::Niagara)
		{
			return;
		}

		if (It->EnableNiagara())
		{
			ConsoleCommand(TEXT("trace.bookmark NDC"));
			ShowBenchmarkMessage(TEXT("Niagara enabled"));
		}
		else
		{
			ShowBenchmarkMessage(TEXT("Niagara assets or benchmark entities unavailable"));
		}

		return;
	}

	ShowBenchmarkMessage(TEXT("Benchmark actor not found"));
}

/**
 * @brief Disables all representations on the level's first benchmark actor.
 * @note Does nothing when representation is already disabled.
 */
void ABenchmarkPlayerController::SelectNone()
{
	// Change representation on the existing benchmark entities.
	for (TActorIterator<AMassRepBenchmarkActor> It(GetWorld()); It; ++It)
	{
		if (It->GetCurrentRepresentation() == EBenchmarkRepresentation::None)
		{
			return;
		}

		if (It->DisableRepresentation())
		{
			ShowBenchmarkMessage(TEXT("Representation disabled"));
		}
		else
		{
			ShowBenchmarkMessage(TEXT("Benchmark entities unavailable"));
		}

		return;
	}

	ShowBenchmarkMessage(TEXT("Benchmark actor not found"));
}

/**
 * @brief Reports a reset request; simulation reset is currently a placeholder.
 */
void ABenchmarkPlayerController::ResetBenchmark()
{
	ShowBenchmarkMessage(TEXT("Reset requested"));
}

/**
 * @brief Writes status to the output log and, when available, the on-screen display.
 * @param Message Status text to log and display.
 */
void ABenchmarkPlayerController::ShowBenchmarkMessage(const FString& Message) const
{
	UE_LOG(LogTemp, Display, TEXT("Benchmark: %s"), *Message);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 3.0f, FColor::Cyan, Message);
	}
}

/**
 * @brief Selects bulk ISM and emits CUSTOM_ISM on success.
 * @note Does nothing when custom ISM is already active.
 */
void ABenchmarkPlayerController::SelectCustomISM()
{
	for (TActorIterator<AMassRepBenchmarkActor> It(GetWorld()); It; ++It)
	{
		if (It->GetCurrentRepresentation() == EBenchmarkRepresentation::CustomISM)
		{
			return;
		}

		if (It->EnableCustomISM())
		{
			ConsoleCommand(TEXT("trace.bookmark CUSTOM_ISM"));
			ShowBenchmarkMessage(TEXT("Custom bulk ISM enabled"));
		}
		else
		{
			ShowBenchmarkMessage(TEXT("Benchmark entities unavailable"));
		}
		return;
	}
	ShowBenchmarkMessage(TEXT("Benchmark actor not found"));
}

/**
 * @brief Builds runtime Enhanced Input assets without requiring Blueprint configuration.
 * @note BeginPlay registers the context with the local player's input subsystem.
 */
void ABenchmarkPlayerController::InitializeCustomISMInput()
{
	if (!CustomISMAction)
	{
		CustomISMAction = NewObject<UInputAction>(this);
		CustomISMAction->ValueType = EInputActionValueType::Boolean;
	}
	if (!CustomISMMappingContext)
	{
		CustomISMMappingContext = NewObject<UInputMappingContext>(this);
		CustomISMMappingContext->MapKey(CustomISMAction, EKeys::Three);
	}
}


