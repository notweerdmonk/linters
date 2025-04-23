# testscript_02_unused_var.gdb
# 2
# testscript_02_unused_var.gdb:06: Unused var: 'unused2' defined at line 6 is never used
# testscript_02_unused_var.gdb:05: Unused var: 'unused1' defined at line 5 is never used
set $unused1 = 1
set $unused2 = -1
