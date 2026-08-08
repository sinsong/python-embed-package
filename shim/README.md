修改 `shim.c` 中的 `RUN_MODULE` 宏的值

```c
#define RUN_MODULE L""
```

然后用 build.bat 编译（使用开发人员命令提示符环境）

## 作用

跟 `python.exe` 放到同一个目录，去启动硬编码的特定 `module`。
