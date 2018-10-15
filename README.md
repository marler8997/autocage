
Consdier programs that can expose their inputs, outputs and runtime dependencies for each invocation. With this information, you could limit those programs to only have access to exactly what they need at runtime.  For example, the `-o <file>` option for `gcc` indicates that it needs write access to `<file>`.  By exposing this information, a container could limit all invocations of `gcc` by only allowing it write access to the file specified with `-o`.  The same would go for read access as well, for example, `gcc` needs read access to each source file passed in, so the container could limit read access to only the source files given on the command line.

For this idea, I'm thinking I should design a "program interface definition" format (maybe just use json).  Something like this:

```json
{
    "interface" : {
        "sourcefiles" : {
            "type" : {
                "name" : "file",
                "access" : ["read"],
                "must_exist" : true
            },
            "cmd_interface" : null,
        },
        "outfile" : {
            "type" : {
                "name" : "file",
                "access" : ["write"]
            },
            "max_count" : 1,
            "cmd_interface" : ["-o $"]
        }
    },
    "cmd_line": {
        // -o file
        "-o" : "outfile",
        // -o=file
        "-o=%" : "outfile",
        // -ofile
        "-o%" : "outfile"
    }
}
```

The `rex` program should load this "program interface definition" and configure a filesystem before starting the process, i.e.
```
rex gcc ...
```

`rex` would look for `gcc.rex` in the same directory as `gcc`, then load that into a kernel module that exposes a filesystem for the new `gcc` process that only allows it access to the files it has declared it needs.  `rex` should also be able to take the full command invocation and return information about it.  For example:
```
rex -info gcc -o hello hello.c

# Prints something like the following
write: hello
read: hello.c
```

With this information, you have ALL the information you need in order to reproduce the environment to run the build again.  This allows you to know exactly what the program is doing.

Note that executables also depend on their libraries, so rex should not only read the "program interface", it should also read the executable itself to figure out what libraries it depdnds on (note: see the `ldd` program), so the previous example would look more like:

```
rex -info gcc -o hello hello.c

# Prints something like the following
write: hello
read: hello.c
read: /lib/x86_64-linux-gnu/libc.so.6
read: /lib64/ld-linux-x86-64.so.2
```
