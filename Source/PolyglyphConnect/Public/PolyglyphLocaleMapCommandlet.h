// Copyright © ToaGames. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "PolyglyphLocaleMapCommandlet.generated.h"

/** Generates the shipped Unreal locale mapping catalog for the running engine version. */
UCLASS()
class POLYGLYPHCONNECT_API UPolyglyphLocaleMapCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	/** Configure commandlet execution for unattended catalog generation. */
	UPolyglyphLocaleMapCommandlet();

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& InParams) override;
	//~ End UCommandlet Interface
};
