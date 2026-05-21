---
module: stress-tests
type: module
status: active
stack: C, ATF, kyua, FreeBSD make
last_modified: 2026-05-15
programs: 6 ATF + 4 stressors
related: [[[ibs-test-suite]], [[run-sh]]]
tags: [freebsd-ci-actions, module, stress, cpu, memory, network, disk]
---

# STRESS Test Suite

> 4-program ATF test suite + 4 background stressor binaries for CPU, memory, network, and disk load. Runs as both a standalone suite and as background pressure during IBS/L3/UMCDF/PMC runs.

## Overview

Tests live in `tests/sys/amd/stress/`. The suite has two distinct operational modes:

1. **Standalone ATF mode** (`--suite STRESS --run-all`): validates that CPU/mem/net/disk operations produce correct results under sustained load. Each test case runs for 120 seconds.
2. **Background stressor mode** (`--stress` flag): the four stressor binaries run as background processes during another suite's test run, injecting CPU/memory/network/disk pressure to expose hardware counter errors that only manifest under load.

This enables validation of AMD PMU correctness under realistic system conditions — e.g., verifying that IBS sample data is valid when the memory subsystem is under concurrent read/write pressure.

## Main Files

- `tests/sys/amd/stress/stress_utils.h` — shared constants and helpers for all stress programs: `stress_ncpus()`, `stress_now_ms()`, `STRESS_DURATION_SEC=120`, `STRESS_XORSHIFT64(x)`, `STRESS_LCG_MUL`
- `tests/sys/amd/stress/stress_ibs.h` — minimal IBS Op MSR interface for the IBS-under-stress test; no libpmc; uses `/dev/cpuctl{N}` directly; provides `sibs_read_msr()`, `sibs_write_msr()`, `sibs_cpu_supports_ibs_op()`, `sibs_op_disable()` (workaround #420)
- `tests/sys/amd/stress/cpu_stress_test.c` — ATF: xorshift64 compute, sched_yield context-switch, FPU sin/cos validation
- `tests/sys/amd/stress/mem_stress_test.c` — ATF: 128 MiB cache thrash, 16 MiB bandwidth (XOR integrity), TLB pressure via mmap/munmap
- `tests/sys/amd/stress/net_stress_test.c` — ATF: AF_UNIX socketpairs, TCP loopback echo, rapid connect/close fd-leak check
- `tests/sys/amd/stress/disk_stress_test.c` — ATF: 64 MiB sequential R/W, 32 MiB random pwrite/pread checksum, fsync loop
- `tests/sys/amd/stress/ibs_op_stress_test.c` — ATF: IBS Op sampling concurrent with each class of system stress; 4 test cases (TC-ISTR-01–04); requires root + IBS-capable AMD CPU
- `tests/sys/amd/stress/cpu_stressor.c` — plain binary: compute + yield workers, SIGTERM-clean, no ATF dependency
- `tests/sys/amd/stress/mem_stressor.c` — plain binary: 128 MiB cache thrash + bandwidth workers
- `tests/sys/amd/stress/net_stressor.c` — plain binary: AF_UNIX socketpair producer/consumer; reads `NET_STRESS_THREADS` env (default 2 when background)
- `tests/sys/amd/stress/disk_stressor.c` — plain binary: seq/rand/fsync workers over 4 MiB `/tmp` file

## Architecture Decision

### [DECISION] Two-mode stressor architecture (ATF + plain binaries)
**Date:** 2026-05-14
**Context:** We need (a) standalone correctness validation (does the stress workload itself behave correctly?) and (b) background load generation (run stressors in parallel with IBS/PMC tests to detect race conditions).
**Decision:** Build both ATF test programs (for standalone `--suite STRESS` runs) and plain stressor binaries (for `--stress` background mode) from the same source directory. `stress_utils.h` is shared. The Makefile uses a `beforeinstall` hook to build and install stressor binaries alongside the ATF programs.
**Discarded alternatives:** Putting stressor binaries in a separate directory (adds maintenance overhead); using ATF programs as background stressors (ATF has a timeout/result model incompatible with "run indefinitely until SIGTERM").
**Impact:** run-sh (`start_background_stressors()`, `stop_background_stressors()`), ibs-test-suite (`ibs_nmi_stress_test`)

### [DECISION] NET_STRESS_THREADS=2 in background mode
**Date:** 2026-05-14
**Context:** Heavy network load combined with high IBS sampling rates can reproduce the NMI storm → network stack panic described in FreeBSD-Tests-026 (Ali's crash analysis, 2026-05-14).
**Decision:** `run.sh` exports `NET_STRESS_THREADS=2` (down from 4 standalone) when using `--stress` as background, and emits a WARNING when the combined suite is IBS or ALL.
**Impact:** run-sh, ibs-test-suite

### [DECISION] STRESS_DURATION_SEC=120 for all ATF cases
**Date:** 2026-05-14
**Context:** Need long enough to catch intermittent failures (TLB eviction, NMI storms, disk fsync errors) while keeping the full suite under 10 minutes.
**Decision:** 120s per case; ATF timeout=200 to give 80s of margin.
**Impact:** All four ATF test programs

## Kyuafile

```
syntax(2)
test_suite("FreeBSD")
atf_test_program{name="cpu_stress_test", }
atf_test_program{name="disk_stress_test", }
atf_test_program{name="ibs_op_stress_test", }
atf_test_program{name="mem_stress_test", }
atf_test_program{name="net_stress_test", }
```

## Test Categories (run.sh)

| Code    | Label          | Severity | Programs              |
|---------|----------------|----------|-----------------------|
| TC-CSTR | CPU Stress     | MEDIUM   | cpu_stress_test       |
| TC-MSTR | Memory Stress  | MEDIUM   | mem_stress_test       |
| TC-NSTR | Network Stress | MEDIUM   | net_stress_test       |
| TC-DSTR | Disk Stress    | MEDIUM   | disk_stress_test      |
| TC-ISTR | IBS-Op Stress  | MEDIUM   | ibs_op_stress_test    |

### [DECISION] IBS Op sampling verification under each stress class
**Date:** 2026-05-14
**Context:** The four plain stress tests validate each subsystem in isolation. We need to confirm that IBS Op hardware sampling still produces coherent results (MSR reads succeed, IbsOpEn stays set, no invalid RIPs) while each class of load is active. This detects hardware/driver bugs that only manifest under combined load.
**Decision:** Added `ibs_op_stress_test.c` (TC-ISTR-01–04). Each test case starts stress threads for one resource class and simultaneously runs `ibs_op_run_sampling()` (120 s at `SIBS_SAFE_PERIOD=0x1000`) on CPU 0. Sampling uses `stress_ibs.h` (no libpmc — direct cpuctl MSR access) to keep the test self-contained. At shutdown, the AMD Erratum #420 drain sequence is verified by confirming `MSR_IBS_OP_CTL` reads 0 after the 50×1µs drain loop. The test is skipped on CPUs without IBS Op support (CPUID 0x8000001B EAX[2]).
**Discarded alternatives:** Reusing the existing `ibs_nmi_stress_test.c` (that test targets NMI rate-limiting, not MSR coherence); using libpmc (heavier dependency, harder to skip gracefully on non-AMD systems).
**Impact:** Makefile (new ATF_TESTS_C entry), Kyuafile (new entry), run.sh (TC-ISTR category), stress-tests.md

### [DECISION] stress_ibs.h — self-contained minimal IBS Op interface
**Date:** 2026-05-14
**Context:** `ibs_op_stress_test.c` needs IBS Op MSR access. Importing the full IBS test suite's `ibs_utils.h` / `ibs_decode.h` would create a cross-directory dependency and require libpmc for a test that intentionally avoids it.
**Decision:** Created `tests/sys/amd/stress/stress_ibs.h` — a standalone header with only the symbols needed by the stress test: MSR addresses, bit masks, `SIBS_SAFE_PERIOD`, `sibs_read_msr()`, `sibs_write_msr()`, `sibs_cpu_supports_ibs_op()`, `sibs_op_disable()`. Uses `/dev/cpuctl{N}` ioctls directly.
**Impact:** stress dir only; no other module dependency.

### [DECISION] STRESS suite must run with kyua parallelism=1
**Date:** 2026-05-14
**Context:** All four ATF test programs (cpu/mem/net/disk) saturate the entire CPU set and main memory for 120 s each. Running them concurrently (kyua default: parallelism=hw.ncpu) causes resource starvation across tests: CPU load from cpu_stress_test prevents net_stress_test's echo server from keeping up with clients; memory pressure from mem_stress_test causes connection-rate tests to miss the 200 s timeout. Three of the twelve net tests broke in every parallel run.
**Decision:** `run.sh::run_all_tests()` forces `PARALLELISM=1` before the kyua invocation whenever `SUITE=STRESS`. The global `--parallelism` flag is intentionally overridden for this suite and documented in the header output.
**Discarded alternatives:** Raising ATF timeouts (masks the real problem; test results still non-deterministic); reducing STRESS_DURATION_SEC (defeats the purpose of long-duration validation).
**Impact:** `run.sh` (run_all_tests), test runtime changes from ~2 min to ~24 min total.

### [DECISION] Add stress_monitor_loop console output for STRESS suite
**Date:** 2026-05-14
**Context:** With parallelism=1 and 12 × 120 s tests the suite runs ~24 min with no visible progress besides per-test result lines. Operators need to see which test is running, elapsed/remaining time, and system resource levels.
**Decision:** Added `stress_monitor_loop()` shell function in `run.sh`. It runs as a background process while kyua executes, printing a two-line resource snapshot every 10 s to `/dev/tty` (bypasses the kyua stdout pipe). Shows: elapsed/estimated-remaining seconds, N/M done, last completed test, CPU load avg, free/total memory, /tmp disk space, lo0 cumulative rx/tx bytes. ETA is computed as `elapsed/done × (total−done)`.
**Impact:** `run.sh` (new function, run_all_tests setup/teardown, result-processing while loop).

## [BUG] make install fails on first build — stress dir missing
**Found:** 2026-05-14
**Symptom:** `./run.sh --suite STRESS --compile` succeeds at build but exits with `install: /usr/tests/sys/amd/stress: No such file or directory` (error code 71).
**Root cause:** `beforeinstall` in Makefile copied stressors before `bsd.test.mk` had a chance to create `TESTSDIR`; the directory didn't exist on the first install.
**Fix/Workaround:** Added `${INSTALL} -d … ${DESTDIR}${TESTSDIR}` as the first command in `beforeinstall` so the directory is created before any file copies.
**Status:** resolved

## [BUG] net_stress_connection_rate SIGSEGV — fd stored in in_port_t race
**Found:** 2026-05-14
**Symptom:** `net_stress_connection_rate` exits with signal 11 (SIGSEGV), core dump. kyua reports "broken: Empty test result or no new line". GDB unavailable so no stack trace.
**Root cause:** `struct connrate_arg::port` is `in_port_t` (uint16_t). The main thread stored `srv_fd` into it (`arg.port = (in_port_t)srv_fd`) to smuggle the fd to the server thread, then immediately overwrote `arg.port = ntohs(real_port)` for the client thread. Race: if `connrate_server` read `arg->port` after the overwrite it got the port number (e.g. 54321) as the socket fd. `FD_SET(54321, &rfds)` in a 1024-bit fd_set overflows the array → SIGSEGV. Same race existed in `tcp_echo_server` via `struct tcp_echo_ctx::port`.
**Fix/Workaround:** Added `int srv_fd` field to both structs. Server threads read from `arg->srv_fd`; `port` now holds only the real port from the start. No more overwrite race.
**Status:** resolved (2026-05-14)

## [BUG] net_stress_unix_socketpair crash — SIGPIPE from shutdown during send
**Found:** 2026-05-14
**Symptom:** `net_stress_unix_socketpair` broken: "Empty test result or no new line". Ran ~120 s then crashed.
**Root cause:** After `stop=true` the main thread calls `shutdown(pairs[i].fd[0], SHUT_RDWR)` while the producer thread may still be inside `send(fd[0], ...)`. `send()` on a shut-down socket generates SIGPIPE; default action terminates the process, so ATF sees no output.
**Fix/Workaround:** Added `signal(SIGPIPE, SIG_IGN)` at the top of `net_stress_unix_socketpair` body. Added the same to `net_stress_loopback_tcp` (echo server's `send()` can also race with client close).
**Status:** resolved (2026-05-14)

## [BUG] net_stress_loopback_tcp timeout under parallel execution
**Found:** 2026-05-14
**Symptom:** Test body timed out at 200 s when run with kyua parallelism=hw.ncpu.
**Root cause:** `cpu_stress_test` saturates all CPU cores in parallel. The echo server's tight `send()/recv()` loop stalls under scheduler starvation. After `stop=true`, client threads finish their in-flight 64 KiB request; the server needs to echo all 64 KiB back before `close()`. Under full CPU saturation this pushes teardown past the 200 s ATF timeout.
**Fix/Workaround:** Sequential execution (parallelism=1) eliminates contention; test completes comfortably within timeout.
**Status:** resolved (2026-05-14) — prerequisite: DECISION above (parallelism=1)

## [DECISION] Add ibs_fetch_stress_test (TC-FSTR) to STRESS suite
**Date:** 2026-05-15
**Context:** STRESS suite only had ibs_op_stress_test covering IBS Op. IBS Fetch sampling was untested under load.
**Decision:** Created `ibs_fetch_stress_test.c` with 4 cases (TC-FSTR-01–04): IBS Fetch + cpu/mem/disk/net stressor. Extended `stress_ibs.h` with Fetch MSR constants (`SIBS_MSR_FETCH_CTL`, `SIBS_FETCH_EN`, `SIBS_FETCH_VAL`, `SIBS_FETCH_SAFE_PERIOD=0x1000`), `sibs_cpu_supports_ibs_fetch()` (CPUID 0x8000001B EAX[0]), and `sibs_fetch_disable()` (10-µs drain, no erratum-#420 equivalent for Fetch). Added `TC-FSTR:IBS-Fetch Stress:MEDIUM` to run.sh category map.
**Discarded alternatives:** Sharing stressor threads between Op and Fetch via a shared header — rejected to keep each test file self-contained.
**Impact:** Makefile, stress_ibs.h, run.sh (case + help text), new file ibs_fetch_stress_test.c

## [BUG] ibs_op_stress_test not installed — stale Kyuafile
**Found:** 2026-05-15
**Symptom:** `--suite STRESS --run-all` ran 12 tests instead of 16; no ibs_op_stress_test results anywhere in output. Installed Kyuafile was missing `ibs_op_stress_test`.
**Root cause:** Binary was added to `ATF_TESTS_C` in the Makefile but the suite was never recompiled/reinstalled after that change. The installed Kyuafile was from a previous build.
**Fix/Workaround:** `./run.sh --suite STRESS --compile --force` rebuilt and reinstalled all binaries including the new ones.
**Status:** resolved (2026-05-15)

## [BUG] mem_stress_cache_thrash — buffer always zero on even CPU counts
**Found:** 2026-05-15
**Symptom:** `mem_stress_cache_thrash` failed: "buffer is entirely zero after thrash — writes may have been silently dropped". Passed on first run (lighter system), failed reliably on second run (after 30 min of IBS load).
**Root cause:** `thrash_thread` seeded `acc = 0`; with a calloc'd all-zero buffer, every `buf[idx] ^= 0` is a no-op and acc stays 0 forever. The dead-store guard `if (acc == 0) buf[0] ^= 1` fires for all 96 threads; on a 96-CPU (even) machine, 96 XOR-1 operations on buf[0] return it to 0. First-run pass was a race-condition fluke.
**Fix/Workaround:** Seed `acc = idx ^ 0xDEADBEEFCAFE0001ULL` (thread-unique non-zero); removed the dead-store guard (no longer needed).
**Status:** resolved (2026-05-15)

## [BUG] net_stress_loopback_tcp — recv blocks indefinitely after stop under load
**Found:** 2026-05-15
**Symptom:** `net_stress_loopback_tcp` broken: "Test case body timed out" at 200 s. Occurred after 30 min of preceding IBS stress tests.
**Root cause:** Client recv loop has no timeout. On a heavily loaded system the echo server falls behind; after `stop=true` the client is stuck in recv until ATF kills the test.
**Fix/Workaround:** Added `SO_RCVTIMEO = 5s` on each client socket after connect. Client exits the recv loop within 5 s of stop, join completes well within the 200 s ATF limit.
**Status:** resolved (2026-05-15)

## Learning Log

2026-05-14 | beforeinstall runs before bsd.test.mk creates TESTSDIR; always mkdir -p the dest dir at the top of beforeinstall when copying non-ATF binaries | stress-tests
2026-05-14 | Never smuggle a socket fd through an in_port_t (uint16_t) field — truncation + overwrite race causes FD_SET overflow → SIGSEGV. Use a dedicated int field. | stress-tests
2026-05-14 | Always SIG_IGN SIGPIPE in ATF test bodies that call send() on sockets that may be shut down during cleanup; default SIGPIPE kills the process leaving ATF with no result. | stress-tests
2026-05-14 | STRESS suite must run with parallelism=1; parallel execution causes resource starvation that makes long-duration tests time out. | stress-tests
2026-05-14 | IBS Op MSR access in stress tests uses stress_ibs.h (no libpmc); always include <sys/types.h> before <sys/cpuctl.h> to pull in FreeBSD type definitions. | stress-tests
2026-05-14 | ibs_op_stress_test verifies IBS MSR coherence under each load class: MSR reads must never error, IbsOpEn must stay set, IbsRipInvalid must never be set. | stress-tests
2026-05-15 | IBS Fetch stop sequence (sibs_fetch_disable) uses 10 µs drain vs Op's 50 µs — Fetch has no AMD erratum #420 equivalent. | stress-tests
2026-05-15 | Always seed thrash-loop accumulators non-zero; acc=0 + calloc buffer means every XOR is a no-op and the integrity check trivially fails on even CPU counts. | stress-tests
2026-05-15 | Add SO_RCVTIMEO to client sockets in TCP stress tests; without it, a blocked recv outlasts the ATF timeout (test_body + teardown > 200 s) under heavy prior load. | stress-tests
2026-05-15 | Full STRESS suite (20 tests) passes clean on EPYC 9654 (96-core Zen 4): all 4 IBS Fetch + all 4 IBS Op cases confirmed working under cpu/mem/disk/net load. | stress-tests

## Cross-references

- `ibs_nmi_stress_test.c` — NMI stability test in the IBS suite that uses inline stress workers; see [[ibs-test-suite]]
- FreeBSD-Tests-026 — Jira story tracking STRESS suite + NMI rate-limit enforcement + `--stress` flag
- `run.sh` `start_background_stressors()` / `stop_background_stressors()` — orchestration; see [[run-sh]]
