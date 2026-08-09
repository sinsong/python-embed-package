rc app.rc
rem /link 必须在编译器输入文件的后面
cl /nologo /EHsc /DWIN32 /DNDEBUG /D_CONSOLE shim.c app.res /Fe:shim-cli.exe /link /SUBSYSTEM:CONSOLE
cl /nologo /EHsc /DWIN32 /DNDEBUG /D_WINDOWS shim.c app.res /Fe:shim-gui.exe /link /SUBSYSTEM:WINDOWS
