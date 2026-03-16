{
  description = "OSv - Unikernel Operating System";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        # Use boost 1.77 - last version before boost::system became header-only
        # OSv requires libboost_system.a which was removed in later versions
        boostStatic = pkgs.boost177.override {
          enableStatic = true;
          enableShared = false;
        };
      in
      {
        devShells.default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
            # Core build tools
            gcc
            gnumake
            python3
            autoconf
            automake
            libtool
            patch
            bison
            flex
            git

            # Required libraries
            boostStatic
            libedit
            openssl
            yaml-cpp
            lua5_4

            # Development tools
            qemu
            gdb
            genromfs
            tcpdump
            pax-utils

            # Java toolchain (for tests)
            openjdk17
            maven
            ant

            # Python dependencies
            python3Packages.dpkt
            python3Packages.requests

            # Additional utilities
            curl
            wget
            unzip

            # Optional: cross-compilation for aarch64
            # Uncomment if needed:
            # pkgsCross.aarch64-multiplatform.buildPackages.gcc
          ];

          shellHook = ''
            echo "OSv development environment loaded"
            echo "Build with: ./scripts/build"
            echo "Run with: ./scripts/run.py"
            echo "Test with: ./scripts/test.py"
            echo ""
            echo "Current architecture: ${system}"

            # Set boost path for OSv Makefile
            # Boost 1.77 includes libboost_system.a, no wrapper needed
            export boost_base="${boostStatic}"
          '';

          # Environment variables
          OSV_ROOT = builtins.toString ./.;

          # Ensure Python can find required modules
          PYTHONPATH = "${pkgs.python3Packages.dpkt}/lib/python${pkgs.python3.pythonVersion}/site-packages:${pkgs.python3Packages.requests}/lib/python${pkgs.python3.pythonVersion}/site-packages";
        };

        # Provide formatter for `nix fmt`
        formatter = pkgs.nixpkgs-fmt;
      }
    );
}
