#!/usr/bin/env python3
"""
Benchmark script for LeetObfuscator tests.
Compiles and runs tests with and without obfuscation, comparing full application
output (excluding the last timing line) and timing.

All successful runs of a test (plain + obfuscated) must produce identical output
bodies for a Match.
"""

import subprocess
import os
import sys
import re
import time
import shutil
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Union
from dataclasses import dataclass, field
from collections import Counter
from tabulate import tabulate

# ==================== CONFIGURATION ====================

TEST_FILES = [
    "StdContainersTest.cpp",
    "FloatingPointMathTest.cpp",
    "BigSignaturesTest.cpp",
    "BitwiseOperationsTest.cpp",
    "BranchingRecursionTest.cpp",
    "ControlFlowObfuscationTest.cpp",
    "MultithreadingTest.cpp",
    "PerformanceStressTest.cpp",
    "StringManipulationTest.cpp",
    "MultiModuleTest",
    # Or explicit list form:
    # ["MultiModuleTest/main.cpp", "MultiModuleTest/Foo.cpp"],
]

OPTIMIZATION_LEVELS = ["O3"]
RUN_COUNT = 5
REBUILD_PER_RUN = False
WARMUP_COMPILE = False
BASE_FLAGS = "-fno-exceptions"
EXTRA_FLAGS = ""
COMPILE_TIMEOUT = 300
EXECUTE_TIMEOUT = 120

SCRIPT_DIR = Path(__file__).parent
TESTS_DIR = SCRIPT_DIR
BUILD_DIR = SCRIPT_DIR / "benchmark_build"
OBFUSCATED_COMPILER = SCRIPT_DIR / "../../build/bin/clang++"
REGULAR_COMPILER = "clang++"
OUTPUT_FORMAT = "grid"

# ==================== DATA STRUCTURES ====================

@dataclass
class TestSpec:
    name: str
    sources: List[Path]


@dataclass
class TestResult:
    test_name: str
    optimization: str
    obfuscated: bool
    run_number: int
    output_body: str          # full stdout excluding the last (timing) line
    time_ns: int
    compile_time: float
    success: bool
    error_message: str = ""


@dataclass
class TestSummary:
    test_name: str
    optimization: str
    obfuscated: bool
    avg_time_ns: float
    avg_compile_time: float
    outputs: List[str] = field(default_factory=list)
    output_match: bool = True   # all runs of *this* config produced identical body
    all_runs_success: bool = True


# ==================== UTILITY FUNCTIONS ====================

def print_separator(char="=", length=80):
    print(char * length)


def print_header(text):
    print_separator()
    print(f" {text}")
    print_separator()


def normalize_test_entry(entry: Union[str, List, Tuple]) -> TestSpec:
    if isinstance(entry, (list, tuple)):
        if not entry:
            raise ValueError("Empty source list in TEST_FILES")
        sources = []
        for s in entry:
            p = TESTS_DIR / s
            if not p.exists():
                raise FileNotFoundError(f"Source not found: {p}")
            sources.append(p.resolve())
        main_candidates = [s for s in sources if s.name.lower() == "main.cpp"]
        if main_candidates:
            name = main_candidates[0].parent.name
        else:
            name = sources[0].stem
        return TestSpec(name=name, sources=sources)

    entry = str(entry)
    path = TESTS_DIR / entry

    if path.is_dir():
        sources = sorted(path.glob("*.cpp"))
        if not sources:
            raise FileNotFoundError(f"No .cpp files found in directory: {path}")
        return TestSpec(name=path.name, sources=[s.resolve() for s in sources])

    if path.is_file() and path.suffix == ".cpp":
        return TestSpec(name=path.stem, sources=[path.resolve()])

    raise FileNotFoundError(f"Test entry not found or not a .cpp / directory: {path}")


