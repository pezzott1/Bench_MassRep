/**
 * @file MassRepBenchmarkActor.cpp
 * @brief Creates entities and manages visual representation setup and cleanup.
 */

#include "MassRepBenchmarkActor.h"
#include "UObject/Class.h"
#include "BenchmarkNiagaraSettingsSubsystem.h"
#include "BenchmarkISMSettingsSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/CollisionProfile.h"

#include "Mass/EntityFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"

#include "MassEntityTypes.h"
#include "MassLODFragments.h"
#include "MassRepresentationFragments.h"
#include "MassRepresentationSubsystem.h"
#include "MassRepresentationTypes.h"

#include "Components/SceneComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraDataChannelAsset.h"



/**
 * @brief Creates the root and initially inactive Niagara and custom ISM components.
 */
AMassRepBenchmarkActor::AMassRepBenchmarkActor()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("BenchmarkRoot"));

	BenchmarkNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("BenchmarkNiagara"));

	BenchmarkNiagaraComponent->SetupAttachment(RootComponent);
	BenchmarkNiagaraComponent->SetAutoActivate(false);
	BenchmarkNiagaraComponent->SetCastShadow(false);

	// Channel positions are world-space. Keep the component at world origin.
	BenchmarkNiagaraComponent->SetAbsolute(true, true, true);

	BenchmarkISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BenchmarkCustomISM"));
	BenchmarkISMComponent->SetupAttachment(RootComponent);
	BenchmarkISMComponent->SetAbsolute(true, true, true);
	BenchmarkISMComponent->SetMobility(EComponentMobility::Movable);
	BenchmarkISMComponent->SetCanEverAffectNavigation(false);
	BenchmarkISMComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BenchmarkISMComponent->SetCastShadow(false);
	BenchmarkISMComponent->SetReceivesDecals(false);
	BenchmarkISMComponent->SetCullDistances(0, 1000000);
	BenchmarkISMComponent->SetVisibility(false);
}

/**
 * @brief Creates one shared mesh description and a deterministic grid of benchmark entities.
 */
