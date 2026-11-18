{
  description = "Development environment for PebbleOS";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-26.05";
  };

  outputs =
    { self, nixpkgs }:
    let
      sdkVersion = "0.1.16";
      sdkBundles = {
        aarch64-darwin = {
          osArch = "darwin-aarch64";
          sha256 = "6ba21b4ba6d7d9d67fa46371e144f9251db001f37c9a8dc00e7aed62341e08ce";
        };
        aarch64-linux = {
          osArch = "linux-aarch64";
          sha256 = "4b7c806ea4d9b325ef8909513f9f81bd1a8b94bd25b367218de7ab918f8f6097";
        };
        x86_64-linux = {
          osArch = "linux-x86_64";
          sha256 = "7a2aec2369d752a5b87da5ccd28bd25c790ccd38531911a3649cb3265cf473ce";
        };
      };
      forSupportedSystems = nixpkgs.lib.genAttrs (builtins.attrNames sdkBundles);
      requirementsHash = builtins.hashString "sha256" (
        builtins.concatStringsSep "\n" (map builtins.readFile [
          ./requirements.txt
          ./tools/libs/pbl-cli/pyproject.toml
          ./tools/libs/pbl-console/pyproject.toml
          ./tools/libs/pebble-loghash/pyproject.toml
        ])
      );
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
              ncurses5 # aarch64-linux toolchain gdb links ABI-5 ncurses/tinfo
              libxcrypt-legacy
              xz
              zstd
              # qemu-pebble host binaries
              glib
              pixman
              zlib
              stdenv.cc.cc.lib
              SDL2
              libpng
              alsa-lib
              libpulseaudio
              # sftool host binary
              systemdLibs
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
            PEBBLEOS_SDK_ROOT = "${pebbleos-sdk}";
            hardeningDisable = [ "fortify" ]; # the firmware is built unoptimized
            packages = with pkgs; [
              pebbleos-sdk
              ccache
              cmake
              dash
              dtc
              gettext
              git
              librsvg
              nodejs
              openocd
              protobuf
              python313
              meson
              ninja
              pkg-config
              # Applications in Rust; rustup installs rust-toolchain.toml.
              rust-bindgen
              rustup
              uv
              # Host compiler for Moddable and tests; x86 Linux tests need multilib.
              (if stdenv.isLinux && stdenv.hostPlatform.isx86_64 then clang_multi else clang)
            ] ++ lib.optionals stdenv.isLinux [
              gcc
            ];
            buildInputs = with pkgs; [
              # Native board window
              SDL2
            ] ++ lib.optionals stdenv.isDarwin [
              apple-sdk
            ] ++ lib.optionals stdenv.isLinux [
              # Required for Moddable build
              glib
              gtk3
            ];
            shellHook = ''
              # Moddable's launcher recipes need echo to expand backslash escapes.
              export MAKEFLAGS="''${MAKEFLAGS:+$MAKEFLAGS }SHELL=${pkgs.dash}/bin/dash"

              export VENV_DIR=".venv"
              if [ ! -f "$VENV_DIR/pyvenv.cfg" ]; then
                uv venv --python ${pkgs.python313.interpreter} "$VENV_DIR" || exit 1
              fi
              source "$VENV_DIR/bin/activate" || exit 1

              # Refresh only when requirements or editable package metadata change.
              requirements_stamp="$VENV_DIR/.requirements.sha256"
              if [ "${requirementsHash}" != "$(cat "$requirements_stamp" 2>/dev/null)" ]; then
                uv pip install --python "$VENV_DIR/bin/python" -r requirements.txt || exit 1
                printf '%s\n' "${requirementsHash}" > "$requirements_stamp"
              fi
            '';
          };
        }
      );
    };
}
