#pragma once

#include "CoreMinimal.h"

/**
 * 插帧同步引擎（移植自 core.js 的 _ensureInterpLoop / _mmoInterpState）。
 *
 * 接收端按 uid / varName 维护 { current, target } 状态，在 TickComponent 里逐帧
 * 用帧率无关的指数平滑把 current 逼近 target。规则与 core.js 完全一致：
 *   - 首次收到：snap
 *   - 瞬移阈值（|target - current| >= 200）：snap
 *   - 正常：只更新 target，由 Tick 做平滑写入 current
 *   - 动态追赶：|diff| > 50 时提高因子（上限 0.8）
 *   - 自动休眠：所有变量均到达 target 时 Tick 直接返回空
 *
 * 非数值、或不在 interp 集合中的变量，不参与插帧，仅存原始值供 GetSyncVar 回退。
 */
class SHANGCLOUDMMO_API FMmoInterpEngine
{
public:
	struct FInterpState
	{
		double Current = 0.0;
		double Target = 0.0;
	};

	struct FChange
	{
		FString Uid;
		FString VarName;
		double Value = 0.0;
	};

	FMmoInterpEngine();

	/** 应用一次 __sync_var__ 更新。Vars: varName -> 值（字符串）；Interp: 需要 interp 的 varName 集合（可为空，沿用已有集合） */
	void ApplySync(const FString& Uid, const TMap<FString, FString>& Vars, const TArray<FString>* Interp);

	/** 在 TickComponent 中调用，推进所有插帧变量 current → target。返回本次发生显著变化的列表。 */
	TArray<FChange> Tick(float DeltaSeconds);

	/** 读取插帧变量当前值（平滑后的 current）；不在插帧集合或尚未建立状态时回退到最近原始值 */
	double GetSyncVar(const FString& Uid, const FString& Name) const;

	/** 读取原始字符串值（不做插帧） */
	FString GetSyncVarRaw(const FString& Uid, const FString& Name) const;

	/** 清理指定 uid 的状态（玩家离开时调用） */
	void ClearUid(const FString& Uid);

	/** 清空全部状态（断开连接时调用） */
	void Clear();

private:
	static constexpr double BaseFactor = 0.15;         // _MMO_INTERP_BASE_FACTOR
	static constexpr double TeleportThreshold = 200.0; // _MMO_INTERP_TELEPORT_THRESHOLD
	static constexpr double Epsilon = 0.001;
	static constexpr double FrameRefMs = 16.67;

	// uid -> (varName -> state)
	TMap<FString, TMap<FString, FInterpState>> State;
	// uid -> 需要 interp 的 varName 集合
	TMap<FString, TSet<FString>> InterpNames;
	// uid -> (varName -> 最近原始值)
	TMap<FString, TMap<FString, FString>> Raw;

	static bool TryParseDouble(const FString& In, double& Out);
};
