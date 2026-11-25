# SPDX-FileCopyrightText: 2026 K. S. Ernest (iFire) Lee
# SPDX-License-Identifier: Apache-2.0 OR MIT
class_name Tokens
# Design tokens for the console: one surface ramp, one accent, three states.
const SURFACE_0 := Color("0e1116")
const SURFACE_1 := Color("161b22")
const SURFACE_2 := Color("1f262e")
const LINE := Color("30363d")
const TEXT := Color("d7dde4")
const TEXT_DIM := Color("8b949e")
const ACCENT := Color("58a6ff")
const OK := Color("3fb950")
const WARN := Color("d29922")
const FAIL := Color("f85149")
const SPACE := [4, 8, 12, 16, 24]
const TYPE_SIZES := {"label": 12, "body": 14, "title": 18, "display": 28}
const SWATCHES := {"surface-0": SURFACE_0, "surface-1": SURFACE_1, "surface-2": SURFACE_2, "line": LINE,
	"text": TEXT, "text-dim": TEXT_DIM, "accent": ACCENT, "ok": OK, "warn": WARN, "fail": FAIL}
