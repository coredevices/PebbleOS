# PBL-17248 set the charset to get around a libiconv bug
set charset US-ASCII
# fix up our python path first since GDB is likely linked with system (not brewed) python
source tools/gdb_scripts/gdb_python_path_fix.py
# source all tools
source tools/gdb_scripts/gdb_tintin.py
source tools/gdb_scripts/gdb_printers.py
