"""Extract complete module examples so CMake builds them as consumers."""
from pathlib import Path
import re
import sys

output = Path(sys.argv[1])
output.mkdir(parents=True, exist_ok=True)
sources = []
for page in map(Path, sys.argv[2:]):
    blocks = re.findall(r'^```cpp\n(.*?)^```$', page.read_text(), re.MULTILINE | re.DOTALL)
    examples = [block for block in blocks if re.search(r'^import lightnerx\.', block, re.MULTILINE)]
    if not examples:
        raise SystemExit(f'No module examples found in {page}')
    for index, example in enumerate(examples, 1):
        path = output / f'{page.stem.lower()}_{index}.cpp'
        if 'int main(' not in example:
            example += '\nint main() {}\n'
        if not path.exists() or path.read_text() != example:
            path.write_text(example)
        sources.append(str(path))

print(';'.join(sources))
