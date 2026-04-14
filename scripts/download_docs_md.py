#!/usr/bin/env python3
"""
Baixa documentações da web e converte páginas HTML em Markdown.

Melhorias desta versão:
- baixa imagens e as salva localmente
- reescreve links internos para .md
- gera SUMMARY.md automaticamente
- preserva a estrutura de diretórios
- mantém um índice links->arquivos para facilitar navegação offline

Uso:
    pip install requests beautifulsoup4 markdownify
    python download_docs_md.py

Saída:
    downloads/
      SUMMARY.md
      README.md
      hard/.../*.md
      _assets/...      (imagens baixadas)
"""

from __future__ import annotations

import os
import re
import sys
import time
import mimetypes
from collections import deque
from pathlib import Path
from typing import Iterable
from urllib.parse import parse_qs, unquote, urldefrag, urljoin, urlparse

import requests
from bs4 import BeautifulSoup
from markdownify import markdownify as md

# ============================================================
# CONFIGURAÇÃO - Altere estas variáveis para apontar para outras documentações
# ============================================================
BASE_URL = "https://docs.exodusemulator.com/Archives/SSDDV25/segahtml/"
START_URL = urljoin(BASE_URL, "hard/index.htm")
ALLOWED_PREFIX = urljoin(BASE_URL, "hard/")
# ============================================================

OUTPUT_DIR = Path("docs/sega_saturn_hardware")
ASSET_DIR = OUTPUT_DIR / "_assets"
TIMEOUT = 30
DELAY = 0.25
MAX_RETRIES = 3

session = requests.Session()
session.headers.update(
    {
        "User-Agent": "Mozilla/5.0 (compatible; DocsMarkdownMirror/1.0)",
        "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/*;q=0.8,*/*;q=0.7",
    }
)

visited_pages: set[str] = set()
downloaded_assets: dict[str, Path] = {}
page_title_index: dict[Path, str] = {}
parent_links: dict[Path, set[Path]] = {}


def log(*args):
    print(*args, flush=True)


def normalize_url(url: str) -> str:
    url, _frag = urldefrag(url)
    return url


def resolve_special_url(url: str) -> str:
    parsed = urlparse(url)
    if parsed.path.endswith("/index.html"):
        page = parse_qs(parsed.query).get("page")
        if page:
            return normalize_url(urljoin(BASE_URL, page[0]))
    return normalize_url(url)


def is_allowed(url: str) -> bool:
    return resolve_special_url(url).startswith(ALLOWED_PREFIX)


def is_html_like(url: str) -> bool:
    path = urlparse(resolve_special_url(url)).path.lower()
    return path.endswith((".htm", ".html")) or path.endswith("/") or "." not in Path(path).name


def fetch_bytes(url: str) -> tuple[bytes, str]:
    last_exc = None
    for attempt in range(1, MAX_RETRIES + 1):
        try:
            response = session.get(resolve_special_url(url), timeout=TIMEOUT)
            response.raise_for_status()
            content_type = response.headers.get("Content-Type", "").split(";")[0].strip().lower()
            time.sleep(DELAY)
            return response.content, content_type
        except Exception as exc:  # noqa: BLE001
            last_exc = exc
            if attempt < MAX_RETRIES:
                time.sleep(min(1.0 * attempt, 3.0))
    raise RuntimeError(f"Falha ao baixar {url}: {last_exc}")


