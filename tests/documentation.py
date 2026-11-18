"""Generate compilable documentation examples and check API excerpts against headers.

Complete programs keep their own includes. Fragments use explicit application
contexts, and API declarations are compared to the real types in unevaluated
expressions. The documentation executable runs the byte view and result examples.
Network example loops are compiled and linked separately.
"""
import argparse
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote

parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
args.output.mkdir(parents=True, exist_ok=True)
pages = [root / 'README.md', *sorted((root / 'docs').glob('*.md'))]
blocks = {}
for page in pages:
    text = page.read_text()
    blocks[page.name] = re.findall(r'```cpp\n(.*?)```', text, re.S)
    for destination in re.findall(r'\[[^\]]*\]\(([^)]+)\)', re.sub(r'```.*?```', '', text, flags=re.S)):
        if '://' not in destination:
            path, _, anchor = destination.partition('#')
            target = page.parent / unquote(path) if path else page
            if not target.exists():
                raise SystemExit(f'{page}: broken local link {destination}')
            if anchor and target.suffix == '.md':
                headings = re.findall(r'^#+ (.+)$', target.read_text(), re.M)
                slugs = [re.sub(r'[^\w\- ]', '', heading.lower().replace('`', '')).replace(' ', '-')
                         for heading in headings]
                if anchor not in slugs:
                    raise SystemExit(f'{page}: broken local anchor {destination}')
    for language, code in re.findall(r'```(bash|cmake)\n(.*?)```', text, re.S):
        if language == 'bash':
            subprocess.run(['bash', '-n'], input=code, text=True, check=True)
        elif any(token in code for token in ['CXX_MODULE', 'CXX_IMPORT_STD', 'cxx_std_26']):
            raise SystemExit(f'{page}: obsolete CMake example')
    for code in blocks[page.name]:
        if re.search(r'\bimport\s|std::(?:span|expected|unexpected|println|remove_cvref_t)\b|const auto& callback', code):
            raise SystemExit(f'{page}: post-C++17 example')

headers = ''.join('#include <' + str(p.relative_to(root / 'include')) + '>\n'
                  for p in sorted((root / 'include/lightnerx').rglob('*.hpp')))
headers += '#include <tuple>\n#include <type_traits>\n#include <iostream>\n#include <utility>\n'
using = 'using namespace lightnerx::communication;\nusing namespace lightnerx::network;\n'
files = []
runnable = []
covered = set()

def emit(name, source):
    path = args.output / (name + '.cpp')
    if not path.exists() or path.read_text() != source:
        path.write_text(source)
    files.append(path)

def take(page, index):
    covered.add((page, index))
    return blocks[page][index]

def complete(page, index, run=False):
    name = re.sub(r'\W', '_', page) + '_' + str(index)
    source = take(page, index)
    if 'int main()' in source:
        source = source.replace('int main()', 'int ' + name + '()')
        end = source.rfind('}')
        source = source[:end] + '    return 0;\n' + source[end:]
        if run:
            runnable.append(name)
    emit(name, source)

# The README presents each program first as consecutive fragments, then in full.
for start, stop, whole in [(0, 7, 13), (7, 13, 14)]:
    includes = blocks['README.md'][whole].split('int main()')[0]
    source = includes + 'int readme_fragments_' + str(start) + '() {\n'
    source += '\n'.join(take('README.md', i) for i in range(start, stop)) + '\n}\n'
    emit('readme_fragments_' + str(start), source)
    complete('README.md', whole)
for index in [0, 1, 3]:
    complete('first-steps.md', index)
for index in [2, 3]:
    complete('communication.md', index)
complete('setup.md', 0)
for index in [0, 1]:
    complete('cpp17-types.md', index, run=True)

context = '''
int example(MessageInterpreter& interpreter, NotificationRegistry& registry,
            ConnectionManager& connections, HSteamNetConnection connection,
            HSteamNetConnection firstConnection, HSteamNetConnection secondConnection,
            HSteamNetConnection connectionToSkip, tcb::span<const std::byte> bytes) {
    const auto payload = bytes;
    const Event event{static_cast<std::uint16_t>((sizeof(Event) + payload.size()) << 1), 47};
'''
for page, indices in [('first-steps.md', [2, 4]), ('communication.md', [0, 1]), ('api.md', [19, 23, 28])]:
    for index in indices:
        name = re.sub(r'\W', '_', page) + '_' + str(index)
        emit(name, headers + 'namespace ' + name + ' {\n' + using + context + take(page, index) + '\nreturn 0;\n}\n}\n')