void AMassRepBenchmarkActor::BeginPlay()
{
	Super::BeginPlay();

	checkf(BenchmarkMesh, TEXT("Benchmark: assign BenchmarkMesh before playing."));

	UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();

	UMassRepresentationSubsystem* RepresentationSubsystem = GetWorld()->GetSubsystem<UMassRepresentationSubsystem>();

	checkf(MassSubsystem && RepresentationSubsystem, TEXT("Benchmark requires Mass Entity and Representation subsystems."));

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	// Register exactly one mesh entry: one ISM instance per entity.
	FStaticMeshInstanceVisualizationDesc MeshDescription;

	FMassStaticMeshInstanceVisualizationMeshDesc& MeshEntry = MeshDescription.Meshes.AddDefaulted_GetRef();

	MeshEntry.Mesh = BenchmarkMesh;
	MeshEntry.MinLODSignificance = 0.0f;
	MeshEntry.MaxLODSignificance = 4.0f;
	MeshEntry.bCastShadows = false;

	if (BenchmarkMaterial)
	{
		MeshEntry.MaterialOverrides.Add(BenchmarkMaterial);
	}

	const FStaticMeshInstanceVisualizationDescHandle MeshHandle = RepresentationSubsystem->FindOrAddStaticMeshDesc(MeshDescription);

	checkf(MeshHandle.IsValid(), TEXT("Benchmark mesh registration failed."));

	// Native UMassUpdateISMProcessor reads this shared subsystem reference.
	FMassRepresentationSubsystemSharedFragment SubsystemFragment;
	SubsystemFragment.RepresentationSubsystem = RepresentationSubsystem;

	FMassArchetypeSharedFragmentValues SharedValues;
	SharedValues.Add(EntityManager.GetOrCreateSharedFragment(SubsystemFragment));
	SharedValues.Sort();

	// Supply the fragments required by the native ISM update processor.
	FMassArchetypeCompositionDescriptor Composition;
	Composition.Add<FTransformFragment>();
	Composition.Add<FBenchmarkNiagaraVisualFragment>();
	Composition.Add<FBenchmarkMovementFragment>();
	Composition.Add<FBenchmarkISMInstanceFragment>();
	Composition.Add<FMassRepresentationFragment>();
	Composition.Add<FMassRepresentationLODFragment>();
	Composition.Add<FMassVisualizationChunkFragment>();
	Composition.Add<FMassRepresentationSubsystemSharedFragment>();

	const FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(Composition);

	const int32 Count = FMath::Max(1, EntityCount);
	const int32 Columns = FMath::Max(1, GridColumns);
	const float Spacing = FMath::Max(1.0f, GridSpacing);

	Entities.Reserve(Count);
	UBenchmarkNiagaraSettingsSubsystem* NiagaraSettings = GetWorld()->GetSubsystem<UBenchmarkNiagaraSettingsSubsystem>();
	NiagaraSettings->Channel = BenchmarkDataChannel;
	NiagaraSettings->SlotCapacity = Count;
	NiagaraSettings->bPublicationEnabled = false;

	BenchmarkISMComponent->SetWorldTransform(FTransform::Identity);
	BenchmarkISMComponent->SetStaticMesh(BenchmarkMesh);
	if (BenchmarkMaterial)
	{
		BenchmarkISMComponent->SetMaterial(0, BenchmarkMaterial);
	}
	UBenchmarkISMSettingsSubsystem* ISMSettings = GetWorld()->GetSubsystem<UBenchmarkISMSettingsSubsystem>();
	ISMSettings->Component = BenchmarkISMComponent;
	ISMSettings->bUpdatesEnabled = false;

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype, SharedValues);

		const FVector LocalPosition((Index % Columns) * Spacing, (Index / Columns) * Spacing, 0.0f);

		FTransform Transform = GetActorTransform();
		Transform.SetLocation(GetActorTransform().TransformPosition(LocalPosition));

		EntityManager.GetFragmentDataChecked<FTransformFragment>(Entity).SetTransform(Transform);

		// Fix representation at ISM without spawning individual actors.
		FMassRepresentationFragment& Representation = EntityManager.GetFragmentDataChecked<FMassRepresentationFragment>(Entity);

		Representation.StaticMeshDescHandle = MeshHandle;
		// Keep entities invisible until ISM is selected.
		Representation.CurrentRepresentation = EMassRepresentationType::None;
		Representation.PrevTransform = Transform;
		Representation.PrevLODSignificance = -1.0f;

		// No distance-based representation changes during this comparison.
		FMassRepresentationLODFragment& RepresentationLOD = EntityManager.GetFragmentDataChecked<FMassRepresentationLODFragment>(Entity);

		RepresentationLOD.LOD = EMassLOD::High;
		RepresentationLOD.PrevLOD = EMassLOD::High;
		RepresentationLOD.LODSignificance = 0.0f;
		RepresentationLOD.Visibility = EMassVisibility::CanBeSeen;
		RepresentationLOD.PrevVisibility = EMassVisibility::CanBeSeen;
		FBenchmarkMovementFragment& Movement = EntityManager.GetFragmentDataChecked<FBenchmarkMovementFragment>(Entity);
		Movement.Origin = Transform.GetLocation();
		Movement.Amplitude = MovementAmplitude;
		Movement.Frequency = MovementFrequency;
		Movement.Phase = (Index % Columns) * 0.1f;
		EntityManager.GetFragmentDataChecked<FBenchmarkISMInstanceFragment>(Entity).InstanceIndex = Index;
		Entities.Add(Entity);
	}

	UE_LOG(LogTemp, Display, TEXT("Benchmark: created %d Mass entities without representation."), Entities.Num());
}

