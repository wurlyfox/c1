#!/usr/bin/env python3
"""Bundle the pack converter into one downloadable ZIP (published with the web
build as c1-pack-tool.zip), so people don't need the repository:

    c1-pack-tool/
        Make pack (Mac).command       double-click on macOS
        Make pack (Windows).bat       double-click on Windows
        README.txt
        make_c1_pack.py
"""
import sys
import time
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent

README = """C1 WEB PACK CONVERTER - from your .bin to a ZIP with only what the game needs
===============================================================================

1. Install Python 3 (only the first time): https://www.python.org/downloads/
   On Windows tick "Add Python to PATH" while installing. macOS may offer to
   install it by itself the first time you run the converter.

2. Put your disc image (.bin or .iso of Crash Bandicoot, NTSC-U, SCUS-94900)
   in this folder, or in Downloads, Desktop or Documents.

3. Double-click:
   - Mac:      "Make pack (Mac).command"
               (if macOS blocks it: right-click -> Open -> Open)
   - Windows:  "Make pack (Windows).bat"
   - Linux / terminal:  python3 make_c1_pack.py "path/to/game.bin"
   The converter finds your image and writes c1-pack.zip next to it.

4. Open the c1 web page and pick that file with "Choose game ZIP".

The ZIP contains the level files of the game: it is for your own use only.
Do not share it.

-------------------------------------------------------------------------------

CONVERSOR DE C1 WEB - de tu .bin a un ZIP con solo lo que el juego necesita

1. Instala Python 3 (solo la primera vez): https://www.python.org/downloads/
   En Windows marca "Add Python to PATH" al instalar.

2. Pon tu imagen del disco (.bin o .iso de Crash Bandicoot, NTSC-U,
   SCUS-94900) en esta carpeta, o en Descargas, Escritorio o Documentos.

3. Doble clic en:
   - Mac:      "Make pack (Mac).command"
               (si macOS lo bloquea: clic derecho -> Abrir -> Abrir)
   - Windows:  "Make pack (Windows).bat"
   El conversor encuentra tu imagen y crea c1-pack.zip junto a ella.

4. Abre la pagina de c1 web y elige ese archivo con "Choose game ZIP".

El ZIP contiene los niveles del juego: es solo para tu uso personal.
No lo compartas.
"""


def add(z, name, data, mode=0o644):
    info = zipfile.ZipInfo("c1-pack-tool/" + name, time.localtime()[:6])
    info.compress_type = zipfile.ZIP_DEFLATED
    info.create_system = 3  # Unix, so the permission bits below are honoured
    info.external_attr = (0o100000 | mode) << 16
    z.writestr(info, data)


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "c1-pack-tool.zip")
    lf = lambda b: b.replace(b"\r\n", b"\n")
    crlf = lambda b: lf(b).replace(b"\n", b"\r\n")
    with zipfile.ZipFile(out, "w") as z:
        add(z, "README.txt", crlf(README.encode("utf-8")))
        add(z, "make_c1_pack.py", lf((HERE / "make_c1_pack.py").read_bytes()), 0o755)
        add(z, "Make pack (Mac).command", lf((HERE / "make_c1_pack.command").read_bytes()), 0o755)
        add(z, "Make pack (Windows).bat", crlf((HERE / "make_c1_pack.bat").read_bytes()))
    print("wrote %s" % out)


if __name__ == "__main__":
    main()
