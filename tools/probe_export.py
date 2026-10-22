#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
探测 Qt 原生支持的文档导出能力。

用于确定哪些格式可以交给 QTextDocumentWriter，
哪些必须自己实现（docx/epub 需要 zip，rtf/latex 需要文本转换）。
"""
import io

p = 'src/main.cpp'
s = io.open(p, encoding='utf-8').read()

PROBE = '''

    // ---- 导出能力探测（临时） ----
    if (parser.isSet(QStringLiteral("probe-export"))) {
        QTextStream s(stdout);
        s.setEncoding(QStringConverter::Utf8);
        s << "QTextDocumentWriter formats:";
        for (const QByteArray &f : QTextDocumentWriter::supportedDocumentFormats()) {
            s << " [" << QString::fromLatin1(f) << "]";
        }
        s << "\\n";
        s.flush();
        return 0;
    }
'''

# 插到 QApplication 构造之后即可（parser 已存在的位置）
anchor = '''    // ---- 正常 GUI 模式 ----'''
if 'probe-export' not in s:
    s = s.replace(anchor, PROBE + '\n' + anchor, 1)
    io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
    print('  已插入探测代码')
else:
    print('  已存在')
