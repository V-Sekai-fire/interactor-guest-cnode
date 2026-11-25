<!--
SPDX-FileCopyrightText: 2026 K. S. Ernest (iFire) Lee
SPDX-License-Identifier: Apache-2.0 OR MIT
-->
# Message shape for Erlang C nodes

One shape for the guest, voice, Bonsai and mujoco nodes. Nothing is retired until each node runs on it.

## Request and reply

A request is `{Command :: atom, Ref :: reference, Arg :: term}` sent to the node's registered name. The reply goes to the sender's pid.

| Reply | Meaning |
|---|---|
| `{Ref, {ok, Term}}` | success |
| `{Ref, {error, Reason :: binary}}` | failure; errors are binaries |
| `{Ref, {event, Term}}` | zero or more, before the final reply, for long jobs |

Bytes are binaries. Arguments and results may be integers, floats, booleans, binaries, lists and tuples.

## Common commands

| Command | Arg | Reply |
|---|---|---|
| `ping` | any | `{ok, pong}` |
| `info` | any | `{ok, #{name, version, commands}}` plus node keys such as `status` |
| `cancel` | none; sent as `{cancel, Ref2, Ref}` | `{ok, cancelled}` or `{error, <<"not cancellable">>}` |
| `batch` | `{Command, [ItemArg]}` | `{ok, [Term]}`, one payload per item; one refused item refuses the whole batch; a batch inside a batch is refused; `info` has `batch => <<"list">>` |
| `call` | `{Symbol, Args}`; Args are integers, `{in, Binary}`, `{out, Size}` | `{ok, {Ret :: integer, [Binary]}}`: the guest function's return value, then the `out` buffers |

`call` runs a plain C entry point of a guest ELF and needs no Godot. `vmcall`, `{<<"fn">>, [Arg]}`, runs a godot-sandbox Variant-ABI guest and needs the Godot host. `info` lists the commands a node has.

## Node rules

- Long names on 127.0.0.1, a shared cookie, loopback only.
- A node publishes only when ready. During a later `load` it answers `{error, <<"loading">>}`.
- `guest-cnode` serves requests on the thread that runs the Godot main loop; other nodes may use a thread per request with a mutex around `ei_send`.
