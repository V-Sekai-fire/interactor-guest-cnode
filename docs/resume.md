<!--
SPDX-FileCopyrightText: 2026 K. S. Ernest (iFire) Lee
SPDX-License-Identifier: Apache-2.0 OR MIT
-->
# Resume note

Mission step: the shared guest host as a NixOS flake and as win64 (RFD66606-2360, 2436). The rollout of a cnode flake to every `*-guest` repo waits until the host passes the gate's guest.

## State

| Piece | State |
|---|---|
| `flake.nix`, `nix/` | `packages.x86_64-linux.{libgodot,sandbox-addon,guest-cnode}` build; inputs pinned in `flake.lock` (entities-godot 0bac827, godot-sandbox 9fadcba) |
| `src/guest_cnode.c` | ei C node: `ping`, `info`, `load`, `vmcall`, `cancel`. `batch` and `call` are in the document, not in the code |
| `project/` | Godot console screen: GPU adapter, guest, call log, design tokens. The node reaches it as the engine singleton `Console` |
| `service/asyncapi.yaml` | AsyncAPI 3.1, passes `tools/asyncapi_check.exs` of interactor-hair-cards-viewer; carries `x-state-machine` |
| `docs/message-shape.md` | the shape agreed with the voice, Bonsai and mujoco nodes |
| win64 | not started. Toolchain agreed: llvm-mingw. `ei` for mingw is being built by the mujoco session from the OTP 29.1.2 source |
| Steam Deck proof | not passed (see below) |
| Voice pilot | not started: needs the host to run a guest first |

Not tested: `load` and `vmcall` against a real guest ELF; the console screen; `info` and `cancel` replies.

## Build

`nix build .#guest-cnode .#libgodot .#sandbox-addon` with flakes enabled. `libgodot` is a full Godot compile (double precision, about 25 minutes on 8 cores, 1.2 GB closure). It needs `disable_path_overrides=no` so `--path` works. Run long builds as `systemd-run --user --no-block --property=Type=oneshot` units, not in a terminal.

## Run

`GUEST_CNODE_COOKIE=c LIBGODOT_PATH=<libgodot.so> guest-cnode --path <project dir> --rendering-driver vulkan --display-driver x11 --windowed`. Not `--headless`: the guests need a RenderingDevice, so Godot gets a real GPU window (Xvfb on a server). `epmd` must be running.

## Steam Deck proof (the GPU run)

Device: Steam Deck over `ssh steamdeck` (AMD GPU, `/dev/dri`, gamescope X servers `:0` and `:1`, user namespaces and `bwrap` work, no Nix). Do not run GPU or display tests on the WSL desk. The Nix closure goes to `~/guest/nix` on the Deck and runs under `bwrap` with that directory at `/nix`; Mesa's radeon Vulkan driver comes from the closure, not the Deck's own drivers. Copy of about 1.4 GB plus Erlang for `epmd`. Pass: the console window shows the adapter name and `rd present`, and an Elixir node gets `{ok, pong}` and one `vmcall` result; a planted wrong function name must give an `{error, _}`.

## The `*-guest` repos (41 on 2026-11-25; the first count was 38)

`docs/guests.tsv` lists every `*-guest` repo in V-Sekai-fire with its kind: `elf-cmake` (CMakeLists.txt or build.sh at the root), `sgd-only` (`.sgd` plus `project.godot`, no ELF build; a Godot host runs these directly), `other` (a build shape not yet read: look in `guest/`, `src/`, `tools/`). None has a `flake.nix`. `interactor-mujoco-sandbox-demo-guest` has its own node on `dev/mujoco-raw-cnode`; read it, do not edit it. Done: none. Pilot: `interactor-voice-guest`. Per repo: read AGENTS.md, add `flake.nix` taking this repo as an input and `riscv64-sysroot` and `contract-guest-runtime` as `flake = false` inputs, commit `flake.lock`, run the pre-commit hooks, open a PR over REST.

## Machine-specific

Scratch in `/tmp/claude-1000/g` on the WSL desk: `closure.txt`, `push.sh`, `rundeck.sh` (bwrap launcher for the Deck, not yet in git), and the copy in `~/guest` on the Deck. All rebuildable from `flake.lock`. Tokens come from `bao-mint-github`; none is in git.
