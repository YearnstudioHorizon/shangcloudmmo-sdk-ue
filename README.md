# ShangCloud MMO Unreal Engine SDK

适用于 Unreal Engine 的 ShangCloudMMO 实时通信插件，提供与主控服务器的 HTTP API 通信以及与边缘节点的 AES-256-GCM 加密实时通信（TCP/UDP/WebSocket）。

## 环境要求

- Unreal Engine 5.0 或更高版本
- C++ 项目（需要编译插件源码）
- 引擎自带模块：`Sockets`、`Networking`、`HTTP`、`Json`、`WebSockets`、`OpenSSL`

## 安装

1. 将 `shangcloud-sdk-mmo-ue` 文件夹复制到项目的 `Plugins/` 目录下，重命名为 `ShangCloudMMO`
2. 重新生成项目文件（右键 `.uproject` → Generate Visual Studio project files）
3. 编译项目

安装后结构：

```
YourProject/
  Plugins/
    ShangCloudMMO/
      ShangCloudMMO.uplugin
      Source/
        ShangCloudMMO/
          ...
```

## 架构概览

```
┌───────────────────────────┐   ConfigureFromApiResponse()   ┌───────────────────────────┐
│   Part 1: API 客户端       │ ────────────────────────────▶  │   Part 2: MMO 实时通信      │
│   UShangCloudApiClient     │  connect_key + edge_url        │   UShangCloudMmoComponent   │
│   (FHttpModule 异步请求)    │                                │   (TCP/UDP/WebSocket)       │
└───────────────────────────┘                                └───────────────────────────┘
```

- **API 客户端** (`UShangCloudApiClient`)：基于 `FHttpModule` 的异步 HTTP 客户端，蓝图可用
- **MMO 通信组件** (`UShangCloudMmoComponent`)：`UActorComponent`，可直接添加到 Actor 上，蓝图可用

## 快速开始（C++）

### 1. 在 Build.cs 中添加模块依赖

```csharp
PublicDependencyModuleNames.Add("ShangCloudMMO");
```

### 2. 创建房间并连接

```cpp
#include "ShangCloudApiClient.h"
#include "ShangCloudMmoComponent.h"

// 在 Actor 中
void AMyActor::BeginPlay()
{
    Super::BeginPlay();

    // 添加 MMO 组件
    MmoComponent = NewObject<UShangCloudMmoComponent>(this);
    MmoComponent->RegisterComponent();

    // 注册事件
    MmoComponent->OnConnected.AddDynamic(this, &AMyActor::OnMmoConnected);
    MmoComponent->OnMessageReceived.AddDynamic(this, &AMyActor::OnMmoMessage);
    MmoComponent->OnUserJoined.AddDynamic(this, &AMyActor::OnMmoUserJoined);
    MmoComponent->OnUserLeft.AddDynamic(this, &AMyActor::OnMmoUserLeft);
    MmoComponent->OnConnectionError.AddDynamic(this, &AMyActor::OnMmoError);
    MmoComponent->OnDisconnected.AddDynamic(this, &AMyActor::OnMmoDisconnected);

    // 创建 API 客户端
    ApiClient = NewObject<UShangCloudApiClient>();
    ApiClient->BaseUrl = TEXT("https://api.yearnstudio.cn");
    ApiClient->AccessToken = TEXT("your_access_token");
    ApiClient->TokenType = TEXT("Bearer");

    // 创建房间
    FOnApiNewRoom Callback;
    Callback.BindDynamic(this, &AMyActor::OnRoomCreated);
    ApiClient->NewRoom(TEXT("tcp"), Callback);
}

void AMyActor::OnRoomCreated(const FMmoNewRoomResponse& Response, const FString& Error)
{
    if (!Error.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("API error: %s"), *Error);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("Room created: %s"), *Response.RoomId);

    // 配置并连接
    MmoComponent->ConfigureFromApiResponse(Response.ConnectKey, Response.EdgeUrl, Response.Protocol);
    MmoComponent->ConnectToEdge();
}

void AMyActor::OnMmoConnected()
{
    UE_LOG(LogTemp, Log, TEXT("Connected to edge!"));
    MmoComponent->SendMessage(TEXT("{\"type\":\"__join__\",\"uid\":\"player1\",\"nickname\":\"Player One\"}"));
}
```

### 3. 加入已有房间

```cpp
FOnApiJoinRoom Callback;
Callback.BindDynamic(this, &AMyActor::OnRoomJoined);
ApiClient->JoinRoom(RoomId, TEXT("tcp"), Callback);
```

