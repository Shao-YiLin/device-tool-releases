"""GitHub Releases 在线更新客户端。

无第三方依赖（标准库 urllib），通过 GitHub 公开 API 检查最新 Release、
下载资产包、校验 sha256，并生成重启替换脚本。

用法：
    from updater import Updater, UpdateInfo
    up = Updater()
    info = up.check()                    # None 表示已是最新
    if info:
        up.download(info, on_progress=cb)  # 下载到临时目录
        up.apply_and_restart(info)         # 退出当前进程并替换重启
"""

from __future__ import annotations

import hashlib
import json
import logging
import os
import re
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from dataclasses import dataclass
from typing import Callable, Optional

logger = logging.getLogger(__name__)

REPO = "Shao-YiLin/device-tool-releases"
API = f"https://api.github.com/repos/{REPO}/releases/latest"
ASSET_NAME = "MadaoDeviceTool.zip"  # Release 资产固定名
VERSION_FILE = "version.txt"        # 包内版本文件（exe 同级）


def current_version() -> str:
    """读取当前版本：开发模式取源码内 version.txt；打包模式取 exe 同级 version.txt。"""
    try:
        if getattr(sys, "frozen", False):
            base = os.path.dirname(sys.executable)
        else:
            base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        p = os.path.join(base, VERSION_FILE)
        if os.path.isfile(p):
            return open(p, encoding="utf-8").read().strip()
    except Exception:
        pass
    return "0.0.0"


def _cmp_version(a: str, b: str) -> int:
    """比较版本号 '1.2.3'（可带 v 前缀）。"""
    def norm(s):
        return [int(x) for x in re.findall(r"\d+", s)[:3]] + [0] * (3 - len(re.findall(r"\d+", s)[:3]))
    na, nb = norm(a), norm(b)
    return (na > nb) - (na < nb)


@dataclass
class UpdateInfo:
    version: str
    download_url: str
    size: int
    body: str = ""
    tag: str = ""


class Updater:
    def __init__(self, repo: str = REPO, timeout: float = 15.0) -> None:
        self._api = f"https://api.github.com/repos/{repo}/releases/latest"
        self._timeout = timeout

    # ---- 检查 ----
    def check(self) -> Optional[UpdateInfo]:
        """检查是否有新版本。返回 UpdateInfo 或 None。"""
        try:
            data = self._fetch_json(self._api)
        except urllib.error.HTTPError as e:
            if e.code == 404:
                raise RuntimeError("更新服务器尚未发布任何版本") from e
            raise RuntimeError(f"检查更新失败（HTTP {e.code}）") from e
        except (urllib.error.URLError, TimeoutError) as e:
            raise RuntimeError(f"检查更新失败（网络错误）: {e}") from e
        tag = data.get("tag_name", "")
        remote_ver = tag.lstrip("v")
        local_ver = current_version()
        if not remote_ver:
            return None
        if _cmp_version(remote_ver, local_ver) <= 0:
            return None
        asset = next((a for a in data.get("assets", []) if a.get("name") == ASSET_NAME), None)
        if asset is None:
            raise RuntimeError(f"Release {tag} 中未找到资产 {ASSET_NAME}")
        return UpdateInfo(
            version=remote_ver,
            download_url=asset["browser_download_url"],
            size=int(asset.get("size", 0)),
            body=data.get("body", ""),
            tag=tag,
        )

    # ---- 下载 ----
    def download(self, info: UpdateInfo,
                 on_progress: Optional[Callable[[int, int], None]] = None) -> str:
        """下载 zip 到临时目录，返回文件路径。"""
        dest = os.path.join(tempfile.gettempdir(), f"madao_update_{info.version}.zip")
        with urllib.request.urlopen(info.download_url, timeout=self._timeout) as resp, \
                open(dest, "wb") as f:
            total = int(resp.headers.get("Content-Length") or info.size)
            done = 0
            while True:
                chunk = resp.read(1 << 16)
                if not chunk:
                    break
                f.write(chunk)
                done += len(chunk)
                if on_progress:
                    on_progress(done, total)
        logger.info(f"更新包已下载: {dest}")
        return dest

    @staticmethod
    def verify_sha256(path: str, expected_hex: str) -> bool:
        """校验 zip 的 sha256。"""
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 16), b""):
                h.update(chunk)
        return h.hexdigest().lower() == expected_hex.strip().lower()

    # ---- 替换重启 ----
    def apply_and_restart(self, zip_path: str) -> None:
        """解压并生成重启替换脚本，启动后当前进程退出。

        替换策略：bat 脚本等待主进程退出后，用解压出的新文件覆盖
        exe 所在目录，然后重启 MadaoDeviceTool.exe。
        """
        if not getattr(sys, "frozen", False):
            raise RuntimeError("仅打包环境支持自动替换重启")
        exe_dir = os.path.dirname(sys.executable)
        exe_name = os.path.basename(sys.executable)
        work = os.path.join(tempfile.gettempdir(), f"madao_update_extract_{os.getpid()}")
        os.makedirs(work, exist_ok=True)
        with zipfile.ZipFile(zip_path) as z:
            z.extractall(work)
        # zip 结构为 MadaoDeviceTool/ 或裸目录
        src = os.path.join(work, "MadaoDeviceTool")
        if not os.path.isdir(src):
            src = work
        bat = os.path.join(tempfile.gettempdir(), f"madao_update_{os.getpid()}.bat")
        # bat: 等待进程退出 -> 覆盖文件 -> 重启
        with open(bat, "w", encoding="gbk") as f:
            f.write(
                "@echo off\r\n"
                "chcp 65001 >nul\r\n"
                "timeout /t 2 /nobreak >nul\r\n"
                "taskkill /F /PID %PID% >nul 2>nul\r\n"
                f'xcopy /E /Y /I /R "{src}" "{exe_dir}" >nul\r\n'
                f'start "" "{os.path.join(exe_dir, exe_name)}"\r\n'
                f'del "%~f0"\r\n'.replace("%PID%", str(os.getpid()))
            )
        logger.info(f"启动更新脚本: {bat}")
        subprocess.Popen(["cmd.exe", "/c", bat],
                         creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        # 退出当前进程（交给 main.py 的 os._exit）
        os._exit(0)

    # ---- 内部 ----
    def _fetch_json(self, url: str) -> dict:
        req = urllib.request.Request(url, headers={
            "User-Agent": "MadaoDeviceTool-updater",
            "Accept": "application/vnd.github+json",
        })
        with urllib.request.urlopen(req, timeout=self._timeout) as resp:
            return json.loads(resp.read().decode("utf-8"))
