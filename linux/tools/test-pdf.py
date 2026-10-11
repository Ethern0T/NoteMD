#!/usr/bin/env python3
"""Check real PDF text, pagination, links, images and failure handling.

Requires Poppler's pdfinfo/pdftotext, only for development tests.
"""
from pathlib import Path
import re
import subprocess
import tempfile
import shutil
import sys
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
binary = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / "build/notemd"

with tempfile.TemporaryDirectory(prefix="notemd-pdf-") as folder:
    folder = Path(folder)
    shutil.copyfile(root / "assets/icons/128x128/pt.notemd.NoteMD.png", folder / "imagem de teste.png")
    text = (
        "# Documento de verificação\n\n"
        "Ação, coração, João e avó: **forte**, *itálico* e `código`.\n\n"
        "- Item principal\n  - Item aninhado\n- [x] Tarefa concluída\n\n"
        "3. Passo terceiro\n4. Passo quarto\n\n"
        "[Ligação ativa](https://example.com/notemd?a=1&b=2)\n\n"
        "![Figura local](imagem%20de%20teste.png)\n\n"
        "## Tabela extensa\n\n| Cabeçalho teste | Descrição |\n| :--- | ---: |\n"
    )
    text += "".join(f"| Linha-{i:03d} | Conteúdo da linha {i} |\n" for i in range(120))
    text += "\n## Célula maior que uma página\n\n| Campo | Conteúdo |\n| --- | --- |\n"
    text += "| Longa | " + "Uma célula muito longa com acentuação. " * 260 + " FIM-CELULA |\n\n"
    text += "## Parágrafo extenso\n\n" + "Texto muito longo com informação útil. " * 500 + " FIM-PARAGRAFO\n\n"
    text += "~~~python\ndef exemplo():\n\n    return '**literal** João'\n~~~\n\nFIM-DOCUMENTO\n"
    source = folder / "teste.md"
    destination = folder / "teste.pdf"
    source.write_text(text)
    subprocess.run([binary, "--export-pdf", source, destination, "Documento de verificação"], check=True)
    info = subprocess.check_output(["pdfinfo", destination], text=True)
    pages = int(re.search(r"Pages:\s+(\d+)", info)[1])
    assert pages >= 6, info
    assert "A4" in info and "Documento de verificação" in info
    extracted = subprocess.check_output(["pdftotext", "-layout", destination, "-"], text=True)
    for marker in ["Ação, coração, João e avó", "Item principal", "Item aninhado", "Tarefa concluída",
                   "3.", "Passo terceiro", "4.", "Passo quarto", "Figura local", "FIM-CELULA",
                   "FIM-PARAGRAFO", "FIM-DOCUMENTO", "**literal** João"]:
        assert marker in extracted, f"Lost text: {marker}"
    for i in range(120):
        assert extracted.count(f"Linha-{i:03d}") == 1, f"Lost or duplicated row: {i}"
    assert extracted.count("Cabeçalho teste") > 1, "Missing repeated table headers"
    assert "**forte**" not in extracted and "~~~python" not in extracted
    images = subprocess.check_output(["pdfimages", "-list", destination], text=True)
    assert re.search(r"^\s*\d+\s+\d+\s+image", images, re.M), "Missing embedded image"
    urls = subprocess.check_output(["pdfinfo", "-url", destination], text=True)
    assert "https://example.com/notemd?a=1&b=2" in urls, "Missing active link"
    xml = subprocess.check_output(["pdftohtml", "-xml", "-hidden", "-stdout", destination], text=True)
    assert "<outline>" in xml and "Tabela extensa</item>" in xml, "Missing heading bookmarks"
    bbox = subprocess.check_output(["pdftotext", "-bbox", destination, "-"], text=True)
    document = ET.fromstring(bbox)
    words = document.findall(".//{*}word")
    assert words
    for word in words:
        assert float(word.attrib["xMin"]) >= 50, (word.text, word.attrib)
        assert float(word.attrib["xMax"]) <= 543, (word.text, word.attrib)
        assert float(word.attrib["yMin"]) >= 50, (word.text, word.attrib)
        assert float(word.attrib["yMax"]) <= 823, (word.text, word.attrib)
    # Empty notes and missing images produce valid, explicit output.
    source.write_text("")
    subprocess.run([binary, "--export-pdf", source, destination], check=True)
    assert "teste" in subprocess.check_output(["pdftotext", destination, "-"], text=True)
    source.write_text("![Em falta](inexistente.png)\n\nConteúdo preservado\n")
    result = subprocess.run([binary, "--export-pdf", source, destination], capture_output=True, text=True, check=True)
    assert "indisponível" in result.stderr
    assert "Conteúdo preservado" in subprocess.check_output(["pdftotext", destination, "-"], text=True)
    # A failed export must not replace an existing destination.
    result = subprocess.run([binary, "--export-pdf", source, folder], capture_output=True)
    assert result.returncode and folder.is_dir()
    assert not list(folder.glob("*.tmp-*")), "Unfinished output left behind"
    print(f"NoteMD PDF integration: OK ({pages} pages, {len(words)} text fragments)")