def extract_output_body_and_time(output: str) -> Tuple[Optional[str], Optional[int]]:
    """Everything except the last line is the body we compare; last line is timing."""
    lines = output.strip().split('\n')
    if len(lines) < 1:
        return None, None

    try:
        time_ns = int(lines[-1].strip())
    except (ValueError, IndexError):
        time_ns = None

    body = "" if len(lines) == 1 else '\n'.join(lines[:-1])
    return body, time_ns


def compile_test(sources: List[Path], output_path: Path, compiler: str,
                 optimization: str, obfuscated: bool) -> Tuple[bool, float, str]:
    flags = f"-{optimization} {BASE_FLAGS} {EXTRA_FLAGS}"
    src_args = " ".join(f'"{s}"' for s in sources)
    cmd = f'{compiler} {src_args} -o "{output_path}" {flags}'

    start_time = time.time()
    try:
        result = subprocess.run(
            cmd, shell=True, capture_output=True, text=True, timeout=COMPILE_TIMEOUT
        )
        compile_time = time.time() - start_time
        if result.returncode != 0:
            return False, compile_time, result.stderr
        return True, compile_time, ""
    except subprocess.TimeoutExpired:
        return False, time.time() - start_time, f"Compilation timeout after {COMPILE_TIMEOUT}s"
    except Exception as e:
        return False, time.time() - start_time, str(e)


def run_test(executable_path: Path) -> Tuple[bool, str, str]:
    try:
        result = subprocess.run(
            str(executable_path), capture_output=True, text=True, timeout=EXECUTE_TIMEOUT
        )
        if result.returncode != 0:
            return False, "", result.stderr
        return True, result.stdout, ""
    except subprocess.TimeoutExpired:
        return False, "", f"Execution timeout after {EXECUTE_TIMEOUT}s"
    except Exception as e:
        return False, "", str(e)


def run_single_test(spec: TestSpec, optimization: str, obfuscated: bool,
                    run_number: int, output_path: Path, compiler: str) -> TestResult:
    compile_time = 0.0
    if REBUILD_PER_RUN or not output_path.exists():
        compile_success, compile_time, compile_error = compile_test(
            spec.sources, output_path, compiler, optimization, obfuscated
        )
        if not compile_success:
            return TestResult(
                test_name=spec.name, optimization=optimization, obfuscated=obfuscated,
                run_number=run_number, output_body="", time_ns=0,
                compile_time=compile_time, success=False,
                error_message=f"Compilation failed: {compile_error}"
            )

    run_success, output, run_error = run_test(output_path)
    if not run_success:
        return TestResult(
            test_name=spec.name, optimization=optimization, obfuscated=obfuscated,
            run_number=run_number, output_body="", time_ns=0,
            compile_time=compile_time, success=False,
            error_message=f"Execution failed: {run_error}"
        )

    body, time_ns = extract_output_body_and_time(output)
    if body is None or time_ns is None:
        return TestResult(
            test_name=spec.name, optimization=optimization, obfuscated=obfuscated,
            run_number=run_number, output_body=body or "", time_ns=time_ns or 0,
            compile_time=compile_time, success=False,
            error_message="Failed to parse output (missing or invalid timing line)"
        )

    return TestResult(
        test_name=spec.name, optimization=optimization, obfuscated=obfuscated,
        run_number=run_number, output_body=body, time_ns=time_ns,
        compile_time=compile_time, success=True
    )


def calculate_summaries(results: List[TestResult]) -> Dict[str, List[TestSummary]]:
    summaries = {}
    grouped = {}
    for result in results:
        if not result.success:
            continue
        key = (result.test_name, result.optimization, result.obfuscated)
        grouped.setdefault(key, []).append(result)

    for (test_name, optimization, obfuscated), group_results in grouped.items():
        avg_time = sum(r.time_ns for r in group_results) / len(group_results)
        real_compile_times = [r.compile_time for r in group_results if r.compile_time > 0]
        avg_compile = (sum(real_compile_times) / len(real_compile_times)) if real_compile_times else 0.0

        outputs = [r.output_body for r in group_results]
        all_match = len(set(outputs)) == 1

        summary = TestSummary(
            test_name=test_name, optimization=optimization, obfuscated=obfuscated,
            avg_time_ns=avg_time, avg_compile_time=avg_compile,
            outputs=outputs, output_match=all_match, all_runs_success=True
        )
        key = f"{test_name}_{optimization}"
        summaries.setdefault(key, []).append(summary)

    return summaries


