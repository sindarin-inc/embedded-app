#!/usr/bin/env python3
"""
Run `make test-kindle-short` multiple times with a timeout and record failing diff images.

For each run the script:
  * Clears existing simulator artifacts under test/CatchTests/KindleImages/TestRun.
  * Executes the command with a per-run timeout (default 10 seconds).
  * Copies any generated images into test/CatchTests/KindleImages/TestOutput/run_##/images.
  * Writes a per-run log with the return code, diff image list, and raw stdout/stderr.
  * Prints a concise console summary focused on failures, diffs, and timeouts.
  * Produces an aggregate summary file in TestOutput/ summarising successes, failures, and diff image counts.

Example:
  ./scripts/kindle_short_inventory.py --runs 10 --timeout 10
"""

from __future__ import annotations

import argparse
import collections
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

DEFAULT_CMD = "make test-kindle-short"
SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_CWD = SCRIPT_DIR.parent
TEST_IMAGES_ROOT = DEFAULT_CWD / "test/CatchTests/KindleImages"
TEST_RUN_DIR = TEST_IMAGES_ROOT / "TestRun"
TEST_OUTPUT_ROOT = TEST_IMAGES_ROOT / "TestOutput"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--runs",
        type=int,
        default=10,
        help="Number of times to execute the command (default: 10)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Per-run timeout in seconds (default: 10)",
    )
    parser.add_argument(
        "--cmd",
        default=DEFAULT_CMD,
        help=f"Command to execute each run (default: {DEFAULT_CMD!r})",
    )
    parser.add_argument(
        "--cwd",
        type=Path,
        default=DEFAULT_CWD,
        help="Working directory for the command (default: repository root)",
    )
    return parser.parse_args()


def ensure_directory(path: Path) -> Path:
    path.mkdir(parents=True, exist_ok=True)
    return path


def clear_directory(path: Path) -> None:
    if not path.exists():
        return
    for child in list(path.iterdir()):
        if child.is_dir() and not child.is_symlink():
            shutil.rmtree(child)
        else:
            child.unlink()


def extract_diff_names(stdout: str) -> set[str]:
    diff_names: set[str] = set()
    for line in stdout.splitlines():
        if "_diff.png" not in line:
            continue
        for token in line.split():
            if token.endswith("_diff.png"):
                diff_names.add(Path(token).name)
    return diff_names


def run_once(args: argparse.Namespace) -> dict:
    cmd = shlex.split(args.cmd)
    try:
        result = subprocess.run(
            cmd,
            cwd=args.cwd,
            timeout=args.timeout,
            text=True,
            capture_output=True,
        )
    except subprocess.TimeoutExpired as exc:
        return {
            "timeout": True,
            "return_code": None,
            "stdout": exc.stdout or "",
            "stderr": exc.stderr or "",
            "diffs_stdout": set(),
        }

    return {
        "timeout": False,
        "return_code": result.returncode,
        "stdout": result.stdout,
        "stderr": result.stderr,
        "diffs_stdout": extract_diff_names(result.stdout),
    }


def collect_generated_images(destination: Path) -> list[str]:
    ensure_directory(destination)
    generated: list[str] = []
    if not TEST_RUN_DIR.exists():
        return generated

    for child in list(TEST_RUN_DIR.iterdir()):
        target = destination / child.name
        if child.is_dir() and not child.is_symlink():
            shutil.copytree(child, target, dirs_exist_ok=True)
            shutil.rmtree(child)
            generated.append(child.name)
        else:
            shutil.copy2(child, target)
            child.unlink()
            generated.append(child.name)
    return generated