/**
 * @brief Stops visual output and releases owned entities, instances, and subsystem references.
 * @param EndPlayReason Engine-provided reason for ending play.
 */
void AMassRepBenchmarkActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetRepresentation(EBenchmarkRepresentation::None);
	
	if (UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>())
	{
		FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

		for (const FMassEntityHandle Entity : Entities)
		{
			if (EntityManager.IsEntityValid(Entity))
			{
				EntityManager.DestroyEntity(Entity);
			}
		}
	}

	Entities.Reset();
	if (UBenchmarkNiagaraSettingsSubsystem* Settings = GetWorld()->GetSubsystem<UBenchmarkNiagaraSettingsSubsystem>())
	{
		Settings->bPublicationEnabled = false;
		Settings->SlotCapacity = 0;
		Settings->Channel = nullptr;
	}

	if (UBenchmarkISMSettingsSubsystem* Settings = GetWorld()->GetSubsystem<UBenchmarkISMSettingsSubsystem>())
	{
		Settings->bUpdatesEnabled = false;
		Settings->Component = nullptr;
		Settings->CurrentTransforms.Reset();
		Settings->PreviousTransforms.Reset();
	}
	BenchmarkISMComponent->ClearInstances();

	Super::EndPlay(EndPlayReason);
}

/**
 * @brief Validates and switches visual modes while retaining the existing simulation.
 * @param NewRepresentation Requested benchmark representation.
 * @return True when selected; false if the Mass subsystem or benchmark entities are unavailable.
 */
