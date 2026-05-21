#!/usr/bin/env python3
"""Final: Replace thesis content in document.xml with polished text."""

import re
import xml.dom.minidom as minidom
from collections import OrderedDict

# ============================================================
# Compiled heading list
# ============================================================
# Each entry: (polished_heading_text, original_heading_text, section_key)
# original_heading_text can be same as polished, or different
HEADING_DEFS = [
    # (polished, original, key) - if original is same, use None
    ('摘要', None, '摘要'),
    ('1.1 课题背景', None, '1.1'),
    ('1.2 DPDK框架选型分析', '1.2 为什么选择DPDK框架', '1.2'),
    ('1.2.1 传统网络架构的性能瓶颈', None, '1.2.1'),
    ('1.2.2 DPDK技术优势', None, '1.2.2'),
    ('1.2.3 DPDK与其他高性能数据包处理方案对比', None, '1.2.3'),
    ('1.3 目的和意义', None, '1.3'),
    ('1.3.1 研究目的', None, '1.3.1'),
    ('1.3.2 研究意义', None, '1.3.2'),
    ('1.4 国内外研究现状', None, '1.4'),
    ('1.4.1 国外研究现状', None, '1.4.1'),
    ('1.4.2 国内研究现状', None, '1.4.2'),
    ('1.5 论文主要工作', None, '1.5'),
    ('2.1 DPDK框架原理', None, '2.1'),
    ('2.1.1 DPDK架构概述', None, '2.1.1'),
    ('2.1.2 用户态轮询机制', None, '2.1.2'),
    ('2.1.3 大页内存技术', None, '2.1.3'),
    ('2.1.4 多队列与RSS', None, '2.1.4'),
    ('2.2 防火墙技术原理', None, '2.2'),
    ('2.2.1 防火墙分类', None, '2.2.1'),
    ('2.2.2 状态化防火墙原理', None, '2.2.2'),
    ('2.2.3 ACL规则匹配', None, '2.2.3'),
    ('2.3 关键数据结构与算法', None, '2.3'),
    ('2.3.1 rte_hash哈希表', None, '2.3.1'),
    ('2.3.2 rte_lpm最长前缀匹配', None, '2.3.2'),
    ('2.3.3 rte_ring无锁环形队列', None, '2.3.3'),
    ('2.4 本章小结', None, '2.4'),
    ('3.1 需求分析', None, '3.1'),
    ('3.1.1 功能性需求', None, '3.1.1'),
    ('3.1.2 非功能性需求', None, '3.1.2'),
    ('3.2 系统架构设计', None, '3.2'),
    ('3.2.1 整体架构', None, '3.2.1'),
    ('3.2.2 数据面架构', None, '3.2.2'),
    ('3.2.3 控制面架构', None, '3.2.3'),
    ('3.3 系统模块划分', None, '3.3'),
    ('3.4 本章小结', None, '3.4'),
    ('4.1 数据面主程序设计', None, '4.1'),
    ('4.1.1 EAL初始化与主循环', None, '4.1.1'),
    ('4.1.2 多核处理模型', None, '4.1.2'),
    ('4.2 ACL模块设计与实现', None, '4.2'),
    ('4.3 会话跟踪模块设计与实现', None, '4.3'),
    ('4.4 路由模块设计与实现', None, '4.4'),
    ('4.5 ARP模块设计与实现', None, '4.5'),
    ('4.6 IPv6模块设计与实现', None, '4.6'),
    ('4.7 控制面CLI设计与实现', None, '4.7'),
    ('4.8 进程间通信设计与实现', None, '4.8'),
    ('4.9 Web后端API设计与实现', None, '4.9'),
    ('4.10 本章小结', None, '4.10'),
    ('5.1 测试环境', None, '5.1'),
    ('5.2 功能测试', None, '5.2'),
    ('5.3 性能测试', None, '5.3'),
    ('5.4 测试结果分析', None, '5.4'),
    ('5.5 本章小结', None, '5.5'),
    ('6.1 总结', None, '6.1'),
    ('6.2 存在的不足', None, '6.2'),
    ('6.3 改进方向', None, '6.3'),
    ('致谢', None, '致谢'),
]

# Build dictionaries for quick lookup
POLISHED_TO_KEY = {}
for p, o, k in HEADING_DEFS:
    POLISHED_TO_KEY[p] = k

ORIG_TO_KEY = {}
for p, o, k in HEADING_DEFS:
    orig_text = o if o else p
    ORIG_TO_KEY[orig_text] = k
    # Also add the polished heading text as a valid match
    # (documents may have either version)
    if o and p != o:
        ORIG_TO_KEY[p] = k

