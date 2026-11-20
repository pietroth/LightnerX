"""Read module implementations for the supplemental transport fault tests."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def implementation(source):
    return '\n'.join(line.replace('export ', '')
                     for line in source.splitlines()
                     if not line.startswith(('module;', 'export module ', 'import ', 'export import ')))
