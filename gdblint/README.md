# gdblint

###### Lint GDB scripts

WIP[^2]

Install gdb[^1]. The program relies on gdb output for creating syntax lookup."

```console
$ ./bin/gdblint ./tests/testscript_01_undefined_var.gdb
testscript_01_undefined_var.gdb:04: Undefined var: 'undefined_var' is referenced at line 4 but never defined
File: /home/weerdmonk/Projects/linters/gdblint/tests/testscript_01_undefined_var.gdb
Found: 1 issue(s)
```

[^1]: https://tinyurl.com/laubh
[^2]: Don't like the source code yet, raise a PR!
