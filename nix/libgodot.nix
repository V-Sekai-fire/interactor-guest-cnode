# SPDX-FileCopyrightText: 2026 K. S. Ernest (iFire) Lee
# SPDX-License-Identifier: Apache-2.0 OR MIT
{ stdenv, lib, scons, pkg-config, python3, src, vulkan-loader, vulkan-headers
, libx11, libxcursor, libxinerama, libxi, libxrandr, libxrender, libxext, libxkbcommon
, libGL, alsa-lib, udev, zlib, fontconfig, dbus }:
stdenv.mkDerivation {
  pname = "libgodot";
  version = "4.x-entities";
  inherit src;
  nativeBuildInputs = [ scons pkg-config python3 ];
  buildInputs = [ vulkan-loader vulkan-headers libx11 libxcursor libxinerama libxi libxrandr libxrender libxext libxkbcommon libGL alsa-lib udev zlib fontconfig dbus ];
  enableParallelBuilding = true;
  sconsFlags = [ "platform=linuxbsd" "target=template_release" "precision=double" "library_type=shared_library"
    "wayland=no" "pulseaudio=no" "speechd=no" "dbus=no" "fontconfig=no" "udev=no" "tests=no" "disable_path_overrides=no" "module_mono_enabled=no" ];
  installPhase = ''
    mkdir -p $out/lib
    cp bin/*.so $out/lib/
  '';
  dontStrip = true;
}
