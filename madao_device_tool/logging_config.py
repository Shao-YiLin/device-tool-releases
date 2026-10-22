"""统一日志配置。

提供 setup_logging() 一次性配置根 logger：
- 控制台输出始终启用
- 文件输出默认启用（写到 logs/ 目录，按日期轮转）
- 格式：[时间] [级别] [模块名] 消息

仅依赖 Python 标准库 logging。各业务模块用自己的 logger = logging.getLogger(__name__)
即可自动接入本配置。
"""

from __future__ import annotations

import logging
import os
import sys
from logging.handlers import TimedRotatingFileHandler
from typing import Optional

_LOG_FORMAT = "[%(asctime)s] [%(levelname)s] [%(name)s] %(message)s"
_DATE_FORMAT = "%Y-%m-%d %H:%M:%S"

_configured = False

# 默认日志目录：项目根目录下的 logs/
_DEFAULT_LOG_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "logs",
)


def setup_logging(level: str = "DEBUG", log_file: Optional[str] = None) -> logging.Logger:
    """配置根 logger（控制台 + 文件），幂等。

    多次调用只首次生效，避免重复 handler。返回根 logger。

    Args:
        level: 日志级别 "DEBUG"/"INFO"/"WARNING"/"ERROR"
        log_file: 自定义日志文件路径。不传则自动写到 logs/device_tool.log（按天轮转）。
    """
    global _configured
    root = logging.getLogger()

    if _configured:
        root.setLevel(level)
        return root

    formatter = logging.Formatter(_LOG_FORMAT, datefmt=_DATE_FORMAT)
    root.setLevel(level)

    # 控制台输出（只显示 WARNING 及以上，避免刷屏）
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel("WARNING")
    console_handler.setFormatter(formatter)
    root.addHandler(console_handler)

    # 文件输出（全部级别，按天轮转，保留 30 天）
    if log_file is None:
        os.makedirs(_DEFAULT_LOG_DIR, exist_ok=True)
        log_file = os.path.join(_DEFAULT_LOG_DIR, "device_tool.log")
    os.makedirs(os.path.dirname(os.path.abspath(log_file)), exist_ok=True)
    file_handler = TimedRotatingFileHandler(
        log_file, when="midnight", interval=1, backupCount=30, encoding="utf-8"
    )
    file_handler.suffix = "%Y-%m-%d.log"
    file_handler.setLevel("DEBUG")
    file_handler.setFormatter(formatter)
    root.addHandler(file_handler)

    _configured = True
    root.debug(f"日志系统初始化完成，文件: {log_file}")
    return root


def get_log_dir() -> str:
    """返回日志文件所在目录。"""
    return _DEFAULT_LOG_DIR
