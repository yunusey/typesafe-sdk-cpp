{
  description = "Unofficial C++23 SDK for the TypeSafe AI API";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {inherit system;};

        llvm = pkgs.llvmPackages_21;
        stdenv = llvm.stdenv;

        buildDeps = [
          pkgs.curl
          pkgs.nlohmann_json
        ];

        testDeps = [
          pkgs.gtest
        ];

        nativeDeps = [
          pkgs.cmake
          pkgs.ninja
          pkgs.pkg-config
        ];

        devTools = [
          llvm.clang-tools # clang-format, clang-tidy
          pkgs.gdb
        ];
      in {
        packages.default = stdenv.mkDerivation {
          pname = "typesafe-sdk-cpp";
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = nativeDeps;
          buildInputs = buildDeps ++ testDeps;
          cmakeFlags = [
            "-DTYPESAFE_BUILD_TESTS=ON"
            "-DTYPESAFE_BUILD_EXAMPLES=ON"
          ];
          doCheck = true;
          checkPhase = "ctest --output-on-failure";
        };

        devShells.default = (pkgs.mkShell.override {inherit stdenv;}) {
          packages = buildDeps ++ testDeps ++ nativeDeps ++ devTools;
        };
      }
    );
}
