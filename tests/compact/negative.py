"""Require invalid expected programs to fail after a positive control builds."""
import argparse
import pathlib
import shlex
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', required=True)
parser.add_argument('--include', required=True)
parser.add_argument('--flags', default='')
parser.add_argument('--native', action='store_true')
args = parser.parse_args()
headers = ('#include <expected>\nnamespace c = std;\n' if args.native else
           '#include <lightnerx/compact/expected.hpp>\nnamespace c = lightnerx::compact;\n')
headers += '#include <memory>\n'
cases = {
    'reference value': 'c::expected<int&, int> e;',
    'array value': 'c::expected<int[2], int> e;',
    'const error': 'c::expected<int, const int> e;',
    'void error': 'c::expected<int, void> e;',
    'unexpected void': 'c::unexpected<void> e;',
    'value access with noncopyable error': 'c::expected<int,std::unique_ptr<int>> e(1); std::move(e).value();',
    'invalid monad error': 'c::expected<int,int> e; e.and_then([](int){ return c::expected<int,long>{}; });',
    'invalid monad value': 'c::expected<int,int> e; e.or_else([](int){ return c::expected<long,int>{}; });',
}
with tempfile.TemporaryDirectory(prefix='lightnerx-compile-contract-') as directory:
    source = pathlib.Path(directory) / 'case.cpp'
    def compile_case(body):
        source.write_text(headers + 'int main() { ' + body + ' }\n')
        return subprocess.run([args.compiler, '-std=c++23' if args.native else '-std=c++17',
                               *shlex.split(args.flags), '-pedantic-errors', '-fsyntax-only', '-I' + args.include, str(source)],
                              capture_output=True, text=True, timeout=30)
    control = compile_case('c::expected<int,int> e(4); return *e;')
    if control.returncode:
        raise SystemExit('Positive compilation control failed:\n' + control.stderr)
    for name, body in cases.items():
        if compile_case(body).returncode == 0:
            raise SystemExit('Invalid program compiled: ' + name)
print('Rejected all ' + str(len(cases)) + ' invalid programs')
