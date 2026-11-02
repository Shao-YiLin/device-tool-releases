# -*- coding: utf-8 -*-
"""一键发布：打包 dist/MadaoDeviceTool → zip + sha256 + 版本号文件。

用法：
    python tools/release.py <版本号>            # 例: python tools/release.py 1.0.0
    python tools/release.py <版本号> --upload   # 需已 gh auth login（GitHub CLI）

流程：
    1. 用 PyInstaller 打包（可选 --no-build 跳过）
    2. 在产物目录写入 version.txt（客户端据此判断本地版本）
    3. 打 zip（资产名 MadaoDeviceTool.zip）+ 生成 .sha256
    4. --upload 时用 gh release create 上传（公开仓库免 token 下载）

产物输出在 release/<版本号>/。
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import urllib.request
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIST = os.path.join(ROOT, "dist", "MadaoDeviceTool")
SPEC = os.path.join(ROOT, "madao_device_tool", "gui", "gui.spec")
OUT = os.path.join(ROOT, "release")
ASSET = "MadaoDeviceTool.zip"
REPO = "Shao-YiLin/device-tool-releases"


def build():
    subprocess.run([sys.executable, "-m", "PyInstaller", SPEC,
                    "--noconfirm", "--distpath", os.path.join(ROOT, "dist"),
                    "--workpath", os.path.join(ROOT, "build")], check=True)


def make_zip(version: str):
    if not os.path.isdir(DIST):
        raise SystemExit(f"产物目录不存在: {DIST}")
    # 版本号文件写入产物目录（exe 同级）
    vf = os.path.join(DIST, "version.txt")
    with open(vf, "w", encoding="utf-8") as f:
        f.write(version + "\n")
    out_dir = os.path.join(OUT, version)
    os.makedirs(out_dir, exist_ok=True)
    zip_path = os.path.join(out_dir, ASSET)
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as z:
        for base, _, files in os.walk(DIST):
            for name in files:
                full = os.path.join(base, name)
                # zip 内含 MadaoDeviceTool/ 顶层目录
                z.write(full, os.path.relpath(full, os.path.dirname(DIST)))
    sha = hashlib.sha256(open(zip_path, "rb").read()).hexdigest()
    sha_path = zip_path + ".sha256"
    with open(sha_path, "w") as f:
        f.write(sha + "\n")
    print(f"zip 已生成: {zip_path}")
    print(f"sha256  : {sha}")
    return zip_path, sha_path


def upload(version: str, zip_path: str, sha_path: str):
    """上传 Release。

    优先用 gh CLI（需 gh auth login）；没有 gh 时用 GITHUB_TOKEN 环境变量
    调 GitHub API（token 需 repo 权限）。
    """
    tag = "v" + version
    if shutil.which("gh"):
        subprocess.run(["gh", "release", "create", tag, zip_path, sha_path,
                        "--repo", REPO, "--title", f"v{version}",
                        "--notes", f"版本 v{version}"], check=True)
        return
    token = os.environ.get("GITHUB_TOKEN")
    if not token:
        raise SystemExit(
            "未找到 gh CLI，且未设置 GITHUB_TOKEN 环境变量。\n"
            "二选一：\n"
            "  1) 安装 GitHub CLI 并执行 gh auth login\n"
            "  2) 设置环境变量 GITHUB_TOKEN=<你的 PAT，权限 repo>\n"
            "或手动上传：GitHub 网页 → Releases → Draft a new release → "
            f"tag={tag} → 拖入 {zip_path} 和 {sha_path}")
    api = f"https://api.github.com/repos/{REPO}/releases"
    headers = {
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github+json",
        "User-Agent": "MadaoDeviceTool-release",
    }
    # 1) 建 release（draft 先不发布，确认无误再 publish）
    body = json.dumps({
        "tag_name": tag,
        "name": f"v{version}",
        "body": f"版本 v{version}",
        "draft": False,
        "prerelease": False,
    }).encode()
    req = urllib.request.Request(api, data=body, headers=headers, method="POST")
    with urllib.request.urlopen(req, timeout=60) as resp:
        rel = json.loads(resp.read().decode())
    upload_url = rel["upload_url"].split("{")[0]
    # 2) 传资产
    for path in (zip_path, sha_path):
        name = os.path.basename(path)
        with open(path, "rb") as f:
            data = f.read()
        h = dict(headers)
        h["Content-Type"] = "application/octet-stream"
        req = urllib.request.Request(f"{upload_url}?name={name}", data=data,
                                     headers=h, method="POST")
        with urllib.request.urlopen(req, timeout=600) as resp:
            print(f"已上传: {name} ({len(data)} 字节)")
    print(f"Release 地址: {rel['html_url']}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("version")
    ap.add_argument("--no-build", action="store_true", help="跳过 PyInstaller 打包")
    ap.add_argument("--upload", action="store_true", help="用 gh CLI 上传 Release")
    args = ap.parse_args()

    if not args.no_build:
        build()
    zip_path, sha_path = make_zip(args.version)
    if args.upload:
        upload(args.version, zip_path, sha_path)
        print(f"已上传到 GitHub Release v{args.version}")
    else:
        print("（未上传。手动上传：GitHub 网页 → Releases → Draft a new release → "
              f"tag=v{args.version} → 拖入 {zip_path} 和 {sha_path}）")


if __name__ == "__main__":
    main()
