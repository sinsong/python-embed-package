Windows 平台打包工具，使用 `python-version-embed-arch.zip`

## 使用方式

命令行参数：
- `--base URL-or-filepath` python embed 压缩包的下载目录，或者你下载好传路径
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
