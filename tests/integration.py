"""Exercise the public Client and Server modules in separate processes over UDP."""
import pathlib
import socket
import subprocess
import sys
import tempfile
import time

binary = sys.argv[1]
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
    probe.bind(('127.0.0.1', 0))
    port = str(probe.getsockname()[1])

with tempfile.TemporaryDirectory(prefix='lightnerx-integration-') as directory:
    log = pathlib.Path(directory) / 'server.log'
    with log.open('w') as output:
        server = subprocess.Popen([binary, '--server', port], stdout=output, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic() + 10
            while 'READY' not in log.read_text():
                if server.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError('Server failed to start: ' + log.read_text())
                time.sleep(0.01)
            subprocess.run([binary, '--client', port], check=True, timeout=25)
            if server.wait(timeout=20) != 0:
                raise RuntimeError('Server failed: ' + log.read_text())
            print(log.read_text(), end='')
        finally:
            if server.poll() is None:
                server.kill()
                server.wait()
