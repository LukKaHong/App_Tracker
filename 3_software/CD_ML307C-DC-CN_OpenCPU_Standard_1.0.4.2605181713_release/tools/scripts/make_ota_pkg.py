# -*- coding: utf-8 -*-
# ===================================================================
# APP 整包 OTA 升级包自动生成（项目定制）
# 依据：ML307C_固件升级开发指导手册 4.1.1.3（APP 差分包/整包制作）
#       tools/fota_tool/升级包制作方法及模组FOTA升级说明.txt 第 2 节
# 流程：out/image/ML307C_APP.bin → fota_tool/a/user_app.bin
#       → FBFMake_CF_V1.6-150.exe 封装 → out/image/{型号}_V{版本}_{编译时间}.bin
#       → 计算 SHA-256/大小 → out/image/ota_pkg_info.txt
# 说明：a/system.img 与 b/system.img 为 SDK 基线，脚本不改动；
#       最终升级包名 {APP_MODEL_CODE}_V{APP版本}_{YYYYMMDD_HHMM}.bin
#       （版本/型号取自 app_config.h，编译时间由 SConscript 传入，缺省取脚本运行时刻），
#       fw_size/fw_checksum 平台属性取值以该升级包为准
# 手动执行：python tools/scripts/make_ota_pkg.py --image-dir out/image
# ===================================================================
import argparse
import datetime
import hashlib
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
FOTA_TOOL = os.path.join(ROOT, 'tools', 'fota_tool')
FBFMAKE = os.path.join(FOTA_TOOL, 'FBFMake_CF_V1.6-150.exe')
APP_BIN_NAME = 'ML307C_APP.bin'
PATCH_NAME = 'system_patch.bin'   # FBFMake 输出的中间产物名
APP_CFG = os.path.join(ROOT, 'custom', 'custom_main', 'inc', 'app_config.h')


def _fw_version():
    """从 app_config.h 提取 APP_FIRMWARE_VERSION，读不到返回 None。
    显式 utf-8 + errors='ignore'：兼容文件被另存为 GBK 的情况（版本行是纯 ASCII）"""
    try:
        with open(APP_CFG, encoding='utf-8', errors='ignore') as fp:
            m = re.search(r'#define\s+APP_FIRMWARE_VERSION\s+"([^"]+)"',
                          fp.read())
        return m.group(1) if m else None
    except OSError:
        return None


def _model_code():
    """从 app_config.h 提取 APP_MODEL_CODE，读不到返回 None。"""
    try:
        with open(APP_CFG, encoding='utf-8', errors='ignore') as fp:
            m = re.search(r'#define\s+APP_MODEL_CODE\s+"([^"]+)"', fp.read())
        return m.group(1) if m else None
    except OSError:
        return None


def _clean_dir(path):
    if not os.path.isdir(path):
        return
    for name in os.listdir(path):
        full = os.path.join(path, name)
        if os.path.isfile(full):
            os.remove(full)
        else:
            shutil.rmtree(full, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--image-dir', required=True, help='编译产物目录 out/image')
    parser.add_argument('--build-ts', default=None,
                        help='编译时间标签 YYYYMMDD_HHMM（SConscript 传入，'
                             '与 scons 声明的目标名保持一致，缺省取脚本运行时刻）')
    args = parser.parse_args()
    image_dir = os.path.abspath(args.image_dir)

    src_bin = os.path.join(image_dir, APP_BIN_NAME)
    if not os.path.isfile(src_bin):
        print('[ota-pkg] ERROR: %s 不存在，请先完成 scons 编译' % src_bin)
        return 1
    if not os.path.isfile(FBFMAKE):
        print('[ota-pkg] ERROR: %s 不存在' % FBFMAKE)
        return 1

    # 1. 编译产物覆盖 fota_tool/a/user_app.bin（基线 system.img 不动）
    shutil.copyfile(src_bin, os.path.join(FOTA_TOOL, 'a', 'user_app.bin'))

    # 2. 清理上次打包中间产物
    _clean_dir(os.path.join(FOTA_TOOL, 'a', 'patchfolder'))

    # 3. FBFMake 封装 APP 整包（config_app 入口 0x002D7000，a/b 同源基线）
    old_cwd = os.getcwd()
    os.chdir(FOTA_TOOL)
    try:
        if os.path.isfile(PATCH_NAME):
            os.remove(PATCH_NAME)
        rc = subprocess.call([FBFMAKE, '-o', PATCH_NAME,
                              '-f', 'config_app', '-a', 'a', '-b', 'a'])
        patch_bin = os.path.join(FOTA_TOOL, PATCH_NAME)
        if rc != 0 or not os.path.isfile(patch_bin):
            print('[ota-pkg] ERROR: FBFMake 执行失败 (rc=%d)' % rc)
            return 2
    finally:
        os.chdir(old_cwd)

    # 4. 升级包以"型号+版本+编译时间"命名拷回 out/image，生成平台属性取值信息
    #    （包名规则与 SConscript 目标声明保持一致：型号缺失时省略型号段）
    build_ts = args.build_ts or datetime.datetime.now().strftime('%Y%m%d_%H%M')
    ver = _fw_version()
    model = _model_code()
    if ver:
        if model:
            final_name = '%s_V%s_%s.bin' % (model, ver, build_ts)
        else:
            print('[ota-pkg] WARNING: 未从 app_config.h 读到 APP_MODEL_CODE，'
                  '包名不含型号段')
            final_name = 'V%s_%s.bin' % (ver, build_ts)
    else:
        print('[ota-pkg] WARNING: 未从 app_config.h 读到 APP_FIRMWARE_VERSION，'
              '使用默认名 ota_pkg.bin')
        final_name = 'ota_pkg.bin'
    dst_bin = os.path.join(image_dir, final_name)
    shutil.copyfile(patch_bin, dst_bin)
    size = os.path.getsize(dst_bin)
    digest = hashlib.sha256()
    with open(dst_bin, 'rb') as fp:
        for chunk in iter(lambda: fp.read(65536), b''):
            digest.update(chunk)
    info_path = os.path.join(image_dir, 'ota_pkg_info.txt')
    with open(info_path, 'w') as fp:
        fp.write('fw_file = %s\n' % final_name)
        fp.write('fw_title = %s\n' % (model if model else 'unknown'))
        fp.write('fw_version = %s\n' % (ver if ver else 'unknown'))
        fp.write('fw_size = %d\n' % size)
        fp.write('fw_checksum = %s\n' % digest.hexdigest())
        fp.write('fw_checksum_algorithm = SHA256\n')

    print('[ota-pkg] %s (%d bytes) sha256=%s' % (dst_bin, size, digest.hexdigest()))
    print('[ota-pkg] 平台属性取值 -> %s' % info_path)
    return 0


if __name__ == '__main__':
    sys.exit(main())
