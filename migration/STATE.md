# Migration state

Last updated: 2026-11-13 (America/Sao_Paulo).

## Repository and branch

| Item | Value |
| --- | --- |
| Repository | `https://github.com/pietroth/LightnerX.git` |
| Verified remote default | `main` (`origin/main`) |
| Baseline commit | `541ae1d39225ec9b747b9f888f96242a710e9073` — `Update README.md` |
| Migration branch | `refactor/cpp17-header-migration` |
| Isolated worktree | `/tmp/LightnerX-cpp17-header-migration` |
| Original checkout | `/home/pietroth/Cpp/LightnerX`, on `main` |
| Pinned GNS | `d534c19aa760df3fb75fd20db13ba1932b8a5463` (`v1.6.1`) |
| Phase 1 change scope | Only `migration/PLAN.md`, `migration/STATE.md`, `migration/API_CHANGES.md` |
| Publication | Local branch; no push or pull request |

The branch was created with `git worktree add -b` from the freshly fetched
`origin/main`. No existing branch was reset or overwritten. Its initial upstream
is `origin/main`; do not use that upstream as an instruction to push to main.
The planning commit on this branch records the Phase 1 delivery; the immutable
source baseline remains the commit above.

## Current status

**Phase 1 complete: repository audit and architecture plan.** The library still
uses its original C++26 modules. The C++17 migration is planned, not implemented.
No production source, CMake, tests, documentation outside `migration/`, or
third-party source has been changed by Phase 1.

The plan covers 14 library modules, five C++ test inputs, four Python harnesses,
all five CMake files, six Markdown documents/examples and relevant configuration.
There are no tracked first-party headers yet. The actual queue API is
`queueMessage(OutboundMessage)`, with `Target` stored in that object; single,
group and broadcast with optional exclusions must all remain supported.

## Finished work

- Verified default branch through GitHub metadata and remote HEAD advertisement;
  fetched origin references and the `v0.1.0-beta` tag.
- Inspected the full commit history, current branch/worktree state, tracked file
  inventory, GNS pin/build integration, tests and documentation.
- Created the requested isolated branch and initialized a clean pinned GNS copy
  there without using or altering the original submodule's working files.
- Read every first-party module and test source, generated-C++ test preamble,
  complete documented example and the ignored local prototype/editor settings.
- Recorded all actual module edges, including the unused Event-to-Client edge,
  and a 14-step topological migration order.
- Identified public span/expected replacement requirements, newer algorithms,
  abbreviated templates, printing and documentation-only traits/helpers.
- Recorded packed wire layouts, parsing permissiveness, raw-pointer ownership,
  queue routing/error aggregation, shared runtime, callback behavior and exported
  but undocumented runtime symbols.
- Designed public `include/lightnerx/{communication,network,compat}` headers and
  eight ordinary source files in the existing communication/network directories.
- Built the unchanged baseline and ran all nine regression tests successfully.
- Created the plan, state and anticipated API-change ledger.

## Validation

| Check | Result |
| --- | --- |
| Fresh configure | Passed; original CMake 4.2 experimental module warning is expected |
| Fresh build | Passed, all 133 steps; `libLightnerX.a` produced |
| CTest | **9/9 passed**, no skips; 65.52 seconds |
| Real networking | GNS socket tests and separate-process local UDP integration passed |
| Consumer | Independent parent configure/build/run passed |
| Export probes | Runtime detail functions/flag accessible; Event alone does not expose `communication::Client` |
| Documentation checks | All 40 baseline entries inventoried; order validated against all 24 direct first-party import edges; Markdown fences, relative links and whitespace checked |
| Preservation checks | Original main commit/status and submodule diff/local-file hashes match the entry state; isolated GNS checkout is clean |
| C++17 migration | Not implemented or validated yet |
| Sanitizers / additional platforms | Not rerun in Phase 1; historical limitations recorded in plan |

Toolchain: GCC/libstdc++ 15.2.0, CMake 4.2.3, Ninja 1.13.2, Python 3.14.4;
OpenSSL 3.5.5, Protobuf 3.21.12; Linux x86_64. Clang 21.1.8 and a clang++-23
binary are available but were not used for this baseline.

Reproduction commands and log locations are in `PLAN.md`, section 10. Build and
test artifacts are ignored, local evidence under `build/phase1-baseline/` in
the isolated worktree; they are not part of the documentation commit.

## Original work preserved

At entry, the original main checkout had only a dirty submodule in Git status:
`external/GameNetworkingSockets/examples/trivial_signaling_client.h`, four blank
lines removed. Its Git diff SHA-256 was
`55e5f5bf4b154a074e21ee282ef818398b36306222a2efadee8a473a320e7ab7`.
The ignored local prototype and editor settings were left in place:

| Local-only file | SHA-256 at audit |
| --- | --- |
| `main.cpp` | `855a0aac9dabf9982dacaae7c40d5dbd0d9f11b51741632c20ee6ce8e61817e8` |
| `.zed/settings.json` | `9cdcd0145d13ebde3f10d5f7b4d54e58579423d69000a315921d53daf08bda58` |
| `.zed/tasks.json` | `6cdaafd9ebb78e0034bdf712ad4369092fcb94062c7706c060b79e998589738b` |
| `.zed/debug.json` | `d3679481f1369af0563842d16913f7cf7dda870b1f2b305753230a7487eabdd0` |

These are preservation evidence, not files to migrate or stage. The clean GNS
copy in the isolated worktree is distinct from that dirty checkout. The existing
`fix/audit-lx` branch and original build artifacts were also left intact.

## Pending work and next phase

Next is **Phase 2: C++17 compatibility foundation**, followed by the exact
module order in `PLAN.md`, section 8. This delivery does not start Phase 2.

1. Pin and validate span/expected backports and their complete required public
   surface; record the immutable revisions/licenses and any differences in
   `API_CHANGES.md`. Candidate providers are documented, not installed.
2. Establish compatibility headers, baseline byte/layout checks and independent
   C++17 header/consumer probes. Avoid including replacement definitions and
   importing old module definitions into the same program.
3. Convert the 14 modules in the plan's dependency order, preserving all runtime
   and protocol contracts; keep shared state in single source definitions.
4. Cut CMake over atomically, adapt all existing tests/fault harnesses and
   examples/editor configuration, and remove obsolete first-party module units.
5. Run the migration acceptance matrix: strict C++17 GCC/Clang, CMake minimum,
   multi-TU and newer-standard consumers, protocol/routing/lifecycle tests and
   supported sanitizers. Keep GNS sources and pin unchanged.

There is no blocker to the next implementation phase. Backport selection/pinning
is an explicit first implementation task; full standard-expected compatibility
must not be assumed from a backport's name. API spelling/ABI changes are
anticipated in the ledger, with no implemented changes recorded yet.
