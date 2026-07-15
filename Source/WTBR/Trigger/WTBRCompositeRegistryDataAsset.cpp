// Copyright Vaelborne: Dominion. All Rights Reserved.

#include "Trigger/WTBRCompositeRegistryDataAsset.h"

EWTBRCompositeBulletType UWTBRCompositeRegistryDataAsset::ResolveCompositeType(
    EWTBRBulletArchetype ArchetypeA,
    EWTBRBulletArchetype ArchetypeB) const
{
    if (ArchetypeA == EWTBRBulletArchetype::None ||
        ArchetypeB == EWTBRBulletArchetype::None ||
        ArchetypeA == EWTBRBulletArchetype::NonCombinable ||
        ArchetypeB == EWTBRBulletArchetype::NonCombinable)
    {
        return EWTBRCompositeBulletType::None;
    }

    for (const FWTBRCompositeDefinition& Definition : Definitions)
    {
        const bool bMatchesForward =
            Definition.RequiredArchetypeA == ArchetypeA &&
            Definition.RequiredArchetypeB == ArchetypeB;
        const bool bMatchesReverse =
            Definition.RequiredArchetypeA == ArchetypeB &&
            Definition.RequiredArchetypeB == ArchetypeA;

        if ((bMatchesForward || bMatchesReverse) &&
            Definition.CompositeType != EWTBRCompositeBulletType::None)
        {
            return Definition.CompositeType;
        }
    }

    return EWTBRCompositeBulletType::None;
}

bool UWTBRCompositeRegistryDataAsset::FindDefinition(
    EWTBRCompositeBulletType Type, FWTBRCompositeDefinition& OutDefinition) const
{
    if (Type == EWTBRCompositeBulletType::None) return false;

    for (const FWTBRCompositeDefinition& Definition : Definitions)
    {
        if (Definition.CompositeType == Type)
        {
            OutDefinition = Definition;
            return true;
        }
    }
    return false;
}
