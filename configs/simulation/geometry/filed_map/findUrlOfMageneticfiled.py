#!/usr/bin/env python3
import sys, requests
from bs4 import BeautifulSoup
from urllib.parse import urljoin

url = "https://ribf.riken.jp/SAMURAI/index.php?Magnet"
html = requests.get(url, timeout=30).text
soup = BeautifulSoup(html, "html.parser")

seen = set()
for a in soup.select("a[href]"):
    href = a.get("href", "").strip()
    if not href:
        continue
    full = urljoin(url, href)
    if full not in seen:
        seen.add(full)
        sys.stdout.write(full + "\n")

