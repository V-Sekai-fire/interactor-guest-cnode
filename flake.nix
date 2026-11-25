# SPDX-FileCopyrightText: 2026 K. S. Ernest (iFire) Lee
# SPDX-License-Identifier: Apache-2.0 OR MIT
{
  description = "Erlang C node that hosts a godot-sandbox guest ELF in libgodot";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  inputs.entities-godot = { url = "github:V-Sekai-fire/entities-godot/0bac82758eadc9fc00b2c80753f0dce3304d50bc"; flake = false; };
  inputs.godot-sandbox = { url = "git+https://github.com/V-Sekai-fire/godot-sandbox?submodules=1"; flake = false; };
  outputs = { self, nixpkgs, entities-godot, godot-sandbox }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      libgodot = pkgs.callPackage ./nix/libgodot.nix { src = entities-godot; };
      sandbox-addon = pkgs.callPackage ./nix/godot-sandbox.nix { src = godot-sandbox; };
      guest-cnode = pkgs.callPackage ./nix/guest-cnode.nix { godotSandboxSource = godot-sandbox; };
    in {
      packages.${system} = { inherit libgodot sandbox-addon guest-cnode; default = guest-cnode; };
    };
}
