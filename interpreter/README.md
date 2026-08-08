## simple-string.c

找到 `PyRun_SimpleString`，传递的参数就是内嵌的 Python 脚本。
基本上就是
```python
import sys
if __name__ == '__main__':
    from module import func
    sys.exit(func())
```

pypa/pip distlib SCRIPT_TEMPLATE 的例子：https://github.com/pypa/pip/blob/f7bfe280f00831b249534fc8e8a549cb48b3d166/src/pip/_vendor/distlib/scripts.py#L42-L49

## pure-embedding.c

找到 `MODULE` `ENTRY` 两个宏

```c
#define MODULE ""
#define ENTRY ""
```

将你的模块和入口函数填进去