def _diff_lines(a: str, b: str) -> List[str]:
    lines_a = a.splitlines()
    lines_b = b.splitlines()
    diffs = []
    max_len = max(len(lines_a), len(lines_b))
    for i in range(max_len):
        la = lines_a[i] if i < len(lines_a) else "<missing>"
        lb = lines_b[i] if i < len(lines_b) else "<missing>"
        if la != lb:
            diffs.append(f"  line {i+1}:")
            diffs.append(f"    A : {la}")
            diffs.append(f"    B : {lb}")
    return diffs


def generate_comparison_table(summaries: Dict[str, List[TestSummary]]) -> Tuple[List[List], List[str]]:
    """
    Match is [+] only when *every* successful run of the test
    (all plain runs + all obfuscated runs) produced the identical output body.
    """
    table_data = []
    mismatch_reports = []

    for key, group in sorted(summaries.items()):
        if len(group) != 2:
            continue

        plain = next((s for s in group if not s.obfuscated), None)
        obf = next((s for s in group if s.obfuscated), None)
        if plain is None or obf is None:
            continue

        # Slowdowns
        runtime_slowdown = (obf.avg_time_ns / plain.avg_time_ns) if plain.avg_time_ns > 0 else 0.0
        compile_slowdown = (obf.avg_compile_time / plain.avg_compile_time) if plain.avg_compile_time > 0 else 0.0

        # Collect *all* outputs from this test (plain + obfuscated)
        all_outputs = plain.outputs + obf.outputs
        unique_bodies = set(all_outputs)

        # Strict match: every single run of the test produced the same body
        if len(unique_bodies) == 1:
            match_status = "[+]"
        else:
            # Distinguish internal inconsistency from plain-vs-obf difference
            if not plain.output_match or not obf.output_match:
                match_status = "[-] (inconsistent)"
            else:
                match_status = "[-]"

        table_data.append([
            plain.test_name,
            plain.optimization,
            f"{plain.avg_time_ns/1_000_000:.2f}ms",
            f"{obf.avg_time_ns/1_000_000:.2f}ms",
            f"{runtime_slowdown:.2f}x",
            f"{plain.avg_compile_time:.2f}s",
            f"{obf.avg_compile_time:.2f}s",
            f"{compile_slowdown:.2f}x",
            match_status,
        ])

        # Detailed report when anything differs
        if match_status != "[+]":
            report = [f"=== {plain.test_name} ({plain.optimization}) ==="]
            report.append(f"  Total unique output bodies across ALL runs (plain+obf): {len(unique_bodies)}")

            if not plain.output_match:
                report.append(f"  Plain runs inconsistent: {len(set(plain.outputs))} distinct bodies")
            if not obf.output_match:
                report.append(f"  Obfuscated runs inconsistent: {len(set(obf.outputs))} distinct bodies")

            # Representative plain vs obfuscated
            plain_rep = Counter(plain.outputs).most_common(1)[0][0] if plain.outputs else ""
            obf_rep = Counter(obf.outputs).most_common(1)[0][0] if obf.outputs else ""

            if plain_rep != obf_rep:
                report.append("  Differing lines (most common plain vs most common obfuscated):")
                report.extend(_diff_lines(plain_rep, obf_rep))
            else:
                report.append("  (Most common plain and obfuscated bodies are identical,")
                report.append("   but other runs produced different bodies)")

            # If more than two unique bodies, list a short fingerprint of each
            if len(unique_bodies) > 2:
                report.append("  Unique body fingerprints:")
                for i, body in enumerate(sorted(unique_bodies), 1):
                    preview = body.replace('\n', '\\n')[:80]
                    if len(body) > 80:
                        preview += "..."
                    report.append(f"    [{i}] {preview}")

            mismatch_reports.append("\n".join(report))

    return table_data, mismatch_reports


