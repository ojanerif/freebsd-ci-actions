---
module: ci-vm-infra
type: infra
status: active
stack: sh, FreeBSD make, kyua
last_modified: 2026-04-30
related: [[[ibs-test-suite]], [[ci-actions-workflows]]]
tags: [freebsd-ci-actions, infra, vm, freebsd-ci]
---

# CI VM Infrastructure
> FreeBSD Release Engineering VM image build config and RC init script for unattended ATF test runs inside CI VMs.

## Overview

`ci/tools/` contains two files consumed by FreeBSD's `release/tools/vm-image-builder`
(not part of this repo). The VM builder reads `ci.conf` to install packages
and configure the image; on first boot, the `freebsdci` RC script detects
its CI context and runs the ATF test suite unattended, collects results, then
shuts down.

This is separate from the GitHub Actions pipeline — it's for FreeBSD Release
Engineering's own CI infrastructure, not for the GitHub Actions runner.

## Main Files

- `ci/tools/ci.conf` — VM image builder config: packages, loader.conf, rc.conf, sysctl.conf, kyua.conf settings
- `ci/tools/freebsdci` — RC init script: metadata extraction from tar device, `kyua test` (smoke or full mode), JUnit XML + text result collection, shutdown/reboot

## Dependencies

- modules: [[ibs-test-suite]]
- external: FreeBSD `release/tools/vm-image-builder` (not in this repo)

## Decisions

## [DECISION] Separate CI VM path from GitHub Actions path
**Date:** 2026-04-30
**Author:** Osvaldo J. Filho
**Actor type:** human
**Source:** ai-prompt
**Session:** sess_2026-04-30_0000
**Context:** FreeBSD Release Engineering has its own VM-based CI that pre-dates
GitHub Actions. Both need to run the IBS test suite.
**Decision:** Keep `ci/tools/` for the RE VM path. GitHub Actions path lives
in `actions/` and `.github/workflows/`. No shared code between them — different
execution environments (VM first-boot RC vs runner job steps).
**Impact:** Two independent integration paths; both consume the same `tests/` source.

## Known Bugs

## TODOs

## [TODO] Verify ci.conf is current with latest package versions
**Priority:** low
**Context:** `linux_base-rl9` switch (see linuxulator module) may need to be
reflected in ci.conf if VMs also use Linuxulator. Currently unclear.
**Status:** pending

## Snippets

## Learning Log

2026-04-30 | First read. Small, stable module. Main risk is drift between ci.conf packages and runner-setup packages if rl9 change isn't mirrored. | ci-vm-infra