traits = '''
template<class> struct signature;
template<class R, class... A> struct signature<R(*)(A...)> { using type = std::tuple<R, std::tuple<A...>, std::false_type, std::false_type>; };
template<class R, class... A> struct signature<R(*)(A...) noexcept> { using type = std::tuple<R, std::tuple<A...>, std::false_type, std::true_type>; };
template<class R, class C, class... A> struct signature<R(C::*)(A...)> { using type = std::tuple<R, std::tuple<A...>, std::false_type, std::false_type>; };
template<class R, class C, class... A> struct signature<R(C::*)(A...) noexcept> { using type = std::tuple<R, std::tuple<A...>, std::false_type, std::true_type>; };
template<class R, class C, class... A> struct signature<R(C::*)(A...) const> { using type = std::tuple<R, std::tuple<A...>, std::true_type, std::false_type>; };
template<class R, class C, class... A> struct signature<R(C::*)(A...) const noexcept> { using type = std::tuple<R, std::tuple<A...>, std::true_type, std::true_type>; };
template<class T> using signature_t = typename signature<T>::type;
struct Callback { void operator()(std::uint16_t, HSteamNetConnection) const {} };
'''
classes = {0: 'BaseProtocol', 1: 'MBT', 2: 'Event', 3: 'Notification', 4: 'NotificationRegistry',
           5: 'Intention', 6: 'Response', 7: 'UnsuccessfulResponse', 10: 'MessageInterpreter',
           11: 'MessageInterpreter', 12: 'MessageInterpreter', 15: 'Server', 16: 'Server',
           17: 'Client', 18: 'ConnectionManager', 24: 'Target'}
for index, actual in classes.items():
    code = take('api.md', index)
    declared = re.sub(r'\b' + actual + r'\s*\(', 'Declared(', code)
    checks = ''
    for match in re.finditer(r'(\w+)\s*\((.*?)\)\s*(?:const\s*)?(?:noexcept\s*)?;', code, re.S):
        name, parameters = match.groups()
        if name == actual:
            types = [re.sub(r'\s+\w+\s*$', '', parameter.strip()) for parameter in parameters.split(',')]
            checks += 'static_assert(std::is_constructible_v<' + actual + ', ' + ', '.join(types) + '>);\n'
        else:
            if name == 'forEach':
                name = 'template forEach<Callback>'
            checks += 'static_assert(std::is_same_v<signature_t<decltype(&Declared::' + name + ')>, signature_t<decltype(&' + actual + '::' + name + ')>>);\n'
    emit('api_' + str(index), headers + using + traits + 'namespace api_' + str(index) + ' {\nstruct Declared {\n' + declared + '\n};\n' + checks + '}\n')
for index, namespace in [(13, 'communication'), (14, 'network'), (20, 'network'), (21, 'network'), (22, 'network'), (27, 'network')]:
    code = take('api.md', index)
    checks = ''
    for name in re.findall(r'(\w+)\s*\(', code):
        checks += 'static_assert(std::is_same_v<decltype(&declared::' + name + '), decltype(&lightnerx::' + namespace + '::' + name + ')>);\n'
    emit('api_' + str(index), headers + using + 'namespace api_' + str(index) + ' { namespace declared {\n' + code + '}\n' + checks + '}\n')
for index, actual, fields in [(9, 'Message', ['header', 'payload']), (25, 'Target', ['connection', 'broadcastExceptions']), (26, 'OutboundMessage', ['message', 'target'])]:
    code = take('api.md', index)
    if index == 25:
        code = 'struct Declared {\n' + code + '};\n'
    else:
        code = code.replace('struct ' + actual, 'struct Declared')
    checks = ''.join('static_assert(std::is_same_v<decltype(Declared::' + field + '), decltype(' + actual + '::' + field + ')>);\n' for field in fields)
    emit('api_' + str(index), headers + using + 'namespace api_' + str(index) + ' {\n' + code + checks + '}\n')
code = take('api.md', 8)
checks = ''.join('static_assert(declared::' + name + ' == lightnerx::communication::' + name + ');\n'
                 for name in re.findall(r'uint8_t (\w+)', code))
emit('api_constants', headers + 'namespace declared {\n' + code + '}\n' + checks)

all_blocks = {(page, i) for page, snippets in blocks.items() for i in range(len(snippets))}
if all_blocks != covered:
    raise SystemExit('Documentation snippets need an explicit compilation context: ' + str(all_blocks ^ covered))
emit('main', ''.join('int ' + name + '();\n' for name in runnable) + 'int main() {\n' +
     ''.join('    if (' + name + '()) return 1;\n' for name in runnable) + '}\n')
print(';'.join(str(path) for path in files))