def print_results_table(table_data: List[List]):
    headers = [
        "Test", "Opt",
        "Plain Time", "Obf Time", "Runtime Slowdown",
        "Plain Compile", "Obf Compile", "Compile Slowdown",
        "Match",
    ]
    print("\n" + tabulate(table_data, headers=headers, tablefmt=OUTPUT_FORMAT))
    print()


def print_statistics(summaries: Dict[str, List[TestSummary]]):
    total_tests = len([s for group in summaries.values() for s in group if s.obfuscated])
    successful_tests = len([s for group in summaries.values() for s in group if s.obfuscated and s.all_runs_success])

    runtime_slowdowns = []
    compile_slowdowns = []
    full_matches = 0
    mismatches = 0

    for group in summaries.values():
        if len(group) != 2:
            continue
        plain = next((s for s in group if not s.obfuscated), None)
        obf = next((s for s in group if s.obfuscated), None)
        if plain is None or obf is None:
            continue

        if plain.avg_time_ns > 0:
            runtime_slowdowns.append(obf.avg_time_ns / plain.avg_time_ns)
        if plain.avg_compile_time > 0:
            compile_slowdowns.append(obf.avg_compile_time / plain.avg_compile_time)

        all_outputs = plain.outputs + obf.outputs
        if len(set(all_outputs)) == 1:
            full_matches += 1
        else:
            mismatches += 1

    total_comparisons = len([g for g in summaries.values() if len(g) == 2])
    avg_runtime = sum(runtime_slowdowns) / len(runtime_slowdowns) if runtime_slowdowns else 0
    avg_compile = sum(compile_slowdowns) / len(compile_slowdowns) if compile_slowdowns else 0
    match_rate = full_matches / total_comparisons if total_comparisons else 0

    print_separator("-")
    print("OVERALL STATISTICS")
    print_separator("-")
    print(f"Total test configurations: {total_tests}")
    print(f"Successful runs: {successful_tests}")
    print(f"Average runtime slowdown: {avg_runtime:.2f}x")
    print(f"Average compile slowdown: {avg_compile:.2f}x")
    print(f"Full output match rate (all plain+obf runs identical): {match_rate:.1%}")
    if mismatches > 0:
        print(f"WARNING: {mismatches} tests where not all outputs matched")
    print_separator()


