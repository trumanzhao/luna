#!/usr/bin/python
# -*- coding: utf-8 -*-
import io, os, shutil, sys
import chardet
import codecs
import datetime

#注意需要安装chardet模块

today = datetime.datetime.now().strftime("%Y-%m-%d");

#检测文件编码,如果不是的话,统一改为utf-8
#将table转换为space
def recode(path):
    raw = open(path, 'rb').read();
    if raw.startswith(codecs.BOM_UTF8):
        encoding = 'utf-8-sig'
    else:
        result = chardet.detect(raw)
        encoding = result['encoding']

    lines = io.open(path, "r", encoding=encoding).readlines();
    for i in range(0, len(lines)):
        lines[i] = lines[i].rstrip().expandtabs(4) + "\n";
    io.open(path, "w", encoding="utf-8-sig").writelines(lines);

sign = list();
sign.append(u"/*");
sign.append(u"** repository: https://github.com/trumanzhao/luna");
sign.append(u"** trumanzhao, %s, trumanzhao@foxmail.com" % today);
sign.append(u"*/");
sign.append(u"");

def signed(lines):
    if len(lines) < len(sign):
        return False;
    if lines[0].rstrip() == sign[0] and lines[1].rstrip() == sign[1]:
        return True;
    return False;

def sign_file(path):
    recode(path);

    lines = io.open(path, "r", encoding="utf-8-sig").readlines();
    if signed(lines):
        print("%s 已签名!" % path);
        return;

    for i in range(0, len(sign)):
        lines.insert(i, sign[i] + u"\n");

    print("签名: %s" % path);
    io.open(path, "w", encoding="utf-8").writelines(lines);

root = ".";
items = os.listdir(root);
for item in items:
    path = os.path.join(root, item);
    ext = os.path.splitext(path)[1].lower();
    if ext == ".cpp" or ext == ".h":
        sign_file(path);
