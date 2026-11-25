# SPDX-FileCopyrightText: 2026 K. S. Ernest (iFire) Lee
# SPDX-License-Identifier: Apache-2.0 OR MIT
{ stdenv, scons, pkg-config, python3, src }:
stdenv.mkDerivation {
  pname = "godot-sandbox";
  version = "0-unstable";
  inherit src;
  nativeBuildInputs = [ scons pkg-config python3 ];
  enableParallelBuilding = true;
  sconsFlags = [ "platform=linux" "arch=x86_64" "target=template_release" "precision=double" "use_mingw=no" ];
  installPhase = ''
    mkdir -p $out
    cp -r bin $out/bin
  '';
  dontStrip = true;
}
