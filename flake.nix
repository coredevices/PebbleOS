{
  description = "Development environment for PebbleOS";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-25.11";
  };

  outputs =
    { self, nixpkgs }:
    let
      sdkVersion = "0.1.3";
      sdkBundles = {
        aarch64-darwin = {
          osArch = "darwin-aarch64";
          sha256 = "bc47e9e8596769579a6cae257ffa8a5ebbfcdfb90b7b636b1baf457b37ae714a";
        };
        aarch64-linux = {
          osArch = "linux-aarch64";
          sha256 = "45ebceca406ef50fa29fc78c79bb892a8340c2dd7a0e04b8e1dd02dbff88c7de";
        };
        x86_64-linux = {
          osArch = "linux-x86_64";
          sha256 = "77bb95e8f39aa51371bc695b4b84fa915ad11ad9b2b14d66c59ba260a520aba8";
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

                # Moddable's mac/tools.mk generates launcher scripts via
                # `echo '...\n...'` and depends on `\n` being expanded. macOS
                # /bin/sh (bash 3.2, XSI-compliant in POSIX mode) does this,
                # but Nix's bash 5.x — picked up as `sh` via PATH — does not,
                # producing scripts with a malformed shebang. Pin make's SHELL
                # to /bin/sh so the recipe runs under the expected shell.
                export MAKEFLAGS="SHELL=/bin/sh"
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
