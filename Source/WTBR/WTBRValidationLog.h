// Copyright Vaelborne: Dominion. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

bool WTBRShouldLogValidation();

#define WTBR_VALIDATION_LOG(Verbosity, Format, ...) \
    do \
    { \
        if (WTBRShouldLogValidation()) \
        { \
            UE_LOG(LogTemp, Verbosity, Format, ##__VA_ARGS__); \
        } \
    } while (false)