## 蓝图使用

所有 API 和组件均支持蓝图：

1. **添加组件**：在 Actor 蓝图中添加 `ShangCloudMmoComponent`
2. **创建 API 客户端**：使用 `Construct Object from Class` 节点创建 `ShangCloudApiClient`
3. **设置属性**：设置 `AccessToken`、`BaseUrl`、`TokenType`
4. **调用接口**：调用 `New Room`、`Join Room` 等蓝图函数
5. **绑定事件**：右键组件的事件（OnConnected、OnMessageReceived 等）→ Add Event

## API 客户端详细用法

`UShangCloudApiClient` 是一个 `UObject`，所有接口均为异步回调模式。

### 房间管理

```cpp
// 创建房间，protocol 可选 "tcp" / "websocket"
ApiClient->NewRoom(TEXT("tcp"), OnNewRoomCallback);

// 加入房间
ApiClient->JoinRoom(RoomId, TEXT("tcp"), OnJoinRoomCallback);

// 设置房间配置（仅房主）
ApiClient->SetRoomConfig(RoomId, false, OnSimpleCallback);

// 踢出用户（仅房主）
ApiClient->KickUser(RoomId, TEXT("12345"), OnSimpleCallback);

// 查询房间人数
ApiClient->GetRoomUserCount(RoomId, OnUserCountCallback);
```

### 房间数据（键值存储）

```cpp
// 设置数据（仅房主），Type 可选 "number" / "string" / "boolean"
ApiClient->SetRoomData(RoomId, TEXT("score"), TEXT("100"), TEXT("number"), OnSimpleCallback);

// 获取所有数据（返回 JSON 字符串）
ApiClient->GetRoomData(RoomId, OnGetDataCallback);

// 删除数据（仅房主）
ApiClient->DeleteRoomData(RoomId, TEXT("score"), OnSimpleCallback);
```

### 回调签名

| 委托 | 参数 | 用于 |
|------|------|------|
| `FOnApiNewRoom` | `Response`, `Error` | NewRoom |
| `FOnApiJoinRoom` | `Response`, `Error` | JoinRoom |
| `FOnApiSimple` | `Error` | Config/Data/Kick |
| `FOnApiGetRoomData` | `JsonData`, `Error` | GetRoomData |
| `FOnApiUserCount` | `UserCount`, `Error` | GetRoomUserCount |

`Error` 为空字符串表示成功。

## ShangCloudMmoComponent

`UShangCloudMmoComponent` 是一个 `UActorComponent`，可添加到任意 Actor。

### 属性

| 属性 | 类型 | 说明 |
|------|------|------|
| Protocol | `EMmoProtocol` | 通信协议：TCP / UDP / WebSocket |
| ConnectKey | `FString` | 由 API 返回的连接密钥 |
| EdgeHost | `FString` | 边缘节点主机地址 |
| EdgePort | `int32` | 边缘节点端口 |
| EdgeUrl | `FString` | WebSocket 完整 URL |
| ConnectionState | `EMmoConnectionState` | 当前连接状态（只读） |

### 事件（BlueprintAssignable）

| 事件 | 参数 | 说明 |
|------|------|------|
| `OnConnected` | 无 | 连接成功 |
| `OnDisconnected` | 无 | 连接断开 |
| `OnConnectionError` | `Error` | 连接错误 |
| `OnMessageReceived` | `Message` | 收到业务消息 |
| `OnRawMessageReceived` | `Data` | 收到二进制消息 |
| `OnUserJoined` | `Uid`, `Nickname` | 用户加入 |
| `OnUserLeft` | `Uid` | 用户离开 |
| `OnServerClosed` | 无 | 服务端关闭连接 |

### 方法

| 方法 | 说明 |
|------|------|
| `ConfigureFromApiResponse(ConnectKey, EdgeUrl, Protocol)` | 从 API 响应配置参数 |
| `ConnectToEdge()` | 连接边缘节点 |
| `DisconnectFromEdge()` | 断开连接 |
| `SendMessage(FString)` | 发送文本消息 |
| `SendRaw(TArray<uint8>)` | 发送二进制数据 |

## 通信协议

### 安全机制

- **算法**：AES-256-GCM（通过 UE 内置的 OpenSSL 实现）
- **密钥派生**：客户端生成 32 字节随机 Seed，通过 SHA-256 派生 AES 密钥
- **载荷结构**：`[12B Nonce][AES-GCM 密文(8B 时间戳ms + 实际数据 + 16B Tag)]`
- **防重放**：20 秒滑动窗口

