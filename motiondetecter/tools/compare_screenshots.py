#!/usr/bin/env python3
"""Create review artifacts, never equate a pixel score with UI acceptance.

Requires Pillow. Run with the bundled Python described in the stage-one README.
"""
import argparse
import json
import os
from pathlib import Path
from PIL import Image, ImageChops, ImageEnhance, ImageDraw, ImageStat

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--reference', type=Path, default=ROOT / 'assets/reference')
parser.add_argument('--current', type=Path, default=ROOT / 'docs/verification/ui-stage1/screenshots')
parser.add_argument('--out', type=Path, default=ROOT / 'docs/verification/ui-stage1/comparison')
args = parser.parse_args()
args.out.mkdir(parents=True, exist_ok=True)
results = []
contact = Image.new('RGB', (1440, 660), '#17171b')
for index in range(1, 7):
    ref = Image.open(args.reference / f'page-{index}.png').convert('RGB')
    cur = Image.open(args.current / f'page-{index}.png').convert('RGB')
    if ref.size != (1440, 900) or cur.size != ref.size:
        raise ValueError(f'Page {index}: invalid comparison dimensions')
    Image.blend(ref, cur, .5).save(args.out / f'page-{index}-overlay.png')
    diff = ImageChops.difference(ref, cur)
    ImageEnhance.Contrast(diff).enhance(3).save(args.out / f'page-{index}-diff.png')
    results.append({'page': index, 'mean_absolute_channel_error': round(sum(ImageStat.Stat(diff).mean) / 3, 3),
                    'note': 'Diagnostic only; changed semantics, sample values and video content are not matched.'})
    thumb = cur.resize((480, 300), Image.Resampling.LANCZOS)
    x, y = (index-1) % 3 * 480, (index-1) // 3 * 330
    contact.paste(thumb, (x, y + 30))
    ImageDraw.Draw(contact).text((x+12, y+9), f'P{index} / Qt Widgets / UI SAMPLE', fill='white')
contact.crop((0, 0, 1440, 660)).save(args.out.parent / 'six-pages.png')
(args.out / 'diagnostics.json').write_text(json.dumps(results, ensure_ascii=False, indent=2))
sections=[]
for i in range(1,7):
    ref_path=os.path.relpath(args.reference / f"page-{i}.png", args.out)
    cur_path=os.path.relpath(args.current / f"page-{i}.png", args.out)
    sections.append(f'''<section><h2>P{i}</h2><div class="compare"><img src="{ref_path}" alt="reference"><img class="current" src="{cur_path}" alt="current UI"></div><p><label>界面透明度 <input aria-label="P{i}界面透明度" type="range" min="0" max="100" value="50" oninput="this.closest('section').querySelector('.current').style.opacity=this.value/100"></label> · <a href="page-{i}-overlay.png">50%叠图</a> · <a href="page-{i}-diff.png">差异图</a></p></section>''')
html='''<!doctype html><html lang="zh-CN"><meta charset="utf-8"><title>PoseLab UI视觉复核</title><style>body{background:#0a0a0b;color:#eee;font:14px -apple-system,sans-serif;margin:30px auto;max-width:1440px}h1,h2{color:#ff8800}section{margin:32px 0}.compare{position:relative;aspect-ratio:1.6;background:black}.compare img{width:100%;height:100%;position:absolute;inset:0}.current{opacity:.5}a{color:#00e5ff}input{width:240px;vertical-align:middle}</style><h1>PoseLab · 六页视觉复核</h1><p>底层为设计稿，上层为当前 Qt 截图。拖动透明度检查布局。文案、有效值、二维码占位及真实视频相关差异已明确；像素误差仅作排查工具，不是“还原度百分比”。</p>'''+''.join(sections)+'</html>'
# From docs/verification/ui-stage1/comparison, the project root is four levels up.
html=html.replace('../../../../../assets/', '../../../../assets/')
(args.out / 'index.html').write_text(html)
print('Created six-page contact sheet, 6 overlays, 6 diffs and comparison viewer:', args.out)