# Special boundary headings (not content sections)
BOUNDARY_HEADINGS = {
    'Abstract', '参考文献',
    '第1章 绪论', '第2章 相关技术理论基础',
    '第3章 系统需求分析与总体设计', '第4章 系统详细设计与实现',
    '第5章 系统测试', '第6章 总结与展望',
    'DPDK-Based High-Performance Packet Firewall Design and Implementation',
}

# Items to skip in polished text (figures, tables, chapter dividers)
POLISHED_SKIP = {
    '图3.1 系统整体架构图', '图3.2 数据面数据包处理流水线',
    '图4.1 数据面系统初始化流程图', '图4.2 IPv4快速数据处理路径图',
    '图4.3 IPv6快速数据处理路径图', '图4.4 Go后端设计架构图',
    '表5.1 不同包大小下的吞吐量测试结果',
    '包大小', '64B', '128B', '256B', '512B', '1518B',
    '吞吐量(Mpps)', '1.44', '0.84', '0.45', '0.235', '0.081',
    '线速占比', '97%', '99%', '99%', '100%', '100%',
    '第1章 绪论', '第2章 相关技术理论基础',
    '第3章 系统需求分析与总体设计', '第4章 系统详细设计与实现',
    '第5章 系统测试', '第6章 总结与展望',
}

# Table content to skip in original
SKIP_TABLE = {
    '包大小', '64B', '128B', '256B', '512B', '1518B',
    '吞吐量(Mpps)', '1.44', '0.84', '0.45', '0.235', '0.081',
    '线速占比', '97%', '99%', '99%', '100%', '100%',
    '表5.1 不同包大小下的吞吐量测试结果',
}


# ============================================================
# Parse polished text
# ============================================================
def parse_polished(filepath):
    with open(filepath, 'r', encoding='utf-8') as f:
        text = f.read()

    lines = text.split('\n')
    sections = OrderedDict()
    current_key = None
    current_paras = []
    line_buf = []

    for line in lines:
        stripped = line.strip()

        if stripped.startswith('====') or stripped.startswith('----'):
            continue
        if stripped == '':
            # Flush buffered paragraph before skipping empty line
            if line_buf:
                current_paras.append(''.join(line_buf))
                line_buf = []
            continue
        if stripped.startswith('润色稿'):
            continue
        if stripped in BOUNDARY_HEADINGS:
            continue
        if stripped in POLISHED_SKIP:
            continue

        if stripped in POLISHED_TO_KEY:
            if current_key is not None:
                if line_buf:
                    current_paras.append(''.join(line_buf))
                    line_buf = []
                sections[current_key] = current_paras
            current_key = POLISHED_TO_KEY[stripped]
            current_paras = []
            line_buf = []
            continue

        if not stripped:
            if line_buf:
                current_paras.append(''.join(line_buf))
                line_buf = []
            continue

        if stripped in POLISHED_SKIP:
            continue

        line_buf.append(stripped)

    if current_key is not None:
        if line_buf:
            current_paras.append(''.join(line_buf))
        sections[current_key] = current_paras

    return sections


# ============================================================
# Parse original docx and identify sections
# ============================================================
def parse_original(xml_path):
    with open(xml_path, 'r', encoding='utf-8') as f:
        content = f.read()

    para_re = re.compile(r'<w:p[ >].*?</w:p>', re.DOTALL)
    paras_matches = list(para_re.finditer(content))

    paragraphs = []
    for m in paras_matches:
        xml = m.group(0)
        texts = re.findall(r'<w:t[^>]*>(.*?)</w:t>', xml)
        combined = ''.join(texts).strip()
        paragraphs.append({'xml': xml, 'text': combined})

    # Identify sections
    sections = OrderedDict()
    current_key = None
    current_heading = None
    current_content = []

    for idx, para in enumerate(paragraphs):
        t = para['text']
        if not t:
            continue

        # Check if it's a heading or boundary
        if t in ORIG_TO_KEY or t in BOUNDARY_HEADINGS:
            if current_key is not None:
                sections[current_key] = {
                    'heading': current_heading,
                    'content': list(current_content),
                }
            if t in ORIG_TO_KEY:
                current_key = ORIG_TO_KEY[t]
            else:
                current_key = None  # boundary - end current section, start none
            current_heading = idx
            current_content = []
            continue

        if current_key is not None:
            if t.startswith('图3.') or t.startswith('图4.') or t.startswith('图5.'):
                continue
            if t in SKIP_TABLE:
                continue
            current_content.append(idx)

    if current_key is not None:
        sections[current_key] = {
            'heading': current_heading,
            'content': list(current_content),
        }

    return paragraphs, sections