def write_run_log(
    run_dir: Path,
    run_index: int,
    outcome: dict,
    diff_names: list[str],
    timeout_seconds: float,
) -> None:
    log_path = run_dir / "output.txt"
    rc_text = "TIMEOUT" if outcome["timeout"] else str(outcome["return_code"])
    diff_list = ", ".join(diff_names) if diff_names else "(none)"
    diff_count = len(diff_names)

    with log_path.open("w", encoding="utf-8") as fh:
        fh.write(f"Run: {run_index}\n")
        fh.write(f"Return code: {rc_text}\n")
        fh.write(f"Image failures: {diff_count}\n")
        fh.write(f"Failed images: {diff_list}\n")
        if outcome["timeout"]:
            fh.write(f"Timeout after: {timeout_seconds}s\n")
        fh.write("\n--- STDOUT ---\n")
        fh.write(outcome["stdout"])
        if outcome["stdout"] and not outcome["stdout"].endswith("\n"):
            fh.write("\n")
        fh.write("\n--- STDERR ---\n")
        fh.write(outcome["stderr"])
        if outcome["stderr"] and not outcome["stderr"].endswith("\n"):
            fh.write("\n")


def write_summary_file(
    output_root: Path,
    total_runs: int,
    success_count: int,
    failure_count: int,
    diff_counter: collections.Counter[str],
    timeouts: list[int],
    failures_without_diff: list[int],
) -> None:
    summary_path = output_root / "summary.txt"
    with summary_path.open("w", encoding="utf-8") as fh:
        fh.write(f"Total runs: {total_runs}\n")
        fh.write(f"Successes: {success_count}\n")
        fh.write(f"Failures: {failure_count}\n")
        if timeouts:
            fh.write(f"Timeout runs: {', '.join(str(i) for i in timeouts)}\n")
        if failures_without_diff:
            fh.write(
                "Non-zero exit without diff runs: "
                + ", ".join(str(i) for i in failures_without_diff)
                + "\n"
            )
        fh.write("\nDiff image failures:\n")
        if diff_counter:
            for name, count in diff_counter.most_common():
                fh.write(f"  {name}: {count}\n")
        else:
            fh.write("  (none)\n")


def main() -> int:
    args = parse_args()

    ensure_directory(TEST_IMAGES_ROOT)
    ensure_directory(TEST_RUN_DIR)
    ensure_directory(TEST_OUTPUT_ROOT)

    # Clear previous artifacts before starting the run batch.
    clear_directory(TEST_OUTPUT_ROOT)
    clear_directory(TEST_RUN_DIR)

    diff_counter: collections.Counter[str] = collections.Counter()
    timeouts: list[int] = []
    failures_without_diff: list[int] = []
    success_count = 0
    failure_count = 0

    print(f"Running {args.cmd!r} {args.runs} times with {args.timeout}s timeout...")

    for run_index in range(1, args.runs + 1):
        run_dir = ensure_directory(TEST_OUTPUT_ROOT / f"run_{run_index:02d}")
        images_dir = ensure_directory(run_dir / "images")

        # Guarantee TestRun is empty before the run starts.
        clear_directory(TEST_RUN_DIR)

        outcome = run_once(args)
        generated_files = collect_generated_images(images_dir)

        diff_names = sorted(
            set(outcome["diffs_stdout"]) | {name for name in generated_files if name.endswith("_diff.png")}
        )

        write_run_log(run_dir, run_index, outcome, diff_names, args.timeout)

        if outcome["timeout"]:
            timeouts.append(run_index)
            failure_count += 1
            print(f"Run {run_index}: timeout after {args.timeout}s")
            continue

        if diff_names:
            diff_counter.update(diff_names)
            failure_count += 1
            print(f"Run {run_index}: diff -> {', '.join(diff_names)}")
        elif outcome["return_code"]:
            failures_without_diff.append(run_index)
            failure_count += 1
            print(f"Run {run_index}: non-zero exit ({outcome['return_code']}) without diff output")
        else:
            success_count += 1

    print("\nSummary:")
    if diff_counter:
        for name, count in diff_counter.most_common():
            print(f"  {name}: {count} occurrence(s)")
    else:
        print("  No diff images observed across runs.")

    if timeouts:
        joined = ", ".join(str(i) for i in timeouts)
        print(f"  Timeouts on runs: {joined}")

    if failures_without_diff:
        joined = ", ".join(str(i) for i in failures_without_diff)
        print(f"  Non-zero exit without diff output on runs: {joined}")

    write_summary_file(
        TEST_OUTPUT_ROOT,
        args.runs,
        success_count,
        failure_count,
        diff_counter,
        timeouts,
        failures_without_diff,
    )

    return 0


if __name__ == "__main__":
    sys.exit(main())
