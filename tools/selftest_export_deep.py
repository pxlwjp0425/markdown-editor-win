#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
导出格式的深度校验（补充 selftest_export.py）。

重点：
  1. EPUB：manifest 声明的每个文件必须真实存在；spine 引用的 id 必须在 manifest 里；
     nav 文档必须带 epub:type="toc"
  2. PDF：必须内嵌中文字体（否则在没装字体的机器上会变方框）
  3. DOCX：styles.xml 里被引用的 styleId 必须已定义
  4. 各文本格式的中文内容保真度（不应出现乱码或丢字）

用法：selftest_export_deep.py <导出目录> <源 Markdown>
"""
import os
import re
import sys
import zipfile
import xml.etree.ElementTree as ET

OUT = sys.argv[1] if len(sys.argv) > 1 else 'dist/out-test'
SRC = sys.argv[2] if len(sys.argv) > 2 else ''

results = []


def check(name, ok, detail=''):
    results.append((name, ok, detail))
    print('  %-6s %-20s %s' % ('PASS' if ok else 'FAIL', name, detail))


def cjk_ratio(text):
    """返回中文字符占非空白字符的比例"""
    stripped = [c for c in text if not c.isspace()]
    if not stripped:
        return 0.0
    cjk = sum(1 for c in stripped if '\u4e00' <= c <= '\u9fa5')
    return cjk / len(stripped)


# ===========================================================================
# 1. EPUB 深度校验
# ===========================================================================
epub = os.path.join(OUT, 'export-epub.epub')
if os.path.exists(epub):
    with zipfile.ZipFile(epub) as z:
        names = z.namelist()
        opf = z.read('OEBPS/content.opf').decode('utf-8')
        root = ET.fromstring(opf)
        NS = {
            'opf': 'http://www.idpf.org/2007/opf',
            'dc': 'http://purl.org/dc/elements/1.1/',
        }

        # manifest 里的每个 href 都要真实存在（相对 OEBPS/）
        manifest = root.find('opf:manifest', NS)
        items = manifest.findall('opf:item', NS) if manifest is not None else []
        declared = []
        missing = []
        for it in items:
            href = it.get('href')
            declared.append(href)
            full = 'OEBPS/' + href
            if full not in names:
                missing.append(href)
        check('epub清单完整', not missing,
              '缺失 %s' % missing if missing else '%d 项全部存在' % len(declared))

        # spine 里的 idref 必须在 manifest 里定义
        spine = root.find('opf:spine', NS)
        idrefs = [r.get('idref') for r in spine.findall('opf:itemref', NS)] if spine is not None else []
        item_ids = {it.get('id') for it in items}
        bad = [i for i in idrefs if i not in item_ids]
        check('epub spine一致', not bad and len(idrefs) > 0,
              '非法 idref %s' % bad if bad else '%d 个 itemref' % len(idrefs))

        # nav 文档必须带 epub:type="toc"
        nav = z.read('OEBPS/nav.xhtml').decode('utf-8')
        check('epub nav语义', 'epub:type="toc"' in nav or "epub:type='toc'" in nav)

        # 元数据完整性
        meta = root.find('opf:metadata', NS)
        meta_txt = ET.tostring(meta, encoding='unicode') if meta is not None else ''
        check('epub元数据', 'dc:identifier' in meta_txt and 'dc:title' in meta_txt
              and 'dc:language' in meta_txt)
        check('epub修改时间', 'dcterms:modified' in meta_txt)

        # 正文中文保真
        tx = z.read('OEBPS/text.xhtml').decode('utf-8')
        body_text = re.sub(r'<[^>]+>', '', tx)
        ratio = cjk_ratio(body_text)
        check('epub正文中文', ratio > 0.3, '中文占比 %.0f%%' % (ratio * 100))
else:
    check('epub', False, '文件不存在')

# ===========================================================================
# 2. PDF 中文字体
# ===========================================================================
pdf = os.path.join(OUT, 'export-pdf.pdf')
if os.path.exists(pdf):
    with open(pdf, 'rb') as f:
        raw = f.read()
    # 字体嵌入：查找 FontFile / FontFile2 / FontFile3
    embeds = raw.count(b'/FontFile')
    check('pdf字体嵌入', embeds > 0, '%d 处字体流' % embeds)
    # 中文 CID 字体通常带 Identity-H 编码
    check('pdf CID编码', b'Identity-H' in raw or b'/CIDFontType' in raw,
          '含 CID 字体')
    # ToUnicode 映射（保证可复制文字）
    check('pdf ToUnicode', b'/ToUnicode' in raw)
    # 侧边栏书签（/Outlines）：源文档带 ATX 标题时应当存在
    check('pdf侧边栏书签', b'/Type /Outlines' in raw, '/Outlines 大纲')
    # xref 完整性：startxref 指向 xref，且所有在用对象偏移都指向自身的 "N 0 obj"
    ok_xref = True
    try:
        sx = int(re.search(rb'startxref\s+(\d+)', raw[raw.rfind(b'startxref'):]).group(1))
        if raw[sx:sx + 4] != b'xref':
            ok_xref = False
        else:
            head = re.match(rb'xref\r\n0 (\d+)\r\n', raw[sx:])
            total = int(head.group(1))
            pos = sx + head.end()
            for n in range(total):
                entry = raw[pos:pos + 20]
                pos += 20
                off = int(entry[0:10])
                typ = entry[17:18]
                if typ == b'n' and not raw[off:off + 40].lstrip().startswith(
                        str(n).encode() + b' 0 obj'):
                    ok_xref = False
                    break
    except Exception:
        ok_xref = False
    check('pdf xref完整', ok_xref, 'xref 与对象偏移一致')
    print('         （PDF 体积 %d KB）' % (len(raw) // 1024))
else:
    check('pdf', False, '文件不存在')

# ===========================================================================
# 3. DOCX 样式引用完整性
# ===========================================================================
docx = os.path.join(OUT, 'export-docx.docx')
if os.path.exists(docx):
    with zipfile.ZipFile(docx) as z:
        styles = z.read('word/styles.xml').decode('utf-8')
        docxml = z.read('word/document.xml').decode('utf-8')
        defined = set(re.findall(r'w:styleId="([^"]+)"', styles))
        used_p = set(re.findall(r'<w:pStyle w:val="([^"]+)"', docxml))
        used_r = set(re.findall(r'<w:rStyle w:val="([^"]+)"', docxml))
        used = used_p | used_r
        undefined = used - defined
        check('docx样式定义', not undefined,
              '未定义 %s' % undefined if undefined else '用到 %d 个样式，全部已定义' % len(used))

        # outlineLvl 必须存在（决定导航窗格能否用）
        check('docx大纲级别', 'outlineLvl' in styles)
        # 表格必须有 tblGrid（Word 要求）
        if '<w:tbl>' in docxml:
            check('docx表格网格', '<w:tblGrid>' in docxml)
else:
    check('docx', False, '文件不存在')

# ===========================================================================
# 4. 中文保真度（各文本格式）
# ===========================================================================
if SRC and os.path.exists(SRC):
    with open(SRC, 'r', encoding='utf-8') as f:
        src_text = f.read()
    src_cjk = sum(1 for c in src_text if '\u4e00' <= c <= '\u9fa5')

    for name, fname in [
        ('markdown', 'export-markdown.md'),
        ('plaintext', 'export-plaintext.txt'),
        ('latex', 'export-latex.tex'),
        ('mediawiki', 'export-mediawiki.wiki'),
        ('rst', 'export-rst.rst'),
        ('textile', 'export-textile.textile'),
    ]:
        p = os.path.join(OUT, fname)
        if not os.path.exists(p):
            continue
        with open(p, 'r', encoding='utf-8', errors='replace') as f:
            t = f.read()
        out_cjk = sum(1 for c in t if '\u4e00' <= c <= '\u9fa5')
        # 允许一定差异（列表符号、编号等非中文内容会增加），
        # 但不应出现大面积丢字
        if src_cjk > 0:
            r = out_cjk / src_cjk
            check('中文保真_%s' % name, r >= 0.85,
                  '%d/%d = %.0f%%' % (out_cjk, src_cjk, r * 100))

    # ---- OPML 是大纲格式，只保留标题层级，所以按「标题文字」校验 ----
    p = os.path.join(OUT, 'export-opml.opml')
    if os.path.exists(p):
        with open(p, 'r', encoding='utf-8', errors='replace') as f:
            opml_text = f.read()
        # 提取源文档所有标题
        headings = re.findall(r'^#{1,6}\s+(.+)$', src_text, re.MULTILINE)
        missing_h = []
        for h in headings:
            # 去掉标题里的行内标记
            plain_h = re.sub(r'\*\*|~~|==|`|\$', '', h).strip()
            if plain_h and plain_h not in opml_text:
                missing_h.append(plain_h)
        check('opml标题完整', not missing_h,
              '缺 %s' % missing_h[:3] if missing_h
              else '%d 个标题全部保留' % len(headings))
        # 大纲必须有缩进层级（嵌套 outline）
        check('opml层级结构', opml_text.count('<outline') > len(headings) * 0.5,
              '%d 个 outline 节点' % opml_text.count('<outline'))

    # RTF 是转义形式，单独查 \u 数量
    p = os.path.join(OUT, 'export-rtf.rtf')
    if os.path.exists(p):
        with open(p, 'r', encoding='utf-8', errors='replace') as f:
            t = f.read()
        ucount = t.count('\\u')
        check('中文保真_rtf', ucount >= src_cjk * 0.9,
              '%d 个 \\u 转义 / 源 %d 个汉字' % (ucount, src_cjk))

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
print('全部 %d 项深度检查通过' % len(results))
sys.exit(0)