def main():
    print_header("LeetObfuscator Benchmark Script")

    try:
        test_specs = [normalize_test_entry(e) for e in TEST_FILES]
    except (FileNotFoundError, ValueError) as e:
        print(f"ERROR: {e}")
        sys.exit(1)

    print("Configuration:")
    print(f"  Test entries: {len(test_specs)}")
    for spec in test_specs:
        src_names = [s.name for s in spec.sources]
        if len(src_names) == 1:
            print(f"    - {spec.name} (single: {src_names[0]})")
        else:
            print(f"    - {spec.name} (multi: {', '.join(src_names)})")
    print(f"  Optimization levels: {', '.join(OPTIMIZATION_LEVELS)}")
    print(f"  Runs per configuration: {RUN_COUNT}")
    print(f"  Rebuild per run: {REBUILD_PER_RUN}")
    print(f"  Warmup compilation: {WARMUP_COMPILE}")
    print(f"  Base flags: {BASE_FLAGS}")
    print(f"  Extra flags: {EXTRA_FLAGS}")
    print(f"  Compile timeout: {COMPILE_TIMEOUT}s")
    print(f"  Execute timeout: {EXECUTE_TIMEOUT}s")
    print(f"  Obfuscated compiler: {OBFUSCATED_COMPILER}")
    print(f"  Regular compiler: {REGULAR_COMPILER}")
    print()

    if not OBFUSCATED_COMPILER.exists():
        print(f"WARNING: Obfuscated compiler not found at {OBFUSCATED_COMPILER}")
        print("Skipping obfuscated tests.")
    else:
        print(f"Using obfuscated compiler: {OBFUSCATED_COMPILER}")

    BUILD_DIR.mkdir(exist_ok=True)

    all_results = []
    total_configs = len(test_specs) * len(OPTIMIZATION_LEVELS) * 2
    current_config = 0

    for spec in test_specs:
        for optimization in OPTIMIZATION_LEVELS:
            for obfuscated in [False, True]:
                if obfuscated and not OBFUSCATED_COMPILER.exists():
                    continue

                current_config += 1
                multi_note = f" ({len(spec.sources)} files)" if len(spec.sources) > 1 else ""
                print(f"[{current_config}/{total_configs}] Testing {spec.name}{multi_note} -{optimization} {'(obfuscated)' if obfuscated else '(plain)'}")

                output_name = f"{spec.name}_{optimization}_{'obf' if obfuscated else 'plain'}"
                output_path = BUILD_DIR / output_name
                compiler = str(OBFUSCATED_COMPILER) if obfuscated else REGULAR_COMPILER

                if WARMUP_COMPILE and obfuscated:
                    print(f"  Warmup compilation...")
                    warmup_path = BUILD_DIR / f"{output_name}_warmup"
                    warmup_success, warmup_time, warmup_error = compile_test(
                        spec.sources, warmup_path, str(OBFUSCATED_COMPILER), optimization, obfuscated
                    )
                    if warmup_success:
                        print(f"  Warmup complete: {warmup_time:.2f}s")
                    else:
                        print(f"  Warmup failed: {warmup_error}")

                test_results = []
                compile_time = 0.0

                if not REBUILD_PER_RUN:
                    print(f"  Building once for {RUN_COUNT} runs...")
                    compile_success, compile_time, compile_error = compile_test(
                        spec.sources, output_path, compiler, optimization, obfuscated
                    )
                    if not compile_success:
                        print(f"  Build FAILED - {compile_error}")
                        for run in range(1, RUN_COUNT + 1):
                            test_results.append(TestResult(
                                test_name=spec.name, optimization=optimization, obfuscated=obfuscated,
                                run_number=run, output_body="", time_ns=0,
                                compile_time=compile_time, success=False,
                                error_message=f"Compilation failed: {compile_error}"
                            ))
                        all_results.extend(test_results)
                        continue

                for run in range(1, RUN_COUNT + 1):
                    result = run_single_test(spec, optimization, obfuscated, run, output_path, compiler)
                    if not REBUILD_PER_RUN:
                        result.compile_time = compile_time if run == 1 else 0.0
                    test_results.append(result)

                    if not result.success:
                        print(f"  Run {run}: FAILED - {result.error_message}")
                    else:
                        compile_str = f"{result.compile_time:.2f}s" if result.compile_time > 0 else "cached"
                        print(f"  Run {run}: OK, compile: {compile_str}, run: {result.time_ns/1_000_000:.2f}ms)")

                all_results.extend(test_results)

    print_header("Processing Results")
    summaries = calculate_summaries(all_results)

    print_header("Benchmark Results")
    table_data, mismatch_reports = generate_comparison_table(summaries)

    if table_data:
        print_results_table(table_data)
        print_statistics(summaries)

        if mismatch_reports:
            print_header("Output Differences (all plain + obfuscated runs of each test)")
            for report in mismatch_reports:
                print(report)
                print()
        else:
            print("All compared outputs matched (every run of each test produced identical body).")
    else:
        print("No results to display.")

    print("Cleaning up build artifacts...")
    try:
        shutil.rmtree(BUILD_DIR)
        print("Cleanup complete.")
    except Exception as e:
        print(f"Cleanup failed: {e}")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\nInterrupted by user.")
        sys.exit(1)
    except Exception as e:
        print(f"\nERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)