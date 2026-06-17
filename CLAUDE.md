# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Noxim is a cycle-accurate SystemC-based Network-on-Chip (NoC) simulator. This fork (branch `feat/limited_noxim`) is a deliberately reduced version of upstream Noxim: wireless/Hub features, power consumption modeling, and non-MESH topologies have been removed. The active line of work is around **error-correcting encoding models** (RAW, REPEAT, HAMMING, ILH — interleaved Hamming) applied to flits in a 2D mesh. See `src/encodingModels/`.

## Build, run, test

Builds happen from the `bin/` directory, not the repo root.

```bash
# Build the simulator (output: bin/noxim)
cd bin && make

# Build & run unit tests (gtest; output: bin/test)
cd bin && make test && ./test

# Regenerate header dependencies after adding/renaming files
cd bin && make depend

# Clean
cd bin && make clean

# Run a simulation against a config
./bin/noxim -config config_examples/default_config.yaml
```

`bin/Makefile` hard-codes `SYSTEMC := /lib/systemc-2.3.1` and `YAML := /usr/include/yaml-cpp` — edit these if your install paths differ. Tests are any `*_test.cpp` under `src/`; they link against the same core objects as the simulator (with `Main.o` filtered out) plus `-lgtest -lgtest_main -pthread`.

To enable debug logging via the `LOG` macro, uncomment `DEBUG := -g -DDEBUG` in `bin/Makefile` and rebuild from clean.

## Architecture

### Simulation entry & lifetime
`src/Main.cpp` is the SystemC `sc_main`. It calls `configure()` to load YAML + parse CLI overrides into `GlobalParams` (static class — every module reads its config from there), instantiates a single `NoC` module, drives reset, then runs for `simulation_time` cycles. On completion (or on `SIGQUIT`) `GlobalStats` walks the NoC and prints aggregate stats.

### Module hierarchy (SystemC SC_MODULEs)
- **`NoC`** (`src/NoC.{h,cpp}`) — top-level. Currently only `TOPOLOGY_MESH` is supported; `buildMesh()` allocates the tile grid and wires N/S/E/W `req`/`ack`/`flit`/`free_slots`/`buffer_full_status` signals between neighbors. Holds `GlobalRoutingTable` and `GlobalTrafficTable`.
- **`Tile`** (`src/Tile.h`) — a Router + a ProcessingElement, plus per-tile signals.
- **`Router`** (`src/Router.{h,cpp}`) — input buffers (one `Buffer` per direction per VC), `ReservationTable` for output-port arbitration, plugs in a `RoutingAlgorithm` and `SelectionStrategy` by name from `GlobalParams`.
- **`ProcessingElement`** (`src/ProcessingElement.{h,cpp}`) — injects/sinks packets according to the configured traffic distribution and packet injection rate; this is where the encoding model wraps outgoing packets and unwraps incoming flits.

### Plugin-style extension points
Three subdirectories under `src/` use the same self-registration pattern: a base class (`RoutingAlgorithm`, `SelectionStrategy`, `EncodingModel`), a static map (`RoutingAlgorithms::routingAlgorithmsMap` and friends), and a `*Register` struct that each concrete implementation declares as a static member to insert itself into the map at startup. The YAML config selects an implementation by string name (e.g. `routing_algorithm: XY`), which is looked up via `RoutingAlgorithms::get(name)`.

- `src/routingAlgorithms/` — `XY`, `WEST_FIRST`, `NORTH_LAST`, `NEGATIVE_FIRST`, `ODD_EVEN`, `DYAD`, `TABLE_BASED`
- `src/selectionStrategies/` — `RANDOM`, `BUFFER_LEVEL`, `NOP`
- `src/encodingModels/` — `RAW`, `REPEAT`, `HAMMING`, `ILH` (interleaved Hamming). The base class in `EncodingModel.h` provides shared helpers for hop simulation, payload prediction, and decode success/failure counters.

To add a new plugin: create `.h`/`.cpp` in the appropriate subdirectory, declare a static `*Register` member, define it in the `.cpp` with the string name and a `getInstance()` pointer, then `make depend && make`. No central registry edit is needed because the `bin/Makefile` globs all `src/*/*.cpp`.

