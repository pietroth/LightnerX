"""Compose production declarations/definitions for deterministic fault tests.

Only project dependency includes are suppressed: the caller supplies each real
header and implementation once, substituting the same narrow transport seams as
the module-era tests. The ordinary C++ tests link the unmodified library.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE_FLAGS = [
    '-I' + str(ROOT / directory)
    for directory in (
        'include',
        'external/GameNetworkingSockets/include',
    )
]

def body(path, *, steam=True):
    stem = Path(path).with_suffix('')
    files = [ROOT / 'include/lightnerx' / stem.with_suffix('.hpp'), ROOT / stem.with_suffix('.cpp')]
    lines = []
    for file in files:
        for line in file.read_text().splitlines():
            if line.startswith('#include <lightnerx/') and line not in (
                '#include <lightnerx/Result.hpp>',
                '#include <lightnerx/network/connection_handle.hpp>',
            ):
                continue
            if not steam and line.startswith('#include') and 'steam/' in line:
                continue
            lines.append(line)
    return '\n'.join(lines)


def replace_once(source, original, replacement):
    if source.count(original) != 1:
        raise SystemExit(f'Expected exactly one fault injection point: {original}')
    return source.replace(original, replacement)