bool AMassRepBenchmarkActor::SetRepresentation(EBenchmarkRepresentation NewRepresentation)
{
	// Assert configuration invariants before changing the active representation.
	const UEnum* RepresentationEnum = StaticEnum<EBenchmarkRepresentation>();
	const int64 RepresentationValue = static_cast<int64>(NewRepresentation);
	checkf(RepresentationEnum->IsValidEnumValue(
		RepresentationValue) && RepresentationValue != RepresentationEnum->GetMaxEnumValue(), 
		TEXT("Benchmark: invalid representation value %lld."), RepresentationValue);

	checkf(NewRepresentation != EBenchmarkRepresentation::Niagara || (BenchmarkNiagaraSystem && BenchmarkDataChannel && BenchmarkNiagaraComponent),
		TEXT("Benchmark: assign the Niagara system and Data Channel; a Niagara component is required."));

	UMassEntitySubsystem* MassSubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem || Entities.IsEmpty())
	{
		return false;
	}

	if (CurrentRepresentation == NewRepresentation)
	{
		return true;
	}

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	const EMassRepresentationType MassRepresentation = (NewRepresentation == EBenchmarkRepresentation::ISM) ? EMassRepresentationType::StaticMeshInstance : EMassRepresentationType::None;

	for (const FMassEntityHandle Entity : Entities)
	{
		if (!EntityManager.IsEntityValid(Entity))
		{
			continue;
		}

		FMassRepresentationFragment& Representation = EntityManager.GetFragmentDataChecked<FMassRepresentationFragment>(Entity);
		if (Representation.CurrentRepresentation == MassRepresentation)
		{
			continue;
		}

		Representation.PrevRepresentation = Representation.CurrentRepresentation;
		Representation.CurrentRepresentation = MassRepresentation;
		if (MassRepresentation == EMassRepresentationType::StaticMeshInstance)
		{
			Representation.PrevTransform = EntityManager.GetFragmentDataChecked<FTransformFragment>(Entity).GetTransform();
			Representation.PrevLODSignificance = -1.0f;
		}
	}

	UBenchmarkISMSettingsSubsystem* ISMSettings = GetWorld()->GetSubsystem<UBenchmarkISMSettingsSubsystem>();
	ISMSettings->bUpdatesEnabled = false;
	if (NewRepresentation == EBenchmarkRepresentation::CustomISM)
	{
		/**
		 * @brief Seed instances and transform history once when entering custom ISM mode.
		 * @note Fragment indices match the fixed entity array; individual removals are unsupported.
		 */
		BenchmarkISMComponent->ClearInstances();
		BenchmarkISMComponent->SetWorldTransform(FTransform::Identity);
		ISMSettings->CurrentTransforms.SetNum(Entities.Num());
		for (int32 Index = 0; Index < Entities.Num(); ++Index)
		{
			ISMSettings->CurrentTransforms[Index] = EntityManager.GetFragmentDataChecked<FTransformFragment>(Entities[Index]).GetTransform();
		}
		ISMSettings->PreviousTransforms = ISMSettings->CurrentTransforms;
		BenchmarkISMComponent->AddInstances(ISMSettings->CurrentTransforms, /*bShouldReturnIndices=*/false, /*bWorldSpace=*/false, /*bUpdateNavigation=*/false);
		BenchmarkISMComponent->SetHasPerInstancePrevTransforms(true);
		BenchmarkISMComponent->SetVisibility(true);
		ISMSettings->bUpdatesEnabled = true;
	}
	else
	{
		BenchmarkISMComponent->SetVisibility(false);
		BenchmarkISMComponent->ClearInstances();
		ISMSettings->CurrentTransforms.Reset();
		ISMSettings->PreviousTransforms.Reset();
	}

	CurrentRepresentation = NewRepresentation;
	if (UBenchmarkNiagaraSettingsSubsystem* Settings = GetWorld()->GetSubsystem<UBenchmarkNiagaraSettingsSubsystem>())
	{
		Settings->Channel = BenchmarkDataChannel;
		Settings->bPublicationEnabled = NewRepresentation == EBenchmarkRepresentation::Niagara;
		if (Settings->bPublicationEnabled)
		{
			++Settings->PublicationRevision;
		}
	}
	switch (CurrentRepresentation)
	{
	case EBenchmarkRepresentation::None:
	case EBenchmarkRepresentation::ISM:
	case EBenchmarkRepresentation::CustomISM:
		if (BenchmarkNiagaraComponent)
		{
			// Persistent particles must stop immediately when changing modes.
			BenchmarkNiagaraComponent->DeactivateImmediate();
			BenchmarkNiagaraComponent->SetVisibility(false);
		}
		break;
	case EBenchmarkRepresentation::Niagara:
		BenchmarkNiagaraComponent->SetWorldTransform(FTransform::Identity);
		BenchmarkNiagaraComponent->SetAsset(BenchmarkNiagaraSystem);
		BenchmarkNiagaraComponent->SetVariableInt(TEXT("User.EntityCount"), Entities.Num());
		BenchmarkNiagaraComponent->SetVisibility(true);
		BenchmarkNiagaraComponent->Activate(true);
		break;
	}

	return true;
}

/**
 * @brief Selects native Mass ISM representation.
 * @return True when selected; false if the Mass subsystem or benchmark entities are unavailable.
 */
bool AMassRepBenchmarkActor::EnableISM()
{
	return SetRepresentation(EBenchmarkRepresentation::ISM);
}

/**
 * @brief Selects simulation without visible representation.
 * @return True when selected; false if the Mass subsystem or benchmark entities are unavailable.
 */
bool AMassRepBenchmarkActor::DisableRepresentation()
{
	return SetRepresentation(EBenchmarkRepresentation::None);
}

/**
 * @brief Selects Niagara Data Channel representation.
 * @return True when selected; false if the Mass subsystem or benchmark entities are unavailable.
 */
bool AMassRepBenchmarkActor::EnableNiagara()
{
	return SetRepresentation(EBenchmarkRepresentation::Niagara);
}


/**
 * @brief Selects custom bulk ISM representation.
 * @return True when selected; false if the Mass subsystem or benchmark entities are unavailable.
 */
bool AMassRepBenchmarkActor::EnableCustomISM()
{
	return SetRepresentation(EBenchmarkRepresentation::CustomISM);
}


