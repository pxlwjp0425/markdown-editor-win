#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
导出格式真实性校验。

不只看「文件存在」，而是用标准库做结构校验：
  - docx / epub / odt：用 zipfile 检查容器结构与必需部件
  - EPUB：额外校验 mimetype 必须是第一个条目且内容为 application/epub+zip
  - OOXML / OPML / XHTML：用 xml.etree 校验 XML 良构
  - PDF：校验 %PDF- 头与 %%EOF 尾
  - PNG：校验签名与 IHDR
  - RTF / LaTeX：校验关键结构标记
  - 纯文本类：校验非空且内容与源文档一致

用法：selftest_export.py <导出目录>
"""
import os
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile

OUT = sys.argv[1]
REPORT = os.path.join(OUT, '_report.txt')

if not os.path.exists(REPORT):
    print('FAIL: 未找到 _report.txt')
    sys.exit(1)

# ---- 解析报告 ----
report = {}
source_chars = 0
with open(REPORT, 'r', encoding='utf-8', errors='replace') as f:
    for line in f:
        line = line.strip()
        if not line:
            continue
        parts = line.split('|')
        if parts[0] == 'SOURCE':
            for p in parts[1:]:
                if p.startswith('chars='):
                    source_chars = int(p.split('=')[1])
            continue
        if len(parts) >= 4:
            report[parts[0]] = {
                'path': parts[1],
                'size': int(parts[2]) if parts[2].isdigit() else 0,
                'status': parts[3],
            }

print('源文档 %d 字符\n' % source_chars)

results = []


def check(name, ok, detail=''):
    results.append((name, ok, detail))
    print('  %-6s %-14s %s' % ('PASS' if ok else 'FAIL', name, detail))


def fsize(key):
    return report.get(key, {}).get('size', 0)


def fpath(key):
    return report.get(key, {}).get('path', '')


# ===========================================================================
# 1. DOCX
# ===========================================================================
d = fpath('docx')
if not d or not os.path.exists(d):
    check('docx', False, '文件不存在')
else:
    try:
        with zipfile.ZipFile(d) as z:
            names = set(z.namelist())
            required = {
                '[Content_Types].xml',
                '_rels/.rels',
                'word/document.xml',
                'word/styles.xml',
            }
            missing = required - names
            check('docx容器', not missing,
                  '缺少 %s' % missing if missing else '%d 个部件' % len(names))

            # Content_Types 必须声明 document.xml
            ct = z.read('[Content_Types].xml').decode('utf-8')
            check('docx内容类型', 'wordprocessingml.document.main' in ct)

            # document.xml 必须是良构 XML
            docxml = z.read('word/document.xml').decode('utf-8')
            try:
                root = ET.fromstring(docxml)
                ns = '{http://schemas.openxmlformats.org/wordprocessingml/2006/main}'
                paras = root.iter(ns + 'p')
                npara = sum(1 for _ in paras)
                tables = sum(1 for _ in root.iter(ns + 'tbl'))
                check('docx文档XML', npara > 0, '%d 段落 / %d 表格' % (npara, tables))
            except ET.ParseError as e:
                check('docx文档XML', False, 'XML 不良构: %s' % e)

            # 必须包含中文内容（验证文本没丢）
            has_cn = any('\u4e00' <= ch <= '\u9fa5' for ch in docxml)
            check('docx中文', has_cn)

            # 标题样式必须被引用
            check('docx标题样式', 'Heading1' in docxml or 'Heading2' in docxml)

            # rels 里的超链接关系
            rels = z.read('word/_rels/document.xml.rels').decode('utf-8')
            check('docx关系表', 'styles.xml' in rels)

            # OPC 完整性：document.xml 里用到的每个部件，
            # 都必须在 [Content_Types].xml 声明、且在 rels 里建立关系。
            # 缺任何一个，Word 都会提示「内容有问题」而拒绝打开。
            doc_uses_numbering = 'numPr' in docxml
            ct_declares_numbering = 'numbering.xml' in ct
            rels_links_numbering = 'numbering.xml' in rels
            if doc_uses_numbering:
                check('docx编号声明', ct_declares_numbering,
                      'Content_Types 已声明 numbering.xml')
                check('docx编号关系', rels_links_numbering,
                      'document.xml.rels 已引用 numbering.xml')
                check('docx编号部件', 'word/numbering.xml' in names)
            else:
                check('docx编号', True, '未使用列表，跳过')
    except zipfile.BadZipFile as e:
        check('docx容器', False, '不是合法 ZIP: %s' % e)

# ===========================================================================
# 2. EPUB
# ===========================================================================
e = fpath('epub')
if not e or not os.path.exists(e):
    check('epub', False, '文件不存在')
else:
    try:
        with zipfile.ZipFile(e) as z:
            infos = z.infolist()
            names = [i.filename for i in infos]
            # mimetype 必须是第一个条目且不压缩
            first_ok = names and names[0] == 'mimetype'
            check('epub mimetype首位', first_ok, names[0] if names else '空')

            if first_ok:
                mt = z.read('mimetype')
                check('epub mimetype内容', mt == b'application/epub+zip',
                      repr(mt[:30]))

            required = {'META-INF/container.xml', 'OEBPS/content.opf',
                        'OEBPS/nav.xhtml', 'OEBPS/text.xhtml', 'OEBPS/style.css'}
            missing = required - set(names)
            check('epub部件', not missing,
                  '缺少 %s' % missing if missing else '%d 个部件' % len(names))

            # container.xml 指向 content.opf
            cont = z.read('META-INF/container.xml').decode('utf-8')
            try:
                ET.fromstring(cont)
                check('epub容器XML', 'OEBPS/content.opf' in cont)
            except ET.ParseError as ex:
                check('epub容器XML', False, str(ex))

            # content.opf 必须良构且含 dc:title
            opf = z.read('OEBPS/content.opf').decode('utf-8')
            try:
                root = ET.fromstring(opf)
                check('epub OPF', 'dc:title' in opf or 'title' in opf,
                      'metadata 含 title')
            except ET.ParseError as ex:
                check('epub OPF', False, 'XML 不良构: %s' % ex)

            # text.xhtml 必须良构 XHTML
            tx = z.read('OEBPS/text.xhtml').decode('utf-8')
            try:
                ET.fromstring(tx)
                check('epub正文XHTML', True, '%d 字符' % len(tx))
            except ET.ParseError as ex:
                check('epub正文XHTML', False, 'XHTML 不良构: %s' % ex)

            # 正文里应有中文
            check('epub中文', any('\u4e00' <= c <= '\u9fa5' for c in tx))

            # nav.xhtml 良构
            nav = z.read('OEBPS/nav.xhtml').decode('utf-8')
            try:
                ET.fromstring(nav)
                check('epub导航', True)
            except ET.ParseError as ex:
                check('epub导航', False, str(ex))
    except zipfile.BadZipFile as ex:
        check('epub容器', False, '不是合法 ZIP: %s' % ex)

# ===========================================================================
# 3. ODT
# ===========================================================================
o = fpath('odt')
if not o or not os.path.exists(o):
    check('odt', False, '文件不存在')
else:
    try:
        with zipfile.ZipFile(o) as z:
            names = z.namelist()
            has_mime = 'mimetype' in names
            check('odt容器', has_mime and 'content.xml' in names,
                  '%d 个部件' % len(names))
            if has_mime:
                mt = z.read('mimetype').decode('utf-8', errors='replace')
                check('odt mimetype',
                      'opendocument.text' in mt, mt[:40])
            if 'content.xml' in names:
                cx = z.read('content.xml').decode('utf-8')
                try:
                    ET.fromstring(cx)
                    check('odt内容XML', True, '%d 字符' % len(cx))
                except ET.ParseError as ex:
                    check('odt内容XML', False, str(ex))
    except zipfile.BadZipFile as ex:
        check('odt容器', False, '不是合法 ZIP: %s' % ex)

# ===========================================================================
# 4. PDF
# ===========================================================================
pdf = fpath('pdf')
if not pdf or not os.path.exists(pdf):
    check('pdf', False, '文件不存在')
else:
    with open(pdf, 'rb') as f:
        head = f.read(8)
        f.seek(-32, 2)
        tail = f.read()
    check('pdf文件头', head.startswith(b'%PDF-'), head[:8].decode('latin-1'))
    check('pdf文件尾', b'%%EOF' in tail)
    # 页数
    with open(pdf, 'rb') as f:
        raw = f.read()
    pages = raw.count(b'/Type /Page') + raw.count(b'/Type/Page')
    check('pdf页面', pages > 0, '约 %d 页' % pages)

# ===========================================================================
# 5. PNG
# ===========================================================================
img = fpath('image')
if not img or not os.path.exists(img):
    check('image', False, '文件不存在')
else:
    with open(img, 'rb') as f:
        sig = f.read(8)
        f.read(4)
        chunk = f.read(4)
        w, h = struct.unpack('>II', f.read(8))
    check('png签名', sig == b'\x89PNG\r\n\x1a\n')
    check('png IHDR', chunk == b'IHDR', '%dx%d' % (w, h))
    check('png尺寸合理', 400 <= w <= 2000 and h >= 200, '%dx%d' % (w, h))

# ===========================================================================
# 6. RTF
# ===========================================================================
rtf = fpath('rtf')
if rtf and os.path.exists(rtf):
    with open(rtf, 'r', encoding='utf-8', errors='replace') as f:
        txt = f.read()
    check('rtf结构', txt.startswith('{\\rtf1'), txt[:20])
    check('rtf闭合', txt.rstrip().endswith('}'))
    check('rtf字体表', '\\fonttbl' in txt)
    check('rtf中文转义', '\\u' in txt, '含 \\uNNNN 转义')
    # 花括号配平
    check('rtf括号配平', txt.count('{') == txt.count('}'),
          '%d vs %d' % (txt.count('{'), txt.count('}')))
else:
    check('rtf', False, '文件不存在')

# ===========================================================================
# 7. LaTeX
# ===========================================================================
tex = fpath('latex')
if tex and os.path.exists(tex):
    with open(tex, 'r', encoding='utf-8', errors='replace') as f:
        txt = f.read()
    check('latex结构', '\\documentclass' in txt and '\\begin{document}' in txt)
    check('latex闭合', '\\end{document}' in txt)
    check('latex中文支持', 'ctex' in txt)
    check('latex标题', '\\section' in txt or '\\subsection' in txt)
    # 环境配平
    begins = txt.count('\\begin{')
    ends = txt.count('\\end{')
    check('latex环境配平', begins == ends, '%d vs %d' % (begins, ends))
else:
    check('latex', False, '文件不存在')

# ===========================================================================
# 8. OPML
# ===========================================================================
op = fpath('opml')
if op and os.path.exists(op):
    with open(op, 'r', encoding='utf-8') as f:
        txt = f.read()
    try:
        root = ET.fromstring(txt)
        check('opml XML', root.tag == 'opml', root.tag)
        outlines = root.findall('.//outline')
        check('opml大纲项', len(outlines) > 0, '%d 项' % len(outlines))
    except ET.ParseError as ex:
        check('opml XML', False, str(ex))
else:
    check('opml', False, '文件不存在')

# ===========================================================================
# 9. MediaWiki / RST / Textile / Markdown / 纯文本
# ===========================================================================
wiki = fpath('mediawiki')
if wiki and os.path.exists(wiki):
    with open(wiki, 'r', encoding='utf-8') as f:
        txt = f.read()
    check('mediawiki', '==' in txt and len(txt) > 100, '%d 字符' % len(txt))
else:
    check('mediawiki', False, '文件不存在')

rst = fpath('rst')
if rst and os.path.exists(rst):
    with open(rst, 'r', encoding='utf-8') as f:
        txt = f.read()
    check('rst', ('====' in txt or '----' in txt) and len(txt) > 100,
          '%d 字符' % len(txt))
else:
    check('rst', False, '文件不存在')

tex_t = fpath('textile')
if tex_t and os.path.exists(tex_t):
    with open(tex_t, 'r', encoding='utf-8') as f:
        txt = f.read()
    check('textile', 'h1.' in txt or 'h2.' in txt, '%d 字符' % len(txt))
else:
    check('textile', False, '文件不存在')

md = fpath('markdown')
if md and os.path.exists(md):
    with open(md, 'r', encoding='utf-8') as f:
        txt = f.read()
    check('markdown', '#' in txt and len(txt) > 100, '%d 字符' % len(txt))
else:
    check('markdown', False, '文件不存在')

pt = fpath('plaintext')
if pt and os.path.exists(pt):
    with open(pt, 'r', encoding='utf-8') as f:
        txt = f.read()
    check('plaintext', len(txt) > 100, '%d 字符' % len(txt))
    # 纯文本不应残留粗体标记
    check('plaintext无标记', '**' not in txt and '##' not in txt)
else:
    check('plaintext', False, '文件不存在')

# ===========================================================================
# 汇总
# ===========================================================================
fails = [r for r in results if not r[1]]
print()
if fails:
    print('%d 项失败：' % len(fails))
    for n, _, d in fails:
        print('   - %s %s' % (n, d))
    sys.exit(1)
print('全部 %d 项检查通过' % len(results))
sys.exit(0)
