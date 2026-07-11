#include "MmoInterpEngine.h"

#include <cmath>

FMmoInterpEngine::FMmoInterpEngine()
{
}

bool FMmoInterpEngine::TryParseDouble(const FString& In, double& Out)
{
	FString S = In.TrimStartAndEnd();
	if (S.IsEmpty()) return false;

	// 用 LexicalFromString 解析浮点（支持负数、小数、指数）
	if (!S.IsNumeric()) return false;
	Out = FCString::Atod(*S);
	return true;
}

void FMmoInterpEngine::ApplySync(const FString& Uid, const TMap<FString, FString>& Vars, const TArray<FString>* Interp)
{
	if (Uid.IsEmpty() || Vars.Num() == 0) return;

	TMap<FString, FInterpState>* ByVar = State.Find(Uid);
	if (!ByVar)
	{
		ByVar = &State.Add(Uid, TMap<FString, FInterpState>());
	}
	TMap<FString, FString>* RawByVar = Raw.Find(Uid);
	if (!RawByVar)
	{
		RawByVar = &Raw.Add(Uid, TMap<FString, FString>());
	}

	// 更新 interp 集合
	if (Interp)
	{
		TSet<FString> InterpSet;
		InterpSet.Reserve(Interp->Num());
		for (const FString& Name : *Interp)
		{
			InterpSet.Add(Name);
		}
		InterpNames[Uid] = MoveTemp(InterpSet);
	}
	const TSet<FString>* InterpSetPtr = InterpNames.Find(Uid);

	for (const auto& Pair : Vars)
	{
		const FString& Name = Pair.Key;
		FString V = Pair.Value;
		RawByVar->Add(Name, V);

		bool bNeedInterp = InterpSetPtr && InterpSetPtr->Contains(Name);
		if (!bNeedInterp) continue;

		double NumV = 0.0;
		if (!TryParseDouble(V, NumV)) continue; // 非数值不插帧

		FInterpState* StPtr = ByVar->Find(Name);
		if (!StPtr)
		{
			// 首次收到：snap
			ByVar->Add(Name, FInterpState{ NumV, NumV });
		}
		else if (FMath::Abs(NumV - StPtr->Current) >= TeleportThreshold)
		{
			// 瞬移检测：snap
			*StPtr = FInterpState{ NumV, NumV };
		}
		else
		{
			// 正常：仅更新 target
			StPtr->Target = NumV;
		}
	}
}

TArray<FMmoInterpEngine::FChange> FMmoInterpEngine::Tick(float DeltaSeconds)
{
	TArray<FChange> Changed;
	if (State.Num() == 0) return Changed;

	double DtMs = static_cast<double>(DeltaSeconds) * 1000.0;
	// 帧率无关平滑因子
	double BaseFactor = 1.0 - FMath::Pow(1.0 - BaseFactor, DtMs / FrameRefMs);

	for (auto& UidEntry : State)
	{
		const FString& Uid = UidEntry.Key;
		TMap<FString, FInterpState>& ByVar = UidEntry.Value;
		for (auto& NameEntry : ByVar)
		{
			const FString& Name = NameEntry.Key;
			FInterpState& St = NameEntry.Value;
			double Diff = St.Target - St.Current;

			if (FMath::Abs(Diff) <= Epsilon)
			{
				St.Current = St.Target;
				continue;
			}

			// 动态追赶：偏差过大时提高因子
			double Factor = BaseFactor;
			if (FMath::Abs(Diff) > 50.0)
			{
				Factor = FMath::Min(BaseFactor * (FMath::Abs(Diff) / 50.0), 0.8);
			}

			St.Current += Diff * Factor;

			// 脏检查：仅在变化显著时上报
			if (FMath::Abs(Diff * Factor) > Epsilon)
			{
				Changed.Add(FChange{ Uid, Name, St.Current });
			}
		}
	}
	return Changed;
}

double FMmoInterpEngine::GetSyncVar(const FString& Uid, const FString& Name) const
{
	if (const TMap<FString, FInterpState>* ByVar = State.Find(Uid))
	{
		if (const FInterpState* St = ByVar->Find(Name))
		{
			return St->Current;
		}
	}
	if (const TMap<FString, FString>* RawByVar = Raw.Find(Uid))
	{
		if (const FString* RawPtr = RawByVar->Find(Name))
		{
			double D = 0.0;
			if (TryParseDouble(*RawPtr, D))
			{
				return D;
			}
		}
	}
	return 0.0;
}

FString FMmoInterpEngine::GetSyncVarRaw(const FString& Uid, const FString& Name) const
{
	if (const TMap<FString, FString>* RawByVar = Raw.Find(Uid))
	{
		if (const FString* RawPtr = RawByVar->Find(Name))
		{
			return *RawPtr;
		}
	}
	return FString();
}

void FMmoInterpEngine::ClearUid(const FString& Uid)
{
	if (Uid.IsEmpty()) return;
	State.Remove(Uid);
	InterpNames.Remove(Uid);
	Raw.Remove(Uid);
}

void FMmoInterpEngine::Clear()
{
	State.Reset();
	InterpNames.Reset();
	Raw.Reset();
}
