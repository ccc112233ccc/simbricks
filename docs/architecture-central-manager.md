# 中心管理组件架构设计（草案）

## 背景与问题
当前仿真器采用 listener/connecter 两种角色：listener 需要创建共享内存队列并监听 socket，连接建立后再向对方发送共享内存队列地址等元信息。这一流程带来如下问题：

- 连接建立逻辑分散在各个仿真器内，增加实现与维护成本。
- 拓扑描述分散、隐式，难以配置化管理与快速变更。
- 端口协商与共享内存资源创建时机复杂，增加出错概率。

## 目标
引入一个“中心管理组件（Manager）”，其职责是：

1. 读取一个拓扑/配置文件，声明仿真器、端口、以及端口间的连接关系。
2. 根据拓扑自动创建通信 channel（IPC），并将必要的连接信息提供给各仿真器。
3. 启动仿真器进程，并注入其所需的通信端口配置。
4. 支持更成熟的 IPC 机制（例如 Unix Domain Socket、mmap+shm、消息队列等），以便权衡性能与开发成本。

## 总体架构

```
+--------------------+       +---------------------+
|  Topology Config   | ----> |   Central Manager   |
+--------------------+       +---------------------+
                                      |
                   +------------------+------------------+
                   |                                     |
        +----------v----------+               +----------v----------+
        |   Simulator Proc A  |               |   Simulator Proc B  |
        +---------------------+               +---------------------+
                   |                                     |
                   +------------- Channel --------------+
```

### 核心组件

- **Topology Config**
  - 配置文件描述仿真器列表、端口列表、端口间连接关系、启动参数等。
  - 建议以 JSON/YAML/TOML 为主，并提供 schema 校验。

- **Central Manager**
  - 解析拓扑配置。
  - 创建 channel 并管理资源生命周期。
  - 启动仿真器进程并向其下发端口与 channel 信息。
  - 提供统一日志与错误处理。

- **Simulator Process**
  - 启动时只需要读取 Manager 提供的端口信息。
  - 根据端口名称和 channel 信息进行连接，不再负责对端协商与资源创建。

## 配置模型设计（示例）

> 以下仅为结构示意，实际字段可根据现有项目命名调整。

```yaml
simulators:
  - name: nic0
    exec: ./sims/nic
    args: ["--foo", "bar"]
    ports:
      - name: net
        direction: bidirectional
  - name: host0
    exec: ./sims/host
    args: []
    ports:
      - name: net
        direction: bidirectional
links:
  - a: nic0.net
    b: host0.net
    channel:
      type: unix_socket
      options:
        path: /tmp/simbricks/nic0-host0.sock
```

### 配置要点
- `simulators[].ports[]` 中声明可用端口及方向。
- `links[]` 以 `simulator.port` 形式连接两端。
- `channel` 定义 IPC 类型与参数，例如：
  - `unix_socket`（易用、成熟）
  - `shm_ring`（高性能，共享内存+ring queue）
  - `posix_mq`（消息队列）

## 通信通道（Channel）抽象

定义统一的通道抽象，便于替换底层 IPC：

```
Channel
  - type
  - create()
  - info()
  - destroy()
```

- **create()**：由 Manager 创建底层 IPC 资源。
- **info()**：返回给仿真器的连接信息（地址、path、key 等）。
- **destroy()**：Manager 负责释放。

仿真器只需要根据 `info()` 的数据连接，不再参与通道创建流程。

## 启动流程（建议）

1. Manager 读取并验证配置文件。
2. Manager 为每个 `link` 创建 channel。
3. Manager 生成每个仿真器的端口运行参数（包括对端信息）。
4. Manager 启动仿真器进程，并注入端口配置（环境变量/命令行/配置文件）。
5. 仿真器启动后连接相应通道。

## 进程间信息注入方式

可选方式：

- **命令行参数**：简单直接，但参数量可能较大。
- **环境变量**：适用于小规模配置。
- **临时配置文件**：由 Manager 生成 per-simulator 配置文件，仿真器读取。

推荐：**临时配置文件**（结构清晰、可扩展、便于调试）。

## 可靠性与错误处理

- Manager 在创建 channel 时进行资源冲突检测（例如 socket path 冲突）。
- 仿真器启动失败时，Manager 应能清理已创建的资源并退出。
- 为每个 channel 与 simulator 关联明确日志前缀，便于诊断。

## 兼容性考虑

- 支持与当前 listener/connecter 模式共存的过渡模式。
- 通过 adapter 将旧模式封装成 channel type，逐步迁移。

## 初步落地建议

1. 实现最小可用的 Manager + 配置解析。
2. 先支持一种 IPC（如 unix socket 或当前 shm ring）。
3. 修改仿真器启动逻辑：接受 Manager 注入的端口信息。
4. 引入 schema 校验与更完整的拓扑检查。

## 后续扩展方向

