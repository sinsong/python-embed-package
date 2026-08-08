Windows 平台打包工具，使用 `python-version-embed-arch.zip`

## 使用方式

命令行参数：
- `--base URL-or-filepath` python embed 压缩包的下载地址，或者你下载好传路径
- `--getpip filepath` 可选，你下载好 PyPA get-pip.py 把路径传给他
- `--output` 可选，输出文件名，默认叫 `artifact.zip`
- 剩下的参数将传递给 `pip install`

## 思路

1. 复用 embed 压缩包
2. 进行调整
3. 安装应用，补充依赖
4. 学 `pythonXX.zip` 把 `.py` 转换成 `.pyc`
5. 删除一些不需要的文件
6. TODO: 添加启动器

## 其他内容

- `interpreter` 目录：链接 `libpythonXX.dll` 的启动特定入口的解释器，充当 `python.exe` 本体的角色
  - `pure-embedding.c` 程序性的加载 `module.func` 并调用
  - `simple-string.c` 使用 `PyRun_SimpleString` 内嵌脚本调用
- `shim` 目录：模仿 pip 合成的启动 `python.exe` 的启动器
