#!/usr/bin/env python3
"""Network Systems 09: start N copies of the original-style chat client in auto mode.
Run only against the local, separately started chat-server09 (127.0.0.1:18081).
No extra C++ threading model. Keeps logs per run and summarizes all clients.
"""
import argparse
import csv
from datetime import datetime
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser(description="Lesson09 localhost A/B comparison")
    parser.add_argument("mode", choices=("A", "B"),
                        help="A: server 0ms / B: server 10ms (must match actual server)")
    parser.add_argument("clients", type=int, help="concurrent client processes, 1..80")
    parser.add_argument("requests", type=int, help="SAY count per client, 1..200")
    parser.add_argument("--hold", type=int, default=1,
                        help="hold all connected clients before sending (0..10 seconds)")
    args = parser.parse_args()
    if not 1 <= args.clients <= 80 or not 1 <= args.requests <= 200:
        parser.error("clients 1..80 and requests 1..200 only")
    if not 0 <= args.hold <= 10:
        parser.error("--hold must be 0..10")
    if args.mode == "B" and args.clients * args.requests > 400:
        parser.error("Mode B is intentionally limited to 400 total SAYs for this lesson")
    binary = ROOT / "chat-client09"
    if not binary.is_file():
        parser.error("compile chat-client09.cpp before running")

    output = ROOT / "results-ab"
    output.mkdir(exist_ok=True)
    dirname = f"{datetime.now().strftime('%Y%m%d_%H%M%S_%f')}_{args.mode}_n{args.clients}_r{args.requests}"
    run = output / dirname
    run.mkdir()
    print(f"Run: {run}")
    print("Confirm the server started with mode", "0" if args.mode == "A" else "10",
          "(the runner does NOT change the server mode)")

    processes = []
    open_files = []
    for i in range(args.clients):
        name = f"load{i}"
        log = (run / f"console-{name}.txt").open("w", encoding="utf8")
        open_files.append(log)
        command = [str(binary), "--auto", name, str(args.requests), args.mode, str(args.hold)]
        try:
            p = subprocess.Popen(command, cwd=run, stdout=log, stderr=subprocess.STDOUT)
            processes.append(p)
        except OSError as e:
            print(f"Could not start {name}: {e}", file=sys.stderr)
            break
    codes = []
    for process in processes:
        try:
            codes.append(process.wait(timeout=180))
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            codes.append(124)
    for f in open_files:
        f.close()

    elapsed_values = []
    send_values = []
    finish_values = []
    success = 0
    for path in sorted(run.glob("client-log-*.csv")):
        with path.open(newline="", encoding="utf8") as f:
            for row in csv.DictReader(f):
                if row["mode"] != args.mode:
                    raise ValueError(f"unexpected mode in {path}: {row['mode']}")
                send_values.append(int(row["send_steady_ns"]))
                finish_values.append(int(row["finish_steady_ns"]))
                if row["success"] == "1":
                    success += 1
                    elapsed_values.append(float(row["rtt_ms"]))
    attempted = args.clients * args.requests
    failed = attempted - success
    elapsed_values.sort()
    avg = sum(elapsed_values) / success if success else 0.0
    p95 = elapsed_values[(95 * success + 99) // 100 - 1] if success else 0.0
    # This window excludes the optional --hold before the first SAY.
    window_ms = (max(finish_values) - min(send_values)) / 1_000_000 \
        if send_values and finish_values else 0.0
    throughput = success * 1000 / window_ms if window_ms > 0 else 0.0
    record = {
        "run": dirname, "mode": args.mode, "clients": args.clients,
        "requests_per_client": args.requests, "attempted": attempted,
        "success": success, "failed": failed,
        "avg_rtt_ms": f"{avg:.3f}", "p95_rtt_ms": f"{p95:.3f}",
        "elapsed_ms": f"{window_ms:.3f}",
        "throughput_req_s": f"{throughput:.2f}",
    }
    fields = list(record)
    local = run / "summary.csv"
    with local.open("w", newline="", encoding="utf8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        writer.writerow(record)
    combined = output / "all-summary.csv"
    new = not combined.exists() or combined.stat().st_size == 0
    with combined.open("a", newline="", encoding="utf8") as f:
        writer = csv.DictWriter(f, fieldnames=fields)
        if new:
            writer.writeheader()
        writer.writerow(record)
    print("mode", args.mode, "clients", args.clients, "success", success,
          "failed", failed, "avg", record["avg_rtt_ms"], "ms",
          "p95", record["p95_rtt_ms"], "ms")
    print("Summary:", local)
    print("All runs:", combined)
    if any(code != 0 for code in codes) or len(processes) != args.clients:
        print("Some clients exited with errors; inspect console-*.txt", file=sys.stderr)
        return 2
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
