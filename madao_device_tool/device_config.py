"""设备配置：product_no -> 设备元信息映射。

新格式（替代 src_archive/mouseTool/devices.json）：
- 按 product_no（4 位数字字符串）索引
- 每个设备含 name / type / capabilities
- type 判定规则：首位 1xxx=mouse, 3xxx=keyboard, 4xxx=other, 9xxx=special
- 配置数据内嵌在 Python，不依赖外部 JSON 文件，便于打包

数据源：src_archive/mouseTool/devices.json（22 个设备）。
能力集按设备类型常识：
  - mouse(1xxx):    ["cpi", "polling_rate", "battery"]
  - keyboard(3xxx): ["rgb", "custom_key", "wheel", "battery"]
  - other(4xxx):    []
  - special(9xxx):  []
"""

from __future__ import annotations

from typing import Dict, List, Optional

# 鼠标默认能力（1xxx）
_MOUSE_CAPS: List[str] = ["cpi", "polling_rate", "battery"]
# 键盘默认能力（3xxx）
_KEYBOARD_CAPS: List[str] = ["rgb", "custom_key", "wheel", "battery"]

# product_no -> (name, type, capabilities)
# name / image 数据来自 src_archive/mouseTool/devices.json
_DEVICES: Dict[str, Dict] = {
    # ---- 鼠标 1xxx ----
    "1000": {"name": "M520", "type": "mouse", "image": "m520pro_black.png", "capabilities": list(_MOUSE_CAPS)},
    "1001": {"name": "M110", "type": "mouse", "image": "m110_black.png", "capabilities": list(_MOUSE_CAPS)},
    "1002": {"name": "M520", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1003": {"name": "M610", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1004": {"name": "M110", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1005": {"name": "M610", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1006": {"name": "M220", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1007": {"name": "M110", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1008": {"name": "M110", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1009": {"name": "M110", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1010": {"name": "M111", "type": "mouse", "image": "m111_black.png", "capabilities": list(_MOUSE_CAPS)},
    "1011": {"name": "G50", "type": "mouse", "image": "g50_black.png", "capabilities": list(_MOUSE_CAPS)},
    "1012": {"name": "AM30", "type": "mouse", "image": "lite_m320_black.png", "capabilities": list(_MOUSE_CAPS)},
    "1013": {"name": "M520", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1014": {"name": "M320", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1015": {"name": "M320", "type": "mouse", "image": "lite_m320_white.png", "capabilities": list(_MOUSE_CAPS)},
    "1016": {"name": "M210W", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1017": {"name": "F20", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1018": {"name": "FeiShu-S", "type": "mouse", "image": "standard_black.png", "capabilities": list(_MOUSE_CAPS)},
    "1019": {"name": "PRO MAX", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1020": {"name": "M211X", "type": "mouse", "image": "m211_black.png", "capabilities": list(_MOUSE_CAPS)},
    "1021": {"name": "M610", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1022": {"name": "M210X", "type": "mouse", "image": "m210_black_red.png", "capabilities": list(_MOUSE_CAPS)},
    "1023": {"name": "AM50", "type": "mouse", "image": "am50_white.png", "capabilities": list(_MOUSE_CAPS)},
    "1024": {"name": "M110", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1025": {"name": "M610 spark", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1026": {"name": "M110", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1027": {"name": "M110T", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1028": {"name": "AM50", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1029": {"name": "M320 spark", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1030": {"name": "M110", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1031": {"name": "AM50 Ultra", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1032": {"name": "AM50", "type": "mouse", "image": "am50_qg.png", "capabilities": list(_MOUSE_CAPS)},
    "1033": {"name": "M520 Spark", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1034": {"name": "AM70 Pro", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1035": {"name": "M610 Pro", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1036": {"name": "M211 Spark", "type": "mouse", "image": "m211_spark.png", "capabilities": list(_MOUSE_CAPS)},
    "1037": {"name": "AM10", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1038": {"name": "M111 Spark", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1039": {"name": "M110", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1040": {"name": "M111", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1041": {"name": "M111 Spark", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1042": {"name": "AM60", "type": "mouse", "image": "M610_Ultra.png", "capabilities": list(_MOUSE_CAPS)},
    "1043": {"name": "AM50", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1044": {"name": "AM50 Pro", "type": "mouse", "image": "am50_white.png", "capabilities": list(_MOUSE_CAPS)},
    "1045": {"name": "AM50", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1046": {"name": "M320 Spark", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1047": {"name": "AM50 Ultra+", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1048": {"name": "M110", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "1049": {"name": "AM40", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    # ---- 键盘 3xxx ----
    # 注：当前 AT80 (3007) 是唯一支持 RGB/滚轮功能的键盘；
    # 其他键盘（基础信息已含 product_no/mac/version/battery）按需后续扩展 capabilities。
    "3000": {"name": "T8", "type": "keyboard", "image": "t8.png", "capabilities": []},
    "3001": {"name": "T8", "type": "keyboard", "image": "", "capabilities": []},
    "3002": {"name": "T6", "type": "keyboard", "image": "", "capabilities": []},
    "3003": {"name": "T9", "type": "keyboard", "image": "", "capabilities": []},
    "3004": {"name": "T8", "type": "keyboard", "image": "", "capabilities": []},
    "3005": {"name": "D1", "type": "keyboard", "image": "k320.png", "capabilities": []},
    "3006": {"name": "T8", "type": "keyboard", "image": "", "capabilities": []},
    "3007": {"name": "AT80", "type": "keyboard", "image": "at80.png", "capabilities": list(_KEYBOARD_CAPS)},
    "3008": {"name": "N2", "type": "keyboard", "image": "", "capabilities": []},
    "3009": {"name": "D1 pro", "type": "keyboard", "image": "", "capabilities": []},
    "3010": {"name": "D1Lite", "type": "keyboard", "image": "d1_lite.png", "capabilities": []},
    "3011": {"name": "N3", "type": "keyboard", "image": "n3_spark.png", "capabilities": []},
    "3012": {"name": "K1", "type": "keyboard", "image": "k1.png", "capabilities": []},
    "3013": {"name": "N8", "type": "keyboard", "image": "", "capabilities": []},
    "3014": {"name": "T9", "type": "keyboard", "image": "", "capabilities": []},
    "3015": {"name": "N3 Spark", "type": "keyboard", "image": "", "capabilities": []},
    "3016": {"name": "T8Lite", "type": "keyboard", "image": "", "capabilities": []},
    "3017": {"name": "K1", "type": "keyboard", "image": "", "capabilities": []},
    # ---- 麦克风 4xxx ----
    "4000": {"name": "S1", "type": "other", "image": "t8_mic.png", "capabilities": []},
    "4001": {"name": "S2", "type": "other", "image": "s2.png", "capabilities": []},
    "4002": {"name": "S3", "type": "other", "image": "", "capabilities": []},
    # ---- 接收器 5xxx ----
    "5000": {"name": "多联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5001": {"name": "多联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5002": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5003": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5004": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5005": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5006": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5007": {"name": "多联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5008": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5009": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5010": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5011": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5012": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5013": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5014": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5015": {"name": "单联接收器", "type": "dongle", "image": "", "capabilities": []},
    "5016": {"name": "dongle-mic", "type": "dongle", "image": "", "capabilities": []},
    "5017": {"name": "2.4G接收器", "type": "dongle", "image": "", "capabilities": []},
    # ---- 自研 6xxx ----
    "6000": {"name": "显示器挂灯", "type": "other", "image": "", "capabilities": []},
    "6500": {"name": "AI-Hub", "type": "other", "image": "", "capabilities": []},
    # ---- 定制 9xxx ----
    "9000": {"name": "够讯接收器", "type": "other", "image": "", "capabilities": []},
    "9001": {"name": "M110", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "9002": {"name": "FS spark", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "9003": {"name": "LM20", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "9004": {"name": "LM10", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "9005": {"name": "LM20", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "9006": {"name": "LM10", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "9007": {"name": "X1", "type": "other", "image": "", "capabilities": []},
    "9008": {"name": "LM10", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "9009": {"name": "LM20", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "9010": {"name": "thinkplus AI X1", "type": "mouse", "image": "x1.png", "capabilities": list(_MOUSE_CAPS)},
    "9011": {"name": "LM20", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
    "9012": {"name": "M320", "type": "mouse", "image": "", "capabilities": list(_MOUSE_CAPS)},
}

# 支持 RGB 灯光的鼠标 product_no 集合（G50 / AM40 / AM50 系列 / AM60）
_LIGHT_MOUSE_PRODUCT_NOS = {
    "1011",  # G50
    "1018",  # FeiShu-S
    "1023", "1028", "1031", "1032", "1043", "1044", "1045", "1047",  # AM50 系列
    "1042",  # AM60
    "1049",  # AM40（另一 SKU）
}


def supports_light(product_no_str: str) -> bool:
    """判断该鼠标是否支持 RGB 灯光设置。"""
    return product_no_str in _LIGHT_MOUSE_PRODUCT_NOS


def get_device_config(product_no_str: str) -> Optional[Dict]:
    """按 product_no（4 位字符串）返回设备配置；未知返回 None。"""
    return _DEVICES.get(product_no_str)


def infer_device_type(product_no_str: str) -> str:
    """按 product_no 首位推断设备类型。

    规则（对齐马道产品编码体系）：
        1xxx = mouse      鼠标
        3xxx = keyboard   键盘
        4xxx = other      麦克风
        5xxx = dongle     接收器
        6xxx = other      自研产品
        9xxx = 按配置     定制系列（可能鼠标/键盘/其他）
    其他前缀默认 other。

    返回 "mouse" / "keyboard" / "dongle" / "other"。
    """
    if not product_no_str or not product_no_str[0].isdigit():
        return "other"
    prefix = product_no_str[0]
    if prefix == "1":
        return "mouse"
    if prefix == "3":
        return "keyboard"
    if prefix == "5":
        return "dongle"
    # 9xxx 先查配置，配置缺失才回退
    cfg = _DEVICES.get(product_no_str)
    if cfg:
        return cfg.get("type", "other")
    return "other"
