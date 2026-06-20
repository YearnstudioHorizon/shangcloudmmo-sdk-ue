#pragma once

#include "CoreMinimal.h"
#include "MmoTypes.h"

class SHANGCLOUDMMO_API FMmoMessageQueue
{
public:
	void Enqueue(FMmoMessage&& Msg);
	TArray<FMmoMessage> DrainAll();
	bool IsEmpty() const;

private:
	FCriticalSection Mutex;
	TArray<FMmoMessage> Queue;
};
