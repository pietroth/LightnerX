"""Exercise the public Client and Server in separate processes over UDP."""
import argparse
import pathlib
import socket
import subprocess
import tempfile
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('binary', help='Default lifecycle test executable')
parser.add_argument('--server-binary', help='Override server for cross-version tests')
parser.add_argument('--client-binary', help='Override client for cross-version tests')
args = parser.parse_args()
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
    probe.bind(('127.0.0.1', 0))
    port = str(probe.getsockname()[1])

with tempfile.TemporaryDirectory(prefix='lightnerx-integration-') as directory:
    log = pathlib.Path(directory) / 'server.log'
    with log.open('w') as output:
        server = subprocess.Popen([args.server_binary or args.binary, '--server', port], stdout=output, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 10
            while 'READY' not in log.read_text():
                if server.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('Server failed to start: ' + log.read_text())
                time.sleep(0.01)
            subprocess.run([args.client_binary or args.binary, '--client', port], check=True, timeout=25)
            if server.wait(timeout=20) != 0:
                raise RuntimeError('Server failed: ' + log.read_text())
            print(log.read_text(), end='')
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()