### Configuration
`GlobalParams` (in `src/GlobalParams.{h,cpp}`) holds every tunable as a static member. `ConfigurationManager::configure()` first loads `config_examples/default_config.yaml` (or whatever `-config` points at) into `GlobalParams`, then applies CLI overrides via `showHelp`/argument parsing in `ConfigurationManager.cpp`. When adding a new parameter, both the YAML loader and the CLI parser usually need an update. Configs under `config_examples/` cover different mesh sizes (e.g. `256_16h.yaml` = 256-node 16×16) and channel counts.

## Repo-specific notes

- Indent is **2 spaces** throughout (`bac28a9 clean: unify indent to 2 spaces`).
- `make depend` regenerates `bin/Makefile.deps` from `makedepend` output — run it after any header include changes; otherwise stale deps cause silent rebuild misses.
- `exec/`, `other/`, and `doc/` are not part of the build. `other/` contains auxiliary CLI tools (e.g. `noxim_explorer.cpp` for design-space exploration) with their own `Makefile`.
- `.claudeignore` excludes `doc/`, `exec/`, `other/`, `.vscode/`, and generated artifacts (`repomix-output.xml`, `src/tags`) from Claude's view.
- This fork has removed wireless, Hub, multi-channel-radio, and power-consumption code paths from upstream Noxim. Don't reintroduce these without explicit instruction — references to `Hub`, `Channel`, or power tables in old upstream code/docs no longer apply.

## Development Guidelines (開発ガイドライン)

このリポジトリで新しいモジュールの追加や既存コードの改修を行う際は、以下の原則に必ず従ってください。

1. **既存設計パターンの徹底的模倣 (Imitate Existing Patterns)**
   - 新しい機能やモジュールを設計する際は、既存の類似モジュール（例: `Router`, `ProcessingElement`）の構造やプロセス実装を事前に確認してください。
   - SystemCのモジュール作成、プロセスのバインディング（`SC_METHOD`/`SC_THREAD`）、リセット処理の記述方法などは、既存の慣例（`Router`での`perCycleUpdate`など）に必ず倣って実装します。
   
2. **SystemCのサイクル駆動 (Cycle-Accurate Modeling)**
   - サイクル正確なモデリングを行う際は、スレッド内での `wait()` による仮想遅延ではなく、クロックエッジ感度の `SC_METHOD` を使用した自律駆動方式（アプローチB）を第一に選択してください。
   - サイクル毎に解決すべき遅延や状態遷移は、内部のキューやリストを用いて管理し、エッジが来るたびに1サイクルずつ処理を前進させます。

## Active Feature Branch: Cluster-Level Dynamic Encoding & Degradation Error Tracking

We have introduced a closed-loop reliability and degradation tracking system on this branch:

### 1. Degradation Monitor (`src/DegradationMonitor.{h,cpp}`)
- Manages dynamic temperature (transient thermal stress) and permanent wear (irreversible degradation) per router.
- Inside `Router::rxProcess`, the router updates its state cycle-by-cycle (`updateState(bool is_active)`) and applies dynamic penalties:
  - **Delay Penalty**: Additional latency cycles computed via `getCurrentDelay()`.
  - **Virtual Bit Error Rate (BER)**: Accumulates bit flips computed via `getCurrentBER()`.
  - **Packet Loss Rate**: Checked via `getCurrentLossRate()`. If triggered, the flit is dropped.

### 2. Dynamic Decoupled Cluster Encoding (`Router.cpp`)
- Decoupled from static `GlobalParams::cluster_encoding_type`.
- When crossing external cluster boundaries, the router calls `decideClusterEncodingType(..., src_id)` to dynamically select:
  - **Trust Score <= 0** (neutral, max recovery limit, or dangerous): `CLUSTER_ENC_SECDED` (Extended Hamming code / SEC-DED, dynamically calculated using $2^p \ge \text{effective\_bits} + p + 1$).
  - **Trust Score > 0** (proven safe): `CLUSTER_ENC_PARITY` (Single parity bit, fixed at 1 bit redundancy).
- If redundancy bits exceed the standard flit capacity (`GlobalParams::flit_size`), the boundary router dynamically expands the packet sequence length and injects an extra flit.

### 3. Virtual Error Accumulation (`Router.cpp`)
- Router simulates bit error injection on every received flit using its current BER.
- Accumulates the error count in the `Flit::virtual_errors[my_cluster_id]` map. No actual data payload is modified.

