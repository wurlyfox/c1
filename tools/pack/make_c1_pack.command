#!/bin/bash
# Double-click launcher for macOS: makes c1-pack.zip from your disc image.
cd "$(dirname "$0")"
if ! command -v python3 >/dev/null 2>&1; then
  echo "Python 3 is needed: https://www.python.org/downloads/"
  read -r -p "Press Enter to close..."
  exit 1
fi
python3 make_c1_pack.py --pause "$@"
