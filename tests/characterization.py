"""Compare deterministic public behavior with the original C++26 recording."""
import difflib
import pathlib
import subprocess
import sys

actual = subprocess.check_output([sys.argv[1]], text=True, timeout=30)
expected = pathlib.Path(sys.argv[2]).read_text()
if actual != expected:
    sys.stderr.writelines(difflib.unified_diff(
        expected.splitlines(keepends=True), actual.splitlines(keepends=True),
        fromfile='C++26 reference', tofile='current library'))
    raise SystemExit(1)
print('Public behavior matches the C++26 reference')
