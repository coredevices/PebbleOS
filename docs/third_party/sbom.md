# Software bill of materials

The firmware build can describe the third-party components that go into a
firmware image as a [CycloneDX](https://cyclonedx.org) 1.6 software bill of
materials (SBOM). Releases ship one SBOM per firmware image, next to the
`.pbz`, `.bin` and `.elf` files.

## Generating an SBOM

The SBOM is not part of the default build. Build it on request, after
configuring as usual:

```shell
pbl build sbom
```

This writes `pebbleos.cdx.json` to the build directory. The SBOM requires the
Ninja generator, which `pbl configure` uses.

## What it contains

The SBOM is derived from the build itself, so it lists exactly the components
of the configured board and variant:

1. Ninja lists every input of `pebbleos.elf` (sources, objects, prebuilt
   libraries, resources) and, from its dependency logs, every header each
   object was compiled with.
2. Each file is matched against the component metadata (see below) by path.
   Files that belong to no component are PebbleOS's own code.
3. Every matched component is listed with its version, supplier, license and,
   when available, its package URL (purl) and CPE.

The top-level component is the firmware image itself, with its version (from
`git describe`) and the SHA-256 and SHA-512 hashes of the firmware binary.

Generation fails if a file from outside the tree, for example a toolchain
header, belongs to no component, or if a file lies in a submodule's own
`nested` third-party directory without a component of its own. This keeps
new dependencies from being left out silently.

## Component metadata

Components are described in `sbom.yml` files:

- `third_party/sbom.yml`: the submodules
- `fw/sbom.yml`, `lib/sbom.yml`: third-party code kept in the tree
- `cmake/toolchain/sbom.yml`: the toolchain runtime (C library, libgcc)

Each entry has these fields:

| Field | Description |
| --- | --- |
| `name` | Component name, unique across all files. |
| `path` / `paths` | Files or directories, relative to the `sbom.yml`. |
| `toolchain-path` | For toolchain components, a path relative to the toolchain root. |
| `build-paths` | Paths relative to the build directory, for components installed there. |
| `nested` | Subdirectories holding third-party code that needs its own component. |
| `supplier` | Who supplies the component. |
| `license` | SPDX license expression. Use `LicenseRef-` for licenses with no SPDX identifier. |
| `version` | Upstream version. Defaults to `commit`. |
| `version-from` | `compiler`, or a `file` and `define` to read the version from. |
| `commit` | For components inside a submodule, the submodule commit. |
| `type` | CycloneDX component type: `library` (default), `firmware` or `data`. |
| `purl`, `cpe` | Identifiers for vulnerability databases. `{version}` is substituted. |
| `url` | Upstream repository or website. |
| `description` | Short description. |

### Updating a submodule

The `commit` of every component inside a submodule must match the commit the
submodule points to. When updating a submodule, update its `version`,
`commit` and `purl` too. CI checks this on every pull request, as well as that
every submodule is described. To run the same check locally:

```shell
python3 tools/cmake/sbom.py check
```

### Adding third-party code

New submodules and third-party code copied into the tree need an entry in the
nearest `sbom.yml`.
