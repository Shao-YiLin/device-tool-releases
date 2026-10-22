"""音频格式转换：OPUS -> PCM（基于 64 位 libopus，ctypes 直调）。

本项目录音为原始 Opus packet 序列（无 Ogg 封装）：
- 单声道录音（CALL_*）：20ms Hybrid 包，固定 40 字节/帧
- 双通道录音（NOTE_*）：20ms 全频带立体声包，固定 80 字节/帧（40*2）

opus.dll 来自 PyOGG 包（64 位），开发模式下通过 pyogg 定位；
打包模式需将 opus.dll 一并打入。
"""

from __future__ import annotations

import ctypes
import os
import threading

_FRAME_MS = 20  # 每包 20ms
_SAMPLE_RATE = 16000
_MAX_PACKET = 4000  # opus 包最大字节数
_SAMPLES_PER_FRAME = _SAMPLE_RATE * _FRAME_MS // 1000  # 320 采样/帧/声道

_dll_lock = threading.Lock()
_dll_cache: ctypes.CDLL | None = None
_dll_dir_cache: str | None = None

OPUS_OK = 0


def _find_opus_dll() -> str:
    """定位 opus.dll：优先项目 so 目录，其次 PyOGG 包自带。"""
    from _bootstrap import resource_path
    path = resource_path("so", "opus.dll")
    if os.path.isfile(path):
        return path
    try:
        import pyogg
        path = os.path.join(os.path.dirname(pyogg.__file__), "opus.dll")
        if os.path.isfile(path):
            return path
    except Exception:
        pass
    raise FileNotFoundError("未找到 opus.dll（so/ 目录或 PyOGG 包）")


def _load_lib() -> ctypes.CDLL:
    global _dll_cache, _dll_dir_cache
    with _dll_lock:
        if _dll_cache is not None:
            return _dll_cache
        dll_path = _find_opus_dll()
        dll_dir = os.path.dirname(dll_path)
        if hasattr(os, "add_dll_directory"):
            os.add_dll_directory(dll_dir)
        dll = ctypes.CDLL(dll_path)

        # libopus C API
        dll.opus_decoder_create.argtypes = [ctypes.c_int32, ctypes.c_int, ctypes.POINTER(ctypes.c_int)]
        dll.opus_decoder_create.restype = ctypes.c_void_p
        dll.opus_decoder_destroy.argtypes = [ctypes.c_void_p]
        dll.opus_decoder_destroy.restype = None
        dll.opus_decode.argtypes = [
            ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int32,
            ctypes.c_void_p, ctypes.c_int, ctypes.c_int]
        dll.opus_decode.restype = ctypes.c_int

        _dll_cache = dll
        _dll_dir_cache = dll_dir
        return dll


def opus_to_pcm(input_file: str, output_file: str | None = None,
                channels: int = 1, logger=None) -> str | None:
    """将原始 Opus packet 序列文件解码为 16bit PCM（16kHz）。

    Args:
        input_file: 输入 .opus 文件路径
        output_file: 输出 .pcm 路径；缺省为输入同目录同名 .pcm
        channels: 声道数（1=单声道，2=立体声）
        logger: 日志函数（可选）

    Returns:
        输出文件路径，失败返回 None。
    """
    if channels not in (1, 2):
        if logger:
            logger(f"不支持的声道数: {channels}（仅支持 1/2）")
        return None
    if output_file is None:
        output_file = os.path.splitext(input_file)[0] + ".pcm"

    try:
        dll = _load_lib()

        with open(input_file, "rb") as f:
            input_data = f.read()
        if not input_data:
            if logger:
                logger(f"文件为空: {input_file}")
            return None

        err = ctypes.c_int(0)
        dec = dll.opus_decoder_create(_SAMPLE_RATE, channels, ctypes.byref(err))
        if err.value != OPUS_OK or not dec:
            if logger:
                logger(f"opus 解码器创建失败 err={err.value}")
            return None

        try:
            # opus_decode 返回每声道采样数，buffer 需容纳 samples*channels
            frame_capacity = _SAMPLES_PER_FRAME * channels
            out_buf = (ctypes.c_int16 * frame_capacity)()
            total = bytearray()
            fail_count = 0
            offset = 0
            data_len = len(input_data)
            step = 40 * channels  # 本项目固定 40 字节/声道 分帧存储

            while offset < data_len:
                packet = input_data[offset:offset + step]
                offset += step
                # 末帧不足时补零对齐（与旧 mspeech_pcm 行为一致）
                if len(packet) < step:
                    packet = packet + b"\x00" * (step - len(packet))

                nbytes = dll.opus_decode(
                    dec, packet, len(packet), out_buf, frame_capacity, 0)
                if nbytes >= 0:
                    # nbytes = 每声道采样数，实际采样 = nbytes*channels
                    n = nbytes * channels
                    total.extend(memoryview(out_buf)[:n * 2].tobytes())
                else:
                    fail_count += 1
                    if fail_count <= 3 and logger:
                        logger(f"解码失败帧 offset={offset} err={nbytes}")
        finally:
            dll.opus_decoder_destroy(dec)

        if not total:
            if logger:
                logger(f"转换失败，无有效 PCM 数据: {input_file}")
            return None

        with open(output_file, "wb") as f:
            f.write(bytes(total))

        if logger:
            logger(f"转换成功: {input_file} -> {output_file} "
                   f"({len(total)} 字节, {channels}ch, 失败帧 {fail_count})")
        return output_file

    except Exception as e:
        if logger:
            logger(f"转换文件失败 {input_file}: {e}")
        return None
