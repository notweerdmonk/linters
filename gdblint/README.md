# gdblint

###### Lint GDB scripts

WIP[^2]

Install gdb[^1]. The program relies on gdb output for creating syntax lookup."

```console
$ ./bin/gdblint ./tests/testscript_01_undefined_var.gdb
testscript_01_undefined_var.gdb:04: Undefined var: 'undefined_var' is referenced at line 4 but never defined
File: /home/runner/work/linters/linters/gdblint/tests/testscript_01_undefined_var.gdb
Found: 1 issue(s)
```

```console
$ make test
Testsuite directory: /home/runner/work/linters/linters/gdblint/tests

--------------------------------------------------------------------------------

Testing /home/runner/work/linters/linters/gdblint/tests/testscript_01_undefined_var.gdb

Reports:
testscript_01_undefined_var.gdb:04: Undefined var: 'undefined_var' is referenced at line 4 but never defined

Comparing reports: OK
PASSED

--------------------------------------------------------------------------------

Testing /home/runner/work/linters/linters/gdblint/tests/testscript_02_unused_var.gdb

Reports:
testscript_02_unused_var.gdb:06: Unused var: 'unused2' defined at line 6 is never used
testscript_02_unused_var.gdb:05: Unused var: 'unused1' defined at line 5 is never used

Comparing reports: OK
PASSED

--------------------------------------------------------------------------------

Testing /home/runner/work/linters/linters/gdblint/tests/testscript_03_undefined_func.gdb

Reports:
testscript_03_undefined_func.gdb:04: Undefined func: 'undefined_func' is referenced at line 4 but never defined

Comparing reports: OK
PASSED

--------------------------------------------------------------------------------

Testing /home/runner/work/linters/linters/gdblint/tests/testscript_04_unused_func.gdb

Reports:
testscript_04_unused_func.gdb:04: Unused func: 'unused_func' defined at line 4 is never used

Comparing reports: OK
PASSED

--------------------------------------------------------------------------------

Testing /home/runner/work/linters/linters/gdblint/tests/testscript_05_subcommand_ignore.gdb

No reports
Comparing reports: OK
PASSED

--------------------------------------------------------------------------------

Testing /home/runner/work/linters/linters/gdblint/tests/testscript_06_arch_mismatch_reg.gdb

No reports
Comparing reports: OK
PASSED

--------------------------------------------------------------------------------

Testing /home/runner/work/linters/linters/gdblint/tests/testscript_07_arch_correct_reg.gdb

No reports
Comparing reports: OK
PASSED

--------------------------------------------------------------------------------

Testing /home/runner/work/linters/linters/gdblint/tests/testscript_08_internal_conv_vars.gdb

Reports:
testscript_08_internal_conv_vars.gdb:06: Undefined var: '_foobar' is referenced at line 6 but never defined

Comparing reports: OK
PASSED

--------------------------------------------------------------------------------

Testing /home/runner/work/linters/linters/gdblint/tests/testscript_09_machine_regs.gdb

No reports
Comparing reports: OK
PASSED

--------------------------------------------------------------------------------

Testing /home/runner/work/linters/linters/gdblint/tests/testscript_10_user_regs.gdb

No reports
Comparing reports: OK
PASSED

--------------------------------------------------------------------------------

10 of 10 tests passed
```

[^1]: https://tinyurl.com/laubh
[^2]: Don't like the source code yet, raise a PR!
