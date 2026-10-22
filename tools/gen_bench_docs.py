#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
生成不同规模的 Markdown 性能测试文档。

内容模拟真实技术文档：标题层级、中文段落、列表、代码块、表格、引用、链接。
用固定随机种子，保证每次生成结果一致，便于反复对比。

用法：gen_bench_docs.py <输出目录>
"""
import os
import random
import sys

OUT = sys.argv[1] if len(sys.argv) > 1 else 'benchdata'
os.makedirs(OUT, exist_ok=True)

random.seed(20261021)

# ---- 素材池：真实技术文档里会出现的词 ----
TOPICS = ['数据结构', '内存分配', '线程调度', '缓存一致性', '索引优化', '序列化协议',
          '垃圾回收', '锁竞争', '异步回调', '连接池', '限流算法', '一致性哈希',
          '布隆过滤器', '日志切割', '热更新', '灰度发布', '链路追踪', '熔断降级']
VERBS = ['实现', '优化', '分析', '重构', '验证', '压测', '监控', '排查', '定位', '规避']
NOUNS = ['请求', '响应', '事务', '快照', '索引', '副本', '分片', '队列', '缓冲区', '句柄']
ADJS = ['高效的', '稳定的', '可观测的', '幂等的', '可重入的', '无锁的', '批量的', '增量的']

LANGS = ['cpp', 'python', 'sql', 'javascript', 'rust']
CODE_SNIPPETS = {
    'cpp': [
        'std::vector<int> v;',
        'v.reserve(n);',
        'for (int i = 0; i < n; ++i) { v.push_back(i * i); }',
        'auto it = std::find(v.begin(), v.end(), target);',
        'std::sort(v.begin(), v.end());',
        'if (it != v.end()) { return *it; }',
    ],
    'python': [
        'def process(items):',
        '    result = []',
        '    for item in items:',
        '        if item is None:',
        '            continue',
        '        result.append(item.strip())',
        '    return result',
    ],
    'sql': [
        'SELECT id, name, created_at',
        'FROM users',
        'WHERE created_at > NOW() - INTERVAL 7 DAY',
        'ORDER BY created_at DESC',
        'LIMIT 100;',
    ],
    'javascript': [
        'const handler = async (req, res) => {',
        '  const data = await fetchData(req.query.id);',
        '  if (!data) return res.status(404).end();',
        '  res.json({ ok: true, data });',
        '};',
    ],
    'rust': [
        'let mut map: HashMap<String, usize> = HashMap::new();',
        'for (k, v) in pairs.iter() {',
        '    *map.entry(k.clone()).or_insert(0) += v;',
        '}',
    ],
}


def sentence():
    return '%s%s%s%s了%s的%s，%s%s%s。' % (
        random.choice(ADJS), random.choice(TOPICS), random.choice(VERBS),
        random.choice(NOUNS), random.choice(ADJS), random.choice(NOUNS),
        random.choice(VERBS), random.choice(TOPICS), random.choice(NOUNS))


def paragraph(n=4):
    return ''.join(sentence() for _ in range(n))


def gen_section(sec_no, sub_count):
    """生成一节内容"""
    lines = []
    lines.append('## %d. %s与%s的实践' % (sec_no, random.choice(TOPICS), random.choice(TOPICS)))
    lines.append('')
    lines.append(paragraph(random.randint(3, 6)))
    lines.append('')

    for s in range(sub_count):
        kind = random.choice(['para', 'list', 'code', 'table', 'quote', 'mixed'])
        lines.append('### %d.%d %s' % (sec_no, s + 1, random.choice(TOPICS)))
        lines.append('')

        if kind in ('para', 'mixed'):
            for _ in range(random.randint(2, 4)):
                lines.append(paragraph(random.randint(3, 6)))
                lines.append('')

        if kind in ('list', 'mixed'):
            lines.append('要点如下：')
            lines.append('')
            for i in range(random.randint(3, 6)):
                lines.append('- **%s**：%s' % (random.choice(TOPICS), sentence()))
            lines.append('')
            if random.random() < 0.5:
                for i in range(random.randint(2, 3)):
                    lines.append('%d. %s' % (i + 1, sentence()))
                lines.append('')

        if kind in ('code', 'mixed'):
            lang = random.choice(LANGS)
            lines.append('```%s' % lang)
            snippet = CODE_SNIPPETS[lang]
            for _ in range(random.randint(1, 3)):
                lines.extend(snippet)
            lines.append('```')
            lines.append('')

        if kind in ('table', 'mixed'):
            cols = random.randint(3, 4)
            lines.append('| ' + ' | '.join(['项目'] + [random.choice(NOUNS) for _ in range(cols - 1)]) + ' |')
            lines.append('|' + '---|' * cols)
            for _ in range(random.randint(2, 4)):
                lines.append('| ' + ' | '.join([random.choice(TOPICS)] +
                                              [str(random.randint(1, 999)) for _ in range(cols - 1)]) + ' |')
            lines.append('')

        if kind == 'quote':
            lines.append('> %s' % sentence())
            lines.append('>')
            lines.append('> —— %s' % random.choice(TOPICS))
            lines.append('')

    return lines


def build(target_bytes, title):
    lines = ['# %s' % title, '']
    lines.append(paragraph(5))
    lines.append('')
    lines.append('[TOC]')
    lines.append('')

    sec = 1
    while len('\n'.join(lines).encode('utf-8')) < target_bytes:
        lines.extend(gen_section(sec, random.randint(2, 4)))
        sec += 1
        # 每 20 节插一个分割线
        if sec % 20 == 0:
            lines.append('---')
            lines.append('')

    return '\n'.join(lines)


SPECS = [
    ('bench-200k.md', 200 * 1024, '性能测试文档（小型 200KB）'),
    ('bench-1m.md', 1024 * 1024, '性能测试文档（中型 1MB）'),
    ('bench-5m.md', 5 * 1024 * 1024, '性能测试文档（大型 5MB）'),
]

for fname, size, title in SPECS:
    content = build(size, title)
    path = os.path.join(OUT, fname)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)
    raw = content.encode('utf-8')
    print('%-16s %8.2f MB  %8d 行  %9d 字符' % (
        fname, len(raw) / 1024 / 1024, content.count('\n') + 1, len(content)))
