#!/usr/bin/env python3
"""Regenerate the six 1440px design references using Poppler (pdftoppm)."""
import subprocess
from pathlib import Path
root = Path(__file__).resolve().parents[2]
out = root / 'assets/reference'
out.mkdir(parents=True, exist_ok=True)
subprocess.run(['pdftoppm', '-scale-to', '1440', '-png',
                str(root / '动作捕捉检测系统 - 全页面设计稿.pdf'), str(out / 'page')], check=True)