# ============================================================
# Build mapping
# ============================================================
def build_mapping(polished_sections, orig_sections):
    mapping = {}
    stats = {'ok': 0, 'mismatch': 0, 'skip': 0, 'heading_change': 0}

    for key, p_paras in polished_sections.items():
        if key not in orig_sections:
            print(f"  SKIP: {key} - not in original")
            stats['skip'] += 1
            continue

        o_info = orig_sections[key]
        o_indices = o_info['content']

        if len(p_paras) != len(o_indices):
            print(f"  MISMATCH: {key} - polished:{len(p_paras)} orig:{len(o_indices)}")
            stats['mismatch'] += 1
        else:
            stats['ok'] += 1

        for i in range(min(len(p_paras), len(o_indices))):
            mapping[o_indices[i]] = p_paras[i]

    # Heading text changes
    for p_text, o_text, key in HEADING_DEFS:
        if o_text and o_text != p_text and key in orig_sections:
            mapping[orig_sections[key]['heading']] = p_text
            stats['heading_change'] += 1
            print(f"  HEADING: {key} [{o_text}] -> [{p_text}]")

    return mapping, stats


# ============================================================
# Replace text in XML
# ============================================================
def replace_text_in_xml_file(xml_path, mapping, output_path):
    with open(xml_path, 'r', encoding='utf-8') as f:
        content = f.read()

    para_re = re.compile(r'<w:p[ >].*?</w:p>', re.DOTALL)

    parts = []
    last_end = 0
    para_idx = 0

    for m in para_re.finditer(content):
        parts.append(content[last_end:m.start()])
        xml = m.group(0)

        if para_idx in mapping:
            xml = replace_para_text(xml, mapping[para_idx])

        parts.append(xml)
        last_end = m.end()
        para_idx += 1

    parts.append(content[last_end:])
    result = ''.join(parts)

    dom = minidom.parseString(result)
    pretty = dom.toprettyxml(indent='  ')
    lines = pretty.split('\n')
    fixed = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n' + '\n'.join(lines[1:])

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(fixed)

    print(f"\nWritten: {output_path}")
    print(f"Total paragraphs: {para_idx}, Replaced: {len(mapping)}")


def replace_para_text(para_xml, new_text):
    safe_text = new_text.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')

    first_rpr = ''
    rpr_match = re.search(r'<w:r[ >].*?<w:rPr>(.*?)</w:rPr>.*?</w:r>', para_xml, re.DOTALL)
    if rpr_match:
        first_rpr = f'<w:rPr>{rpr_match.group(1)}</w:rPr>'

    result = re.sub(r'<w:r[ >].*?</w:r>', '', para_xml, flags=re.DOTALL)

    ppr_end = result.find('</w:pPr>')
    if ppr_end == -1:
        p_start = result.find('>')
        insert_pos = p_start + 1 if p_start != -1 else 0
    else:
        insert_pos = ppr_end + len('</w:pPr>')

    if first_rpr:
        new_run = f'<w:r>{first_rpr}<w:t xml:space="preserve">{safe_text}</w:t></w:r>'
    else:
        new_run = f'<w:r><w:t xml:space="preserve">{safe_text}</w:t></w:r>'

    return result[:insert_pos] + new_run + result[insert_pos:]


# ============================================================
# MAIN
# ============================================================
def main():
    polished_path = '/home/yy/DPDK_Packet_Firewall/docs/毕业论文/润色稿_全篇.txt'
    xml_path = '/home/yy/DPDK_Packet_Firewall/docs/毕业论文/unpacked_manual/word/document.xml'
    output_path = '/home/yy/DPDK_Packet_Firewall/docs/毕业论文/unpacked_manual/word/document_new.xml'

    print("Parsing polished text...")
    polished = parse_polished(polished_path)
    total_p = sum(len(v) for v in polished.values())
    print(f"Sections: {len(polished)}, Total content paragraphs: {total_p}")
    for k, v in polished.items():
        if len(v) > 3:
            print(f"  {k}: {len(v)} paras")

    print("\nParsing original...")
    orig_paras, orig_sections = parse_original(xml_path)
    total_o = sum(len(v['content']) for v in orig_sections.values())
    print(f"<w:p> elements: {len(orig_paras)}, Sections: {len(orig_sections)}, Content paras: {total_o}")

    print("\nBuilding mapping...")
    mapping, stats = build_mapping(polished, orig_sections)
    print(f"\nStats: OK={stats['ok']}, MISMATCH={stats['mismatch']}, "
          f"SKIP={stats['skip']}, HEADING_CHANGE={stats['heading_change']}")

    print("\nReplacing text...")
    replace_text_in_xml_file(xml_path, mapping, output_path)


if __name__ == '__main__':
    main()
