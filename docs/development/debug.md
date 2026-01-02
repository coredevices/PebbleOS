# Debugging

#### Prerequisites

```bash
source .venv/bin/activate
pip install pebble-tool
```

**Explanation:**
- `pbl` = **internal** tool in this project (located in `python_libs/pbl/`)
- `pebble-tool` = **public** package from PyPI (https://github.com/coredevices/pebble-tool)
- `pbl` requires `pebble-tool` to work (see `python_libs/pbl/pbl/__init__.py` line 3)
- Only install `pebble-tool`, `pbl` is already in the project

```{tip}
Remember to have Pebble core mobile app with LAN developer connection enabled.
```

## Logs

#### Usage

```bash
pebble logs --phone 192.168.1.100
```

## Getting Coredumps

#### Usage

```bash
pbl coredump --phone 192.168.1.100
```

#### Notes

- Coredump is saved as `pebble_coredump_YYYY-MM-DD_HH-MM-SS.core`

## Read Coredumps

#### Convert a Pebble core dump to an ELF file.

```bash
tools/readcore.py <coredump_file>.core core.elf
```

#### Debug with GDB

```bash
arm-none-eabi-gdb build/src/fw/tintin_fw.elf core.elf
```
#### Commands inside GDB

```bash
(gdb) pbl help
```


