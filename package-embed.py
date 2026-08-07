from argparse import ArgumentParser, REMAINDER
from pathlib import Path
from subprocess import Popen
import logging
from zipfile import ZipFile, is_zipfile
import re
import shutil
from urllib.parse import urlparse
from urllib.request import urlretrieve

# https://www.python.org/downloads/
# curl -sSL https://bootstrap.pypa.io/get-pip.py -o get-pip.py

# site.ENABLE_USER_SITE = False
# python 添加 -s 参数

logger = logging.getLogger(__name__)

logging.basicConfig(level=logging.DEBUG)

def get_argument_parser():
    parser = ArgumentParser()
    parser.add_argument("--base", help="access to python-version-embed-arch.zip (URL or filepath)")
    parser.add_argument("--getpip", nargs='?', help="optional access to get-pip.py when you have Internet access trouble")
    parser.add_argument("--output", "-o", help="output filename", default="artifact.zip")
    parser.add_argument('installs', nargs=REMAINDER, help="arguments pass to pip install, e.g. your add and your dependencies")
    return parser

def patch_pth(bdir :Path):
    pth :Path = next(bdir.glob("*._pth"))
    logger.info(f"patching pth file {pth}")
    with pth.open("r+") as f:
        content = f.read()
        content = re.sub(r'#import site', "import site", content)
        f.seek(0)
        f.write(content)
        f.truncate()

def install_pip(bdir :Path, getpip :Path):
    interpreter = bdir / "python.exe"

    p = Popen([interpreter, getpip])
    logger.info(f"install pip exec: {p.args}")
    status = p.wait()
    logger.info(f"install-pip: return status: {status}")

def pip_install(bdir :Path, arguments):
    interpreter = bdir / "python.exe"
    installprefix = bdir / "Lib" / "site-packages"

    if not installprefix.exists():
        logger.info(f"mkdir -> {installprefix}")
        installprefix.mkdir()

    p = Popen([interpreter, "-m", "pip", "install", "--target", installprefix, '--'] + arguments)
    logger.info(f"pip install -- {p.args}")
    status = p.wait()
    logger.info(f"pip install: return status: {status}")

    # delete .dist-info directories
    for distInfo in installprefix.glob("*.dist-info"):
        logger.info(f"rmtree {distInfo}")
        shutil.rmtree(distInfo)

    shutil.rmtree(installprefix / "pip")

def compile_all(bdir :Path):
    interpreter = bdir / "python.exe"

    p = Popen([interpreter, "-m", "compileall", "-b", bdir])
    logger.info(f"compileall: {p.args}")
    status = p.wait()
    logger.info(f"compile_all: return status: {status}")

def strip(bdir :Path):
    logger.info("strip files...")

    logger.info("remove pre-compiled source (.pyc -> .py)")
    for pyc in bdir.rglob("**/*.pyc"):
        py = pyc.with_suffix(".py")
        if py.exists():
            logger.info(f"unlink {py}")
            py.unlink()

    logger.info("purge __pycache__ directory")
    for cachedir in bdir.rglob("**/__pycache__"):
        if cachedir.is_dir():
            logger.info(f"rmtree {cachedir}")
            shutil.rmtree(cachedir)

    logger.info("remove additional files")
    for top, dirs, files in bdir.walk():
        for file in files:
            file = top / file
            match file.suffix:
                case ".typed" | ".rst" | ".md":
                    logger.info(f"unlink {file}")
                    file.unlink()

    logger.info("rmtree Scripts directory")
    shutil.rmtree(bdir / "Scripts")

def main() -> int:
    parser = get_argument_parser()
    args = parser.parse_args()

    match urlparse(args.base).scheme:
        case "http" | "https":
            logging.info(f"downloading python embed zip... {args.base}")
            basepath, _ = urlretrieve(args.base)
            basepath = Path(basepath).resolve()
        case _:
            basepath = Path(args.base).resolve()
            if not basepath.is_file():
                raise AssertionError(f"--base {args.base} must be URL to download or regular file path")
            if not is_zipfile(basepath):
                raise AssertionError(f"{basepath} must be zip file")

    if args.getpip is not None:
        getpippath = Path(args.getpip).resolve()
        if not getpippath.is_file():
            raise AssertionError(f"--getpip {args.getpip} must be URL to download or regular file path")
    else:
        logging.info("downloading get-pip.py...")
        getpippath, _ = urlretrieve("https://bootstrap.pypa.io/get-pip.py")
        getpippath = Path(getpippath).resolve()

    logger.info(f"extra argument to pass: {args.installs}")

    bdir = Path("./package-workdir/").resolve()

    if bdir.exists():
        if bdir.is_dir():
            logger.info(f"build dir {bdir} exists")
        else:
            raise AssertionError(f"path {bdir} must be directory!")

    basezip = ZipFile(basepath)
    # 解压
    basezip.extractall(bdir)
    patch_pth(bdir)
    install_pip(bdir, Path(getpippath).resolve())
    pip_install(bdir, args.installs)
    compile_all(bdir)
    strip(bdir)

    logger.info("create archive ...")
    with ZipFile(args.output, "x") as artifact:
        for path in bdir.rglob('*'):
            if path.is_file():
                arcname = path.relative_to(bdir)
                logger.info(f"add {arcname}")
                artifact.write(path, arcname=arcname)

    return 0

if __name__ == "__main__":
    exit(main())
