{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = with pkgs; [
    binutils
    qemu
    gcc
    gnumake
    gdb
    file
  ];
}