- 复杂拓扑：多端口、多对多连接、广播/多播。
- 调度能力：依赖关系分析，按顺序启动。
- 可视化拓扑与运行状态监控。
- 配置热加载或可重启策略。

## 面向实现的任务说明（供 AI 开始任务）

本节给出基于现有 `sims/mem` 的最小可行实现方案与验证步骤，确保修改后至少支持 `basicmem` 和 `memstim` 的链接与运行。

### Manager 实现语言建议（Python 方案）

**结论：推荐先用 Python 实现 Manager。**

理由（面向最小可行版本）：
- Manager 的职责主要是解析配置、创建 IPC 资源、启动/管理进程、协调端口信息，属于**控制平面**，对性能要求相对较低。
- Python 在配置解析（YAML/JSON/TOML）、进程管理（`subprocess`）、以及本地 IPC 处理方面非常成熟，适合快速迭代拓扑与配置。
- 有利于在架构尚未稳定时快速试错，后续可根据需要迁移至 C/C++/Rust 或保留为长期实现。

需要注意的点：
- 资源生命周期管理（socket/shm 清理）必须严格，避免脏资源。
- 若未来 Manager 需要高频控制/监控，可考虑将 IPC 创建或敏感逻辑下沉为 C/C++ helper。

最小结构建议（示例）：
```
sims/mem/manager/
  manager.py            # 入口：解析配置、创建 channel、启动仿真器
  config.py             # 配置解析与校验
  channels.py           # channel 创建/销毁（shm/socket）
  proc.py               # 进程启动与退出码处理
```

### 目标场景（最小重构目标）

最小目标是**不再沿用** `basicmem` 监听/创建、`memstim` 连接的旧模型：
- 仿真器启动参数中**不再包含** `mem.socket`、`mem.shm` 等路径。  
- **所有通信资源创建与生命周期管理**交由 Manager 负责。  
- 仿真器只负责向 Manager 申请“端口”并获取连接信息。  

这意味着需要在核心库与 `basicmem`/`memstim` 中引入 **Manager 协商/查询接口**，从而由 Manager 统一创建 channel，并把连接信息下发给两端。

### 最小实现范围

1. **新增一个 Manager 可执行程序**（位置可由实现决定，建议放在 `sims/mem/manager` 或 `lib/simbricks/manager`）。
2. **新增拓扑配置文件**（例如 `sims/mem/topos/basicmem-memstim.yaml`），描述：
   - 两个仿真器：`basicmem` 与 `memstim`。
   - 一个端口连接关系：`memstim.mem` ↔ `basicmem.mem`。
   - channel 类型与参数由 Manager 统一创建与维护。
3. **Manager 负责：**
   - 解析拓扑配置并创建 channel 资源。
   - 生成端口对应的连接信息（socket/shm/IPC 句柄等）。
   - 启动 `basicmem` 与 `memstim` 进程，并提供“向 Manager 取端口信息”的访问方式。

### 实现步骤（建议）

1. **拓扑配置格式（最小版）**
   仅需满足 `basicmem` 与 `memstim`：
   ```yaml
   simulators:
     - name: basicmem
       exec: ./sims/mem/basicmem/basicmem
       args: ["1024", "0", "0", "1", "0", "10", "10"]
       ports:
         - name: mem
     - name: memstim
       exec: ./sims/mem/memstim/memstim
       args: ["0", "0", "4", "8", "1", "0", "10", "10"]
       ports:
         - name: mem
   links:
     - a: memstim.mem
       b: basicmem.mem
       channel:
         type: shm_ring
         options:
           socket_path: ./mem.socket
           shm_path: ./mem.shm
   ```
   > 说明：此处不再将 socket/shm 路径放入仿真器 args，而由 Manager 创建并下发。

2. **Manager 启动流程（最小版）**
   - 读取配置并做基本字段校验。
   - 创建 channel 并持有其资源句柄（socket/shm/IPC）。
   - 启动 `basicmem` 与 `memstim` 进程。
   - 提供端口查询/协商接口（例如本地 control socket 或 env + fd 传递）。
   - 等待两者退出并转发退出码。

3. **后续演进（非阻塞）**
   - 将端口协商与连接参数下发做成稳定 API（便于扩展更多仿真器）。
   - 扩展 channel 抽象，支持更多 IPC 类型。

### 编译与验证方式（必须写明）

**编译：**在仓库顶层执行 `make`（应默认构建 `basicmem` 与 `memstim`）。

**验证：**
1. 运行 Manager（示例命令）：
   - `./sims/mem/manager/manager ./sims/mem/topos/basicmem-memstim.yaml`
2. 观察进程日志：
   - `basicmem` 与 `memstim` 应显示成功向 Manager 获取端口信息并建立连接。
3. Manager 退出码应为 0（或与子进程一致），表示两者正常完成。

---

> 本文档为架构草案，后续可根据具体实现细节补充：如 IPC 实现、错误处理策略、管理接口等。
