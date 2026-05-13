"""pytest configuration for scripts/test/.

Adds the scripts/ directory to sys.path so every test module can import
scripts directly without each file duplicating the path mutation.
"""

from __future__ import annotations

import sys
from pathlib import Path

_scripts_dir = str(Path(__file__).parent.parent)
if _scripts_dir not in sys.path:
    sys.path.insert(0, _scripts_dir)
