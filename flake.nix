{
  description = "Development environment for PebbleOS";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-25.11";
  };

  outputs =
    { self, nixpkgs }:
    let
      forSupportedSystems = nixpkgs.lib.genAttrs [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];
    in
    {
      devShells = forSupportedSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          gcc-arm-embedded-14_2r1 = pkgs.gcc-arm-embedded.overrideAttrs (old: rec {
            version = "14.2.rel1";
            src = pkgs.fetchurl {
              url = "https://developer.arm.com/-/media/Files/downloads/gnu/${version}/binrel/arm-gnu-toolchain-${version}-${old.platform}-arm-none-eabi.tar.xz";
              # hashes obtained from location ${url}.sha256asc
              sha256 =
                {
                  aarch64-darwin = "2d9e717dd4f7751d18936ae1365d25916534105ebcb7583039eff1092b824505";
                  aarch64-linux = "87330bab085dd8749d4ed0ad633674b9dc48b237b61069e3b481abd364d0a684";
                  x86_64-linux = "62a63b981fe391a9cbad7ef51b17e49aeaa3e7b0d029b36ca1e9c3b2a9b78823";
                }
                .${pkgs.stdenv.hostPlatform.system}
                  or (throw "Unsupported system: ${pkgs.stdenv.hostPlatform.system}");
            };
          });
        in
        {
          default = pkgs.mkShell {
            hardeningDisable = [ "fortify" ]; # waf expects unoptimized builds
            buildInputs = with pkgs; [
              clang_multi
              emscripten
              gcc
              gcc-arm-embedded-14_2r1
              gettext
              git
              nodejs
              openocd
              protobuf
              python313
            ];
            shellHook = ''
              # Prepare the python venv
              export VENV_DIR=".venv"
              if [ ! -d "$VENV_DIR" ]; then
                echo "Creating virtual environment..."
                python -m venv "$VENV_DIR"
                if [ -f "requirements.txt" ]; then
                  source "$VENV_DIR/bin/activate"
                  pip install -r requirements.txt
                fi
              fi

              # Activate the python venv
              source "$VENV_DIR/bin/activate"
              echo "Python virtual environment activated."

              # Allow waf to find the compilers it needs
              export CC=
              export CXX=
            '';
          };
        }
      );
    };
}
