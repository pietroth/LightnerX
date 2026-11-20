"""Compare deterministic public behavior with the original C++26 recording."""
import difflib
import pathlib
import subprocess
import sys

actual = subprocess.check_output([sys.argv[1]], text=True, timeout=30)
expected = pathlib.Path(sys.argv[2]).read_text()
# Normalize the historical Linux errno values. The executable checks std::errc.
error_labels = {'0': 'success', '107': 'not_connected', '22': 'invalid_argument',
                '11': 'resource_unavailable_try_again'}
expected_lines = expected.splitlines(keepends=True)
error_keys = set()
for index, line in enumerate(expected_lines):
    key, value = line.strip().split('=', 1)
    if value not in error_labels:
        break
    error_keys.add(key)
    expected_lines[index] = f'{key}={error_labels[value]}\n'
if sys.byteorder != 'little':
    # Integers in the serialized frames and digest use native byte order.
    keys = error_keys | {'parser-calls'}
    expected_lines = [line for line in expected_lines if line.split('=', 1)[0] in keys]
    actual = ''.join(line for line in actual.splitlines(keepends=True) if line.split('=', 1)[0] in keys)
expected = ''.join(expected_lines)
if actual != expected:
    sys.stderr.writelines(difflib.unified_diff(
        expected.splitlines(keepends=True), actual.splitlines(keepends=True),
        fromfile='C++26 reference', tofile='current library'))
    raise SystemExit(1)
if sys.byteorder == 'little':
    print('Public behavior matches the C++26 reference')
else:
    print('Error results and parser call count match the C++26 reference')