### TCP

使用长度前缀帧：`[4B 大端长度][加密载荷]`

连接流程：发送 32B Seed → 发送加密的 connect_key → 等待 `__auth_ok__` → 已连接

后台线程 (`FRunnableThread`) 处理接收，通过 `FMmoMessageQueue` 将消息传递到游戏线程。

### UDP

使用 connectId 进行会话绑定：`[8B connectId 大端][加密载荷]`

连接流程：发送 `[8B connectId=0][32B Seed][加密 connect_key]` → 接收 `__auth_ok__` → 已连接

支持 NAT 恢复（15 秒无数据自动重绑 Socket）。后台线程处理接收。

### WebSocket

使用 UE 内置的 `IWebSocket` 接口，运行在游戏线程（UE WebSocket 模块内部管理异步 I/O）。

连接流程：WebSocket 握手 → 发送 32B Seed → 发送加密 connect_key → 等待 `__auth_ok__` → 已连接

### 心跳

客户端每 3 秒自动发送 `__hb__` 心跳包。

## 线程模型

```
游戏线程 (Game Thread)                  后台工作线程 (FRunnableThread)
┌─────────────────────────┐            ┌─────────────────────────┐
│ UShangCloudMmoComponent │            │ TCP: FSocket::Recv()    │
│   ::TickComponent()     │            │ UDP: FSocket::RecvFrom()│
│  ├─ Transport->Poll()   │            │                         │
│  ├─ DrainAll() 消息队列  │◄── 队列 ───│ 解密 → Enqueue          │
│  ├─ Broadcast 事件      │ (FCritical │                         │
│  └─ 更新 ConnectionState│  Section)  │                         │
└─────────────────────────┘            └─────────────────────────┘
```

WebSocket 运行在游戏线程（通过 UE WebSocket 模块的异步回调）。

所有事件均在游戏线程广播，可安全调用 UE API。

## 文件结构

```
ShangCloudMMO/
  ShangCloudMMO.uplugin                      # 插件描述
  Source/ShangCloudMMO/
    ShangCloudMMO.Build.cs                   # 构建配置
    Public/
      ShangCloudMMOModule.h                  # 模块接口
      MmoTypes.h                             # 枚举、消息结构体
      MmoMessageQueue.h                      # 线程安全消息队列
      MmoCrypto.h                            # AES-256-GCM 加解密（OpenSSL）
      MmoTransport.h                         # 传输层基类
      MmoTcpTransport.h                      # TCP 传输（FSocket + FRunnableThread）
      MmoUdpTransport.h                      # UDP 传输（FSocket + FRunnableThread）
      MmoWebSocketTransport.h                # WebSocket 传输（IWebSocket）
      MmoApiModels.h                         # API 响应结构体（USTRUCT）
      ShangCloudApiClient.h                  # HTTP API 客户端（UCLASS）
      ShangCloudMmoComponent.h               # 主组件（UActorComponent）
    Private/
      ShangCloudMMOModule.cpp
      MmoMessageQueue.cpp
      MmoCrypto.cpp
      MmoTransport.cpp
      MmoTcpTransport.cpp
      MmoUdpTransport.cpp
      MmoWebSocketTransport.cpp
      MmoApiModels.cpp
      ShangCloudApiClient.cpp
      ShangCloudMmoComponent.cpp
```

## 典型流程

```
1. 房主创建房间
   ApiClient->NewRoom("tcp", Callback)  →  拿到 ConnectKey + EdgeUrl + RoomId

2. 其他玩家加入房间
   ApiClient->JoinRoom(RoomId, "tcp", Callback)  →  拿到 ConnectKey + EdgeUrl

3. 配置并连接边缘节点
   MmoComponent->ConfigureFromApiResponse(ConnectKey, EdgeUrl, Protocol)
   MmoComponent->ConnectToEdge()

4. 连接成功后发送加入消息
   MmoComponent->SendMessage("{\"type\":\"__join__\",\"uid\":\"...\",\"nickname\":\"...\"}")

5. 房间内操作（通过 API 客户端）
   - 设置房间数据：ApiClient->SetRoomData(...)
   - 读取房间数据：ApiClient->GetRoomData(...)
   - 踢人：ApiClient->KickUser(...)
   - 查人数：ApiClient->GetRoomUserCount(...)

6. 断开连接
   MmoComponent->DisconnectFromEdge()
```

## 许可证

MIT

## 备注

该项目由`Claude Opus 4.6`生成
