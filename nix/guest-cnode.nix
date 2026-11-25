# SPDX-FileCopyrightText: 2026 K. S. Ernest (iFire) Lee
# SPDX-License-Identifier: Apache-2.0 OR MIT
{ stdenv, erlang, godotSandboxSource }:
stdenv.mkDerivation {
  pname = "guest-cnode";
  version = "0.1.0";
  src = ../src;
  buildInputs = [ erlang ];
  buildPhase = ''
    interface=$(echo ${erlang}/lib/erlang/lib/erl_interface-*)
    $CC -O2 -std=gnu11 -Wall -Wextra -I$interface/include -I${godotSandboxSource}/ext/godot-cpp/gdextension \
      guest_cnode.c -L$interface/lib -lei -ldl -lpthread -o guest-cnode
  '';
  installPhase = "install -D guest-cnode $out/bin/guest-cnode";
}
