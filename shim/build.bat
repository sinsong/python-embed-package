cl /nologo /EHsc /DWIN32 /DNDEBUG /D_CONSOLE /SUBSYSTEM:CONSOLE shim.c /Fe:shim-cli.exe
cl /nologo /EHsc /DWIN32 /DNDEBUG /D_WINDOWS /SUBSYSTEM:WINDOWS shim.c /Fe:shim-gui.exe
