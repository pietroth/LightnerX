"""Exercise the public Client and Server in separate processes over UDP."""
import argparse
import errno
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


def available_port():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as probe:
        probe.bind(('127.0.0.1', 0))
        return probe.getsockname()[1]


def port_in_use(port):
    for family, address in ((socket.AF_INET, '0.0.0.0'), (socket.AF_INET6, '::')):
        try:
            with socket.socket(family, socket.SOCK_DGRAM) as probe:
                probe.bind((address, port))
        except OSError as error:
            if error.errno == errno.EADDRINUSE:
                return True
            if error.errno not in (errno.EAFNOSUPPORT, errno.EPROTONOSUPPORT, errno.EADDRNOTAVAIL):
                raise
    return False


def wait_for_server(server, log, port):
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        output = log.read_text()
        if 'READY' in output.splitlines():
            return True
        if server.poll() is not None:
            # This startup diagnostic is also emitted by the reference binary.
            output = log.read_text()
            if 'IP listener start' in output.splitlines() and port_in_use(port):
                return False
            raise RuntimeError('Server failed to start: ' + output)
        time.sleep(0.01)
    raise RuntimeError('Server startup timed out: ' + log.read_text())


with tempfile.TemporaryDirectory(prefix='lightnerx-integration-') as directory:
    log = pathlib.Path(directory) / 'server.log'
    for _ in range(3):
        port = available_port()
        with log.open('w') as output:
            server = subprocess.Popen(
                [args.server_binary or args.binary, '--server', str(port)],
                stdout=output, stderr=subprocess.STDOUT)
            try:
                if not wait_for_server(server, log, port):
                    continue
                subprocess.run([args.client_binary or args.binary, '--client', str(port)], check=True, timeout=25)
                if server.wait(timeout=20) != 0:
                    raise RuntimeError('Server failed: ' + log.read_text())
                print(log.read_text(), end='')
                break
            finally:
                if server.poll() is None:
                    server.kill()
                    server.wait()
    else:
        raise RuntimeError('Server ports were taken before startup on three attempts: ' + log.read_text())
