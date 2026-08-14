// Copyright © ToaGames. All Rights Reserved.

#include "PolyglyphCommandletOverrides.h"

#include "HAL/PlatformMisc.h"
#include "Misc/Parse.h"

#include "PolyglyphProjectSettings.h"
#include "PolyglyphSettings.h"

namespace PolyglyphCommandletOverrides
{
    void Apply(const FString& InParams)
    {
        FString Value;
        if (FParse::Value(*InParams, TEXT("BaseUrl="), Value))
        {
            GetMutableDefault<UPolyglyphProjectSettings>()->BaseUrl = Value;
        }
        if (FParse::Value(*InParams, TEXT("ProjectSlug="), Value))
        {
            GetMutableDefault<UPolyglyphProjectSettings>()->ProjectSlug = Value;
        }
        if (FParse::Value(*InParams, TEXT("LocalizationTarget="), Value))
        {
            GetMutableDefault<UPolyglyphProjectSettings>()->LocalizationTarget = Value;
        }

        FString Key;
        if (!FParse::Value(*InParams, TEXT("ApiKey="), Key))
        {
            Key = FPlatformMisc::GetEnvironmentVariable(TEXT("POLYGLYPH_API_KEY"));
        }
        if (!Key.IsEmpty())
        {
            GetMutableDefault<UPolyglyphSettings>()->ApiKey = Key;
        }
    }
}
