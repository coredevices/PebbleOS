{
  description = "Development environment for PebbleOS";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-25.11";
  };

  outputs =
    { self, nixpkgs }:
    let
      sdkVersion = "0.1.2";
      sdkBundles = {
        aarch64-darwin = {
          osArch = "darwin-aarch64";
          sha256 = "e1536a252156d7278b01a0189d4516af281638c8d53a57c5c501523337e0b349";
        };
        aarch64-linux = {
          osArch = "linux-aarch64";
          sha256 = "3953612189b2c6d81f375f423f0138c792cd9fb564d1852687262556993b2159";
        };
        x86_64-linux = {
          osArch = "linux-x86_64";
          sha256 = "211aafa03c977c576f6d57f3b8517e3898c3cdb6d60e806c0b59dfe633774c0a";
        };
      };
      forSupportedSystems = nixpkgs.lib.genAttrs (builtins.attrNames sdkBundles);
    in
    {
      devShells = forSupportedSystems (
        system:
        let
          pkgs = import nixpkgs { inherit system; };
          bundle = sdkBundles.${system};
          pebbleos-sdk = pkgs.stdenv.mkDerivation {
            pname = "pebbleos-sdk";
            version = sdkVersion;
            src = pkgs.fetchurl {
              url = "https://github.com/coredevices/PebbleOS-SDK/releases/download/v${sdkVersion}/pebbleos-sdk-${sdkVersion}-${bundle.osArch}.tar.gz";
              sha256 = bundle.sha256;
            };

            nativeBuildInputs = pkgs.lib.optionals pkgs.stdenv.isLinux [
              pkgs.autoPatchelfHook
            ];
            buildInputs = pkgs.lib.optionals pkgs.stdenv.isLinux (with pkgs; [
              # arm-none-eabi host binaries (matches nixpkgs gcc-arm-embedded)
              ncurses6
              libxcrypt-legacy
              xz
              zstd
              # qemu-pebble host binaries
              glib
              pixman
              zlib
              stdenv.cc.cc.lib
            ]);

            dontConfigure = true;
            dontBuild = true;
            dontStrip = true;

            installPhase = ''
              runHook preInstall
              bash install.sh --prefix "$out" --defaults --force
              # gdb-py variants need a Python 3.8 not packaged in nixpkgs.
              rm -f "$out"/arm-none-eabi/bin/arm-none-eabi-gdb-py \
                    "$out"/arm-none-eabi/bin/arm-none-eabi-gdb-add-index-py
              # Surface every SDK binary under $out/bin so PATH inclusion picks them up.
              mkdir -p "$out/bin"
              for d in arm-none-eabi/bin qemu/bin sftool; do
                [ -d "$out/$d" ] || continue
                for f in "$out/$d"/*; do
                  [ -f "$f" ] && [ -x "$f" ] && ln -sf "$f" "$out/bin/$(basename "$f")"
                done
              done
              runHook postInstall
            '';
          };
        in
        {
          default = pkgs.mkShellNoCC {
            hardeningDisable = [ "fortify" ]; # waf expects unoptimized builds
            nativeBuildInputs = with pkgs; [
              pkg-config
            ];
            buildInputs = with pkgs; [
              pebbleos-sdk
              gettext
              git
              librsvg
              nodejs
              openocd
              protobuf
              python313
            ] ++ lib.optionals stdenv.isLinux [
              clang_multi
              gcc
              # Required for Moddable build
              dash
              glib
              gtk3
            ];
            shellHook = ''
            # Ensure that apple command line tools are installed on macOS
            ${pkgs.lib.optionalString pkgs.stdenv.isDarwin ''
                # Verify Apple Command Line Tools are installed
                if ! /usr/bin/xcrun --find clang &> /dev/null; then
                  echo "❌ Error: Apple Command Line Tools not found!"
                  echo "   Please install with: xcode-select --install"
                  exit 1
                fi
                echo "✓ Apple CLT found: $(/usr/bin/clang --version | head -1)"
              ''}
              # Disable pyenv to avoid conflicts
              export PYENV_VERSION=system
              unset PYENV_ROOT

              # Prepare the python venv
              export VENV_DIR=".venv"
              if [ ! -d "$VENV_DIR" ]; then
                echo "Creating virtual environment..."
                python -m venv "$VENV_DIR"
                source "$VENV_DIR/bin/activate"
                if [ -f "requirements.txt" ]; then
                  echo "Installing Python dependencies..."
                  pip install -r requirements.txt
                fi
              else
                source "$VENV_DIR/bin/activate"
              fi
              
              echo "Python virtual environment activated."
            '';
          };
        }
      );
    };
}