def fetch_text(url: str) -> str:
    data, _content_type = fetch_bytes(url)
    for encoding in ("utf-8", "cp1252", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("utf-8", errors="replace")


def relative_site_path(url: str) -> Path:
    resolved = resolve_special_url(url)
    rel = urlparse(resolved).path.removeprefix(urlparse(BASE_URL).path)
    return Path(unquote(rel))


def make_output_path(url: str) -> Path:
    rel_path = relative_site_path(url)
    if rel_path.suffix.lower() in {".htm", ".html"}:
        rel_path = rel_path.with_suffix(".md")
    else:
        rel_path = rel_path / "index.md"
    return OUTPUT_DIR / rel_path


def safe_asset_name(url: str, content_type: str) -> Path:
    rel = relative_site_path(url)
    suffix = rel.suffix
    if not suffix:
        guessed = mimetypes.guess_extension(content_type or "") or ".bin"
        suffix = guessed
        rel = rel.with_suffix(suffix)
    return ASSET_DIR / rel


def clean_soup(soup: BeautifulSoup) -> BeautifulSoup:
    for tag in soup(["script", "style", "noscript"]):
        tag.decompose()
    for tag in soup.find_all("base"):
        tag.decompose()
    return soup


def rewrite_page_link(href: str, current_url: str) -> str:
    if not href:
        return href
    absolute = resolve_special_url(urljoin(current_url, href))
    if not is_allowed(absolute):
        return absolute
    dst = make_output_path(absolute)
    src = make_output_path(current_url)
    rel = os.path.relpath(dst, start=src.parent)
    return rel.replace("\\", "/")


def should_download_asset(url: str) -> bool:
    parsed = urlparse(resolve_special_url(url))
    path = parsed.path.lower()
    return path.endswith(
        (
            ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".webp", ".svg",
            ".ico"
        )
    )


def download_asset(url: str, current_url: str) -> str:
    absolute = resolve_special_url(urljoin(current_url, url))
    if absolute in downloaded_assets:
        asset_path = downloaded_assets[absolute]
    else:
        try:
            data, content_type = fetch_bytes(absolute)
            asset_path = safe_asset_name(absolute, content_type)
            asset_path.parent.mkdir(parents=True, exist_ok=True)
            asset_path.write_bytes(data)
            downloaded_assets[absolute] = asset_path
            log(f"[ASSET] {absolute} -> {asset_path}")
        except Exception as exc:  # noqa: BLE001
            log(f"[ASSET-ERRO] {absolute}: {exc}")
            return absolute

    current_md = make_output_path(current_url)
    rel = os.path.relpath(asset_path, start=current_md.parent)
    return rel.replace("\\", "/")


def extract_links(soup: BeautifulSoup, current_url: str) -> Iterable[str]:
    for a in soup.find_all("a", href=True):
        href = a["href"].strip()
        if not href or href.startswith(("#", "mailto:", "javascript:")):
            continue
        absolute = resolve_special_url(urljoin(current_url, href))
        if is_allowed(absolute) and is_html_like(absolute):
            yield absolute


def title_from_soup(soup: BeautifulSoup) -> str:
    if soup.title and soup.title.string:
        return soup.title.string.strip()
    h1 = soup.find(["h1", "h2"])
    if h1:
        return h1.get_text(" ", strip=True)
    return ""


def html_to_markdown(html: str, current_url: str) -> tuple[str, str]:
    soup = clean_soup(BeautifulSoup(html, "html.parser"))

    for a in soup.find_all("a", href=True):
        a["href"] = rewrite_page_link(a["href"], current_url)

    for img in soup.find_all("img", src=True):
        src = img["src"]
        absolute = resolve_special_url(urljoin(current_url, src))
        if should_download_asset(absolute):
            img["src"] = download_asset(src, current_url)
        else:
            img["src"] = absolute

    root = soup.body if soup.body else soup
    markdown = md(str(root), heading_style="ATX", bullets="-", strip=["script", "style", "noscript"])

    title = title_from_soup(soup)
    if title:
        markdown = f"# {title}\n\n{markdown}"

    markdown = re.sub(r"\n{3,}", "\n\n", markdown).strip() + "\n"
    return markdown, title or make_output_path(current_url).stem


def register_parent_child(current_url: str, child_url: str) -> None:
    parent = make_output_path(current_url)
    child = make_output_path(child_url)
    parent_links.setdefault(parent, set()).add(child)


def crawl(start_url: str) -> None:
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    ASSET_DIR.mkdir(parents=True, exist_ok=True)
    queue = deque([resolve_special_url(start_url)])

    while queue:
        url = queue.popleft()
        if url in visited_pages:
            continue
        visited_pages.add(url)
        log(f"[GET] {url}")

        try:
            html = fetch_text(url)
        except Exception as exc:  # noqa: BLE001
            log(f"[ERRO] {url}: {exc}")
            continue

        soup = clean_soup(BeautifulSoup(html, "html.parser"))
        links = list(dict.fromkeys(extract_links(soup, url)))
        for link in links:
            register_parent_child(url, link)
            if link not in visited_pages:
                queue.append(link)

        try:
            out = make_output_path(url)
            out.parent.mkdir(parents=True, exist_ok=True)
            markdown, title = html_to_markdown(html, url)
            out.write_text(markdown, encoding="utf-8")
            page_title_index[out] = title
            log(f"[OK]  {out}")
        except Exception as exc:  # noqa: BLE001
            log(f"[ERRO-CONV] {url}: {exc}")


def generate_summary() -> Path:
    summary_path = OUTPUT_DIR / "SUMMARY.md"
    md_files = sorted(
        p for p in OUTPUT_DIR.rglob("*.md")
        if p.name.lower() != "summary.md"
    )

    lines = ["# Summary", ""]
    root = OUTPUT_DIR / "hard" / "index.md"

    if root.exists():
        title = page_title_index.get(root, "hard")
        lines.append(f"- [{title}]({root.relative_to(OUTPUT_DIR).as_posix()})")

    for path in md_files:
        rel = path.relative_to(OUTPUT_DIR)
        if path == root:
            continue
        depth = max(0, len(rel.parts) - 2)
        indent = "  " * depth
        title = page_title_index.get(path, path.stem)
        lines.append(f"{indent}- [{title}]({rel.as_posix()})")

    summary_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
    return summary_path


def write_readme() -> Path:
    readme = OUTPUT_DIR / "README.md"
    readme.write_text(
        "# Documentação baixada\n\n"
        "Conteúdo gerado por `download_docs_md.py`.\n\n"
        "## Recursos\n\n"
        "- Converte páginas HTML para Markdown\n"
        "- Baixa imagens para `_assets/`\n"
        "- Reescreve links internos para navegação offline\n"
        "- Gera `SUMMARY.md`\n\n"
        "## Uso\n\n"
        "```bash\n"
        "pip install requests beautifulsoup4 markdownify\n"
        "python download_docs_md.py\n"
        "```\n",
        encoding="utf-8",
    )
    return readme


def main() -> int:
    try:
        log(f"Iniciando download da documentação...")
        log(f"BASE_URL: {BASE_URL}")
        log(f"OUTPUT_DIR: {OUTPUT_DIR.resolve()}")
        log("")
        crawl(START_URL)
        summary = generate_summary()
        readme = write_readme()
        log("")
        log(f"[OK]  {summary}")
        log(f"[OK]  {readme}")
        log(f"Concluído. Saída em: {OUTPUT_DIR.resolve()}")
        return 0
    except KeyboardInterrupt:
        log("Interrompido pelo usuário.")
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
