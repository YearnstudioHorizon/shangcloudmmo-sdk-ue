#include "MmoMessageQueue.h"

void FMmoMessageQueue::Enqueue(FMmoMessage&& Msg)
{
	FScopeLock Lock(&Mutex);
	Queue.Add(MoveTemp(Msg));
}

TArray<FMmoMessage> FMmoMessageQueue::DrainAll()
{
	FScopeLock Lock(&Mutex);
	TArray<FMmoMessage> Result = MoveTemp(Queue);
	Queue.Reset();
	return Result;
}

bool FMmoMessageQueue::IsEmpty() const
{
	FScopeLock Lock(&const_cast<FMmoMessageQueue*>(this)->Mutex);
	return Queue.Num() == 0;
}
