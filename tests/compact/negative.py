"""Require invalid span/expected programs to fail after a positive control builds."""
import argparse
import pathlib
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
parser.add_argument('--include', required=True)
parser.add_argument('--native', action='store_true')
args = parser.parse_args()
headers = ('#include <span>\n#include <expected>\nnamespace c = std;\n' if args.native else
           '#include <lightnerx/compact/span.hpp>\n#include <lightnerx/compact/expected.hpp>\nnamespace c = lightnerx::compact;\n')
headers += '#include <array>\n#include <deque>\n#include <vector>\n'
cases = {
    'fixed extent mismatch': 'std::array<int, 3> a{}; c::span<int, 2> s(a);',
    'discarding const': 'const int a[2]{}; c::span<int> s(a);',
    'noncontiguous range': 'std::deque<int> a; c::span<int> s(a);',
    'packed boolean range': 'std::vector<bool> a; c::span<bool> s(a);',
    'subspan outside static extent': 'int a[2]{}; c::span<int, 2> s(a); s.subspan<3>();',
    'reference value': 'c::expected<int&, int> e;',
    'array value': 'c::expected<int[2], int> e;',
    'const error': 'c::expected<int, const int> e;',
    'void error': 'c::expected<int, void> e;',
    'unexpected void': 'c::unexpected<void> e;',
    'invalid monad error': 'c::expected<int,int> e; e.and_then([](int){ return c::expected<int,long>{}; });',
    'invalid monad value': 'c::expected<int,int> e; e.or_else([](int){ return c::expected<long,int>{}; });',
}
with tempfile.TemporaryDirectory(prefix='lightnerx-compile-contract-') as directory:
    source = pathlib.Path(directory) / 'case.cpp'
    def compile_case(body):
        source.write_text(headers + 'int main() { ' + body + ' }\n')
        return subprocess.run([args.compiler, '-std=c++23' if args.native else '-std=c++17',
                               '-pedantic-errors', '-fsyntax-only', '-I' + args.include, str(source)],
                              capture_output=True, text=True, timeout=30)
    control = compile_case('int a[2]{}; c::span<int> s(a); c::expected<int,int> e(4); return s[0] + *e;')
    if control.returncode:
        raise SystemExit('Positive compilation control failed:\n' + control.stderr)
    for name, body in cases.items():
        if compile_case(body).returncode == 0:
            raise SystemExit('Invalid program compiled: ' + name)
print('Rejected all ' + str(len(cases)) + ' invalid programs')
