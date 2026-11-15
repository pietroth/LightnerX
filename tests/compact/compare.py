"""Run identical contract cases against compact and the native standard type."""
import subprocess
import sys

outputs = [subprocess.check_output([binary], timeout=60) for binary in sys.argv[1:]]
if any(output != outputs[0] for output in outputs[1:]):
    raise SystemExit('compact/native output mismatch: ' + repr(outputs))
print(outputs[0].decode(), end='')
