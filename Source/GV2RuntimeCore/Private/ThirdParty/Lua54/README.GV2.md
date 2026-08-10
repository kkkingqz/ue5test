# Lua 5.4.8 integration notes

Source: `https://www.lua.org/ftp/lua-5.4.8.tar.gz`

SHA-256: `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`

Machine-readable integration metadata is recorded in `BuildManifest.json`.

The upstream license is preserved in `LICENSE`. GV2 embeds the runtime sources but
does not compile the `lua`/`luac` command-line programs or the `io`, `os`, `debug`,
`coroutine`, and package-loader libraries.

Local build adaptation: `lprefix.h` suppresses Clang's
`-Wunreachable-code-break` and `-Wunreachable-code-return` diagnostics for
intentional upstream fallback statements because Unreal Build Tool promotes
them to errors. No runtime behavior is changed.
