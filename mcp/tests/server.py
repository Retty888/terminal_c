#!/usr/bin/env python3
import subprocess, sys

def run(cmd: str) -> str:
    p = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if p.returncode != 0:
        sys.stderr.write(p.stdout + "\n" + p.stderr)
        raise SystemExit(1)
    return p.stdout or "ok"

def main():
    tools = {
        "pytest_quick": "pytest -q",
        "ruff_check": "ruff check ."
    }
    if len(sys.argv) < 2 or sys.argv[1] not in tools:
        print("tools:", ", ".join(tools.keys()))
        return
    cmd = tools[sys.argv[1]]
    out = run(cmd)
    sys.stdout.write(out)

if __name__ == "__main__":
    main()