### 4. LIFO Virtual Decoding & ACK/NACK PE Feedback Loop (`ProcessingElement.cpp`)
- **PE Receiver (rxProcess)**:
  - Reassembles incoming flits in `flit_buffer`.
  - Upon receiving `FLIT_TYPE_TAIL`, performs virtual decoding by scanning the traversed path in reverse (**LIFO order**) from the last cluster to the first.
  - Applies ECC rules for each cluster:
    - **`PARITY`**: 0 errors -> success (+1.0); $\ge 1$ errors -> fatal (-10.0).
    - **`SECDED`**: 0 errors -> success (+1.0); 1 error -> corrected (-2.0); $\ge 2$ errors -> fatal (-10.0).
  - If a fatal error occurs, the loop terminates immediately (inner clusters are not evaluated) and registers `eval_fatal` to the cluster, sending a `PACKET_NACK` back to the source PE.
  - Successful packets trigger a `PACKET_ACK` carrying all gathered `cluster_evaluations`.
- **PE Sender (txProcess)**:
  - Receives `Ack` signal and records feedback in `cluster_evaluations` (acting as the cluster Trust Score).
  - If `is_nack` is true, immediately triggers retransmission of the packet. Otherwise, erases it from the outstanding list.
  - **Periodic Evaluation Recovery**: Every `recovery_interval` cycles, PEs scan their `cluster_evaluations` and increment negative values by `+1.0` (clamped to `0.0`), simulating recovery of stressed paths over time.

### 5. Visualization Grid Board (`-evalcluster`)
- CLI parameter `-evalcluster` or configuration YAML param `eval_cluster: true` outputs a `mesh_dim_y` × `mesh_dim_x` grid of the PE evaluation values for each cluster at simulation end.
- Values less than `-99` are displayed as `BAD` to preserve text column alignment.

### 6. Deterministic Cluster Routing Cache & Manager (`src/ClusterRoutingManager.h`)
- **Route Cache Table**: Each PE maintains a cache of the optimal cluster sequence (path) to all destination clusters, matching the grid cluster count `N^2 / 4`.
- **DFS Constraint-Satisfied Path Finder**: Recalculates paths up to `3N/2 - 3` length using a Branch-and-Bound DFS. It enforces three routing constraints to ensure deadlock freedom and loop freedom:
  1. *Odd-Even Turn Model*: Prohibits East-to-North/South turns in even columns, and North/South-to-West turns in odd columns.
  2. *Re-entry Prohibited*: No cycles (re-entering a traversed cluster is forbidden).
  3. *Convex Detour to East Restriction*: Prohibits West-bound movements after any East-bound movement has been taken.
- **Dynamic Cost Formula**: Path cost is computed hop-by-hop as:
  $$\text{cost}(c) = 1.0 + (\text{GlobalParams::eval\_success} - \text{evaluation}(c))$$
  This ensures costs are strictly positive ($\ge 1.0$) to prevent routing loop preferences, while penalizing degraded routes.
- **WEST_LAST Fallback Route**: Used when routes are not yet initialized (initial state) or if no path satisfies the constraints. It routes YX for West-bound destinations (North/South first, then West) and XY for East-bound destinations.
- **Periodic Recalculation**: Executed within `ProcessingElement::txProcess` at every `GlobalParams::recovery_interval` cycles.

## Routing Cautions and Developer Guidelines for Future Routing Modifications

1. **Clean Rebuild Requirement (ABI/Memory Alignment)**: Adding, removing, or changing member variables of SystemC modules (`Router`, `ProcessingElement`, etc.) changes their class memory layout. Always run `wsl make clean && wsl make` after header changes since automatic dependency tracking is not fully robust.
2. **Flit Routing Metadata Format**: In CLUSTER routing, `Flit.route_metadata.custom_data` must be formatted as:
   - `custom_data[0]`: `current_idx` (current index along the path, initialized to `0` and incremented at cluster boundary crossings).
   - `custom_data[1 + i]`: The cluster ID of the $i$-th step in the route.
   Ensure any custom routing code checks that `custom_data.size() >= 2` before dereferencing it, otherwise fallback to local routing.
3. **Step Cost Positivity**: When modifying cost parameters, ensure the computed step cost for all paths remains strictly positive. Zero or negative cost paths can lead to routing algorithms preferring longer loops, causing simulation stalls or infinite loops.

