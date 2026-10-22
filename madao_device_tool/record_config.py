"""录音全局配置。

由 GUI 菜单栏控制，各面板共享同一状态。
"""
from __future__ import annotations

import os

# 项目根目录
_PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# 默认录音保存目录
DEFAULT_RECORD_DIR = os.path.join(_PROJECT_ROOT, "records")

# 是否保存录音文件到磁盘（默认开启）
save_to_file: bool = True

# 当前保存目录
record_dir: str = DEFAULT_RECORD_DIR


def ensure_record_dir() -> str:
    """确保录音目录存在，返回路径。"""
    os.makedirs(record_dir, exist_ok=True)
    return record_dir


def set_save_enabled(enabled: bool) -> None:
    """设置是否保存录音文件。"""
    global save_to_file
    save_to_file = bool(enabled)


def set_record_dir(path: str) -> None:
    """设置录音保存目录。"""
    global record_dir
    record_dir = path
    os.makedirs(record_dir, exist_ok=True)


def is_save_enabled() -> bool:
    return save_to_file


def get_record_dir() -> str:
    return record_dir
