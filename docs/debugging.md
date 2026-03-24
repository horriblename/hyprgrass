# Obtaining a stack trace

If you are facing a crash, a back trace would be helpful for me to work out the
problem.

To do so, first compile Hyprgrass in debug mode.

```shell
meson setup build -Dbuildtype=debug
meson compile -Cbuild
```

Then, start nested Hyprland session with gdb.

```shell
# run this in an existing Hyprland session!
gdb --arg Hyprland -c /path/to/your/debug.conf
```

> [!WARNING]
>
> Do not try to debug directly from a TTY. Run gdb within an existing Hyprland
> session. Otherwise your system will hang.

Within the nested session, load Hyprgrass with:

```shell
hyprctl plugin load build/src/libhyprgrass.so
```

Once you encountered a crash, go back to your gdb terminal and run backtrace to
obtain a stack trace. Attach that when you make a bug report to help me identify
bugs faster :D
