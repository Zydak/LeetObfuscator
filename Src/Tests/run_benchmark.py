import subprocess
import os
import sys
import re
import time
import shutil
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
from typing import Dict, List, Tuple, Optional, Union
from dataclasses import dataclass, field
from collections import Counter
from tabulate import tabulate
import signal

TEST_FILES = [
    "StdContainersTest.cpp",
    "FloatingPointMathTest.cpp",
    {
        "sources": "BigSignaturesTest.cpp",
        "extra_flags": "-msse2 -mfpmath=sse",
    },
    "BitwiseOperationsTest.cpp",
    "BranchingRecursionTest.cpp",
    "ControlFlowObfuscationTest.cpp",
    #"MultithreadingTest.cpp",
    "PerformanceStressTest.cpp",
    "StringManipulationTest.cpp",
    "ClassesTest.cpp",
    "IndirectCallsTest.cpp",
    "TemplatesTest.cpp",
    "MultiModuleTest",
]

TARGET_ENVIRONMENTS = {
    "linux-x64": {
        "flags": "--target=x86_64-linux-gnu --gcc-toolchain=/usr -O3 -fno-exceptions",
        "run_prefix": "",
        "ext": ""
    },
    # "linux-x86": {
    #     "flags": "--target=i686-linux-gnu --gcc-toolchain=/usr -O3 -fno-exceptions",
    #     "run_prefix": "",
    #     "ext": ""
    # },
    # "linux-x64-noopt": {
    #     "flags": "--target=x86_64-linux-gnu --gcc-toolchain=/usr -O0 -fno-exceptions",
    #     "run_prefix": "",
    #     "ext": ""
    # },
    # "linux-x86-noopt": {
    #     "flags": "--target=i686-linux-gnu --gcc-toolchain=/usr -O0 -fno-exceptions",
    #     "run_prefix": "",
    #     "ext": ""
    # },
    "windows-x64": {
        "flags": "--target=x86_64-w64-mingw32 -O3 -femulated-tls -fno-exceptions -static -static-libgcc -static-libstdc++ -Wl,--start-group -lstdc++ -lwinpthread -Wl,--end-group -s",
        "run_prefix": "wine",
        "ext": ".exe"
    },
    # "windows-x86": {
    #     "flags": "--target=i686-w64-mingw32 -O3 -femulated-tls -fno-exceptions -static -static-libgcc -static-libstdc++ -Wl,--start-group -lstdc++ -lwinpthread -Wl,--end-group -s",
    #     "run_prefix": "wine",
    #     "ext": ".exe"
    # },
    # "windows-x64-noopt": {
    #     "flags": "--target=x86_64-w64-mingw32 -O0 -femulated-tls -fno-exceptions -static -static-libgcc -static-libstdc++ -Wl,--start-group -lstdc++ -lwinpthread -Wl,--end-group -s",
    #     "run_prefix": "wine",
    #     "ext": ".exe"
    # },
    # "windows-x86-noopt": {
    #     "flags": "--target=i686-w64-mingw32 -O0 -femulated-tls -fno-exceptions -static -static-libgcc -static-libstdc++ -Wl,--start-group -lstdc++ -lwinpthread -Wl,--end-group -s",
    #     "run_prefix": "wine",
    #     "ext": ".exe"
    # },
}

RUN_COUNT = 2
REBUILD_PER_RUN = False
WARMUP_COMPILE = False
COMPILE_TIMEOUT = 300
EXECUTE_TIMEOUT = 300

MAX_WORKERS = 0

SCRIPT_DIR = Path(__file__).parent
TESTS_DIR = SCRIPT_DIR
BUILD_DIR = SCRIPT_DIR / "benchmark_build"
OBFUSCATED_COMPILER = SCRIPT_DIR / "../../build/bin/clang++"
REGULAR_COMPILER = "clang++"
OUTPUT_FORMAT = "grid"


@dataclass
class TestSpec:
    name: str
    sources: List[Path]
    extra_flags: str = ""


@dataclass
class TestResult:
    test_name: str
    target: str
    obfuscated: bool
    run_number: int
    output_body: str
    time_ns: int
    compile_time: float
    success: bool
    error_message: str = ""
    binary_size: int = 0


@dataclass
class TestSummary:
    test_name: str
    target: str
    obfuscated: bool
    avg_time_ns: float
    avg_compile_time: float
    avg_binary_size: float = 0.0
    outputs: List[str] = field(default_factory=list)
    output_match: bool = True
    all_runs_success: bool = True
    error_messages: List[str] = field(default_factory=list)



def print_separator(char="=", length=80):
    print(char * length)


def print_header(text):
    print_separator()
    print(f" {text}")
    print_separator()


def _resolve_sources(entry) -> Tuple[List[Path], str]:
    """Resolve sources from a string, list, or dict entry. Returns (sources, extra_flags)."""
    extra_flags = ""

    if isinstance(entry, dict):
        extra_flags = entry.get("extra_flags", "") or ""
        sources_entry = entry.get("sources")
        if sources_entry is None:
            raise ValueError("Dict test entry must contain a 'sources' key")
        entry = sources_entry

    if isinstance(entry, (list, tuple)):
        if not entry:
            raise ValueError("Empty source list in TEST_FILES")
        sources = []
        for s in entry:
            p = TESTS_DIR / s
            if not p.exists():
                raise FileNotFoundError(f"Source not found: {p}")
            sources.append(p.resolve())
        return sources, extra_flags

    entry = str(entry)
    path = TESTS_DIR / entry

    if path.is_dir():
        sources = sorted(path.glob("*.cpp"))
        if not sources:
            raise FileNotFoundError(f"No .cpp files found in directory: {path}")
        return [s.resolve() for s in sources], extra_flags

    if path.is_file() and path.suffix == ".cpp":
        return [path.resolve()], extra_flags

    raise FileNotFoundError(f"Test entry not found or not a .cpp / directory: {path}")


def normalize_test_entry(entry: Union[str, List, Tuple, Dict]) -> TestSpec:
    sources, extra_flags = _resolve_sources(entry)

    if isinstance(entry, dict):
        name_override = entry.get("name")
        if name_override:
            name = name_override
        else:
            main_candidates = [s for s in sources if s.name.lower() == "main.cpp"]
            if main_candidates:
                name = main_candidates[0].parent.name
            else:
                name = sources[0].stem
    else:
        main_candidates = [s for s in sources if s.name.lower() == "main.cpp"]
        if main_candidates:
            name = main_candidates[0].parent.name
        else:
            name = sources[0].stem

    return TestSpec(name=name, sources=sources, extra_flags=extra_flags.strip())


def format_size(num_bytes: float) -> str:
    if num_bytes <= 0:
        return "N/A"
    if num_bytes >= 1024 * 1024:
        return f"{num_bytes / 1024 / 1024:.2f}MB"
    return f"{num_bytes / 1024:.1f}KB"


def extract_output_body_and_time(output: str) -> Tuple[Optional[str], Optional[int]]:
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
                 target: str, obfuscated: bool, extra_flags: str = "") -> Tuple[bool, float, str]:
    
    env_config = TARGET_ENVIRONMENTS[target]
    flags = env_config.get("flags", "")
    if extra_flags:
        flags = f"{flags} {extra_flags}"
    src_args = " ".join(f'"{s}"' for s in sources)
    
    cmd = f'{compiler} {flags} {src_args} -o "{output_path}"'

    start_time = time.time()
    proc = None
    try:
        proc = subprocess.Popen(
            cmd, shell=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            start_new_session=True   # own process group
        )
        try:
            stdout, stderr = proc.communicate(timeout=COMPILE_TIMEOUT)
        except subprocess.TimeoutExpired:
            # Kill the entire process group
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except (ProcessLookupError, PermissionError, OSError):
                pass
            # Reap to avoid zombies
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                pass
            return False, time.time() - start_time, f"Compilation timeout after {COMPILE_TIMEOUT}s"

        compile_time = time.time() - start_time
        if proc.returncode != 0:
            return False, compile_time, stderr
        return True, compile_time, ""
    except Exception as e:
        if proc is not None and proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except (ProcessLookupError, PermissionError, OSError):
                pass
        return False, time.time() - start_time, str(e)


def run_test(executable_path: Path, target: str) -> Tuple[bool, str, str]:
    env_config = TARGET_ENVIRONMENTS[target]
    run_prefix = env_config.get("run_prefix", "")
    
    cmd = f'{run_prefix} "{executable_path}"'.strip()

    env = os.environ.copy()
    
    if "wine" in run_prefix:
        env["WINEDLLOVERRIDES"] = "winedbg.exe=d,mscoree=d,mshtml=d"
        env["WINEDEBUG"] = "-all"

    proc = None
    try:
        proc = subprocess.Popen(
            cmd, shell=True,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            env=env, start_new_session=True
        )
        try:
            stdout, stderr = proc.communicate(timeout=EXECUTE_TIMEOUT)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except (ProcessLookupError, PermissionError, OSError):
                pass
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                pass
            return False, "", f"Execution timeout after {EXECUTE_TIMEOUT}s"

        if proc.returncode != 0:
            return False, "", stderr
        return True, stdout, ""
    except Exception as e:
        if proc is not None and proc.poll() is None:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except (ProcessLookupError, PermissionError, OSError):
                pass
        return False, "", str(e)

def run_single_test(spec: TestSpec, target: str, obfuscated: bool,
                    run_number: int, output_path: Path, compiler: str) -> TestResult:
    compile_time = 0.0
    if REBUILD_PER_RUN or not output_path.exists():
        compile_success, compile_time, compile_error = compile_test(
            spec.sources, output_path, compiler, target, obfuscated, extra_flags=spec.extra_flags
        )
        if not compile_success:
            return TestResult(
                test_name=spec.name, target=target, obfuscated=obfuscated,
                run_number=run_number, output_body="", time_ns=0,
                compile_time=compile_time, success=False,
                error_message=f"Compilation failed: {compile_error}",
                binary_size=0
            )

    try:
        binary_size = output_path.stat().st_size
    except OSError:
        binary_size = 0

    run_success, output, run_error = run_test(output_path, target)
    if not run_success:
        return TestResult(
            test_name=spec.name, target=target, obfuscated=obfuscated,
            run_number=run_number, output_body="", time_ns=0,
            compile_time=compile_time, success=False,
            error_message=f"Execution failed: {run_error}",
            binary_size=binary_size
        )

    body, time_ns = extract_output_body_and_time(output)
    if body is None or time_ns is None:
        return TestResult(
            test_name=spec.name, target=target, obfuscated=obfuscated,
            run_number=run_number, output_body=body or "", time_ns=time_ns or 0,
            compile_time=compile_time, success=False,
            error_message="Failed to parse output (missing or invalid timing line)",
            binary_size=binary_size
        )

    return TestResult(
        test_name=spec.name, target=target, obfuscated=obfuscated,
        run_number=run_number, output_body=body, time_ns=time_ns,
        compile_time=compile_time, success=True,
        binary_size=binary_size
    )


def run_one_config(spec: TestSpec, target: str, obfuscated: bool,
                   config: dict, job_index: int, total_jobs: int) -> Tuple[List[TestResult], str]:
    """
    Execute a single (test, target, plain/obf) configuration.
    Returns (list of TestResult, log text for this job).
    Designed to be called from a worker thread.
    """
    log_lines: List[str] = []
    multi_note = f" ({len(spec.sources)} files)" if len(spec.sources) > 1 else ""
    extra_note = f" [+{spec.extra_flags}]" if spec.extra_flags else ""
    label = f"{spec.name}{multi_note}{extra_note} [{target}] {'(obfuscated)' if obfuscated else '(plain)'}"
    log_lines.append(f"[{job_index}/{total_jobs}] Testing {label}")

    output_name = f"{spec.name}_{target}_{'obf' if obfuscated else 'plain'}{config.get('ext', '')}"
    output_path = BUILD_DIR / output_name
    compiler = str(OBFUSCATED_COMPILER) if obfuscated else REGULAR_COMPILER

    if WARMUP_COMPILE and obfuscated:
        log_lines.append("  Warmup compilation...")
        warmup_path = BUILD_DIR / f"{output_name}_warmup{config.get('ext', '')}"
        warmup_success, warmup_time, warmup_error = compile_test(
            spec.sources, warmup_path, str(OBFUSCATED_COMPILER), target, obfuscated,
            extra_flags=spec.extra_flags
        )
        if warmup_success:
            log_lines.append(f"  Warmup complete: {warmup_time:.2f}s")
        else:
            log_lines.append(f"  Warmup failed: {warmup_error}")

    test_results: List[TestResult] = []
    compile_time = 0.0

    if not REBUILD_PER_RUN:
        log_lines.append(f"  Building once for {RUN_COUNT} runs...")
        compile_success, compile_time, compile_error = compile_test(
            spec.sources, output_path, compiler, target, obfuscated,
            extra_flags=spec.extra_flags
        )
        if not compile_success:
            log_lines.append(f"  Build FAILED - {compile_error}")
            for run in range(1, RUN_COUNT + 1):
                test_results.append(TestResult(
                    test_name=spec.name, target=target, obfuscated=obfuscated,
                    run_number=run, output_body="", time_ns=0,
                    compile_time=compile_time, success=False,
                    error_message=f"Compilation failed: {compile_error}"
                ))
            return test_results, "\n".join(log_lines)

    for run in range(1, RUN_COUNT + 1):
        result = run_single_test(spec, target, obfuscated, run, output_path, compiler)
        if not REBUILD_PER_RUN:
            result.compile_time = compile_time if run == 1 else 0.0
        test_results.append(result)

        if not result.success:
            log_lines.append(f"  Run {run}: FAILED - {result.error_message}")
        else:
            compile_str = f"{result.compile_time:.2f}s" if result.compile_time > 0 else "cached"
            log_lines.append(f"  Run {run}: OK, compile: {compile_str}, run: {result.time_ns/1_000_000:.2f}ms")

    return test_results, "\n".join(log_lines)


def calculate_summaries(results: List[TestResult]) -> Dict[str, List[TestSummary]]:
    summaries = {}
    grouped = {}
    
    for result in results:
        key = (result.test_name, result.target, result.obfuscated)
        grouped.setdefault(key, []).append(result)

    for (test_name, target, obfuscated), group_results in grouped.items():
        successful_runs = [r for r in group_results if r.success]
        all_success = len(successful_runs) == len(group_results)
        errors = list(set([r.error_message for r in group_results if not r.success]))

        if successful_runs:
            avg_time = sum(r.time_ns for r in successful_runs) / len(successful_runs)
            outputs = [r.output_body for r in successful_runs]
            all_match = len(set(outputs)) == 1
        else:
            avg_time = 0.0
            outputs = []
            all_match = False

        real_compile_times = [r.compile_time for r in group_results if r.compile_time > 0]
        avg_compile = (sum(real_compile_times) / len(real_compile_times)) if real_compile_times else 0.0

        real_binary_sizes = [r.binary_size for r in group_results if r.binary_size > 0]
        avg_binary_size = (sum(real_binary_sizes) / len(real_binary_sizes)) if real_binary_sizes else 0.0

        summary = TestSummary(
            test_name=test_name, target=target, obfuscated=obfuscated,
            avg_time_ns=avg_time, avg_compile_time=avg_compile,
            avg_binary_size=avg_binary_size,
            outputs=outputs, output_match=all_match, all_runs_success=all_success,
            error_messages=errors
        )
        key = f"{test_name}_{target}"
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
    table_data = []
    mismatch_reports = []

    for key, group in sorted(summaries.items()):
        if len(group) != 2:
            continue

        plain = next((s for s in group if not s.obfuscated), None)
        obf = next((s for s in group if s.obfuscated), None)
        if plain is None or obf is None:
            continue

        plain_time_str = f"{plain.avg_time_ns/1_000_000:.2f}ms" if plain.all_runs_success else "FAIL"
        obf_time_str = f"{obf.avg_time_ns/1_000_000:.2f}ms" if obf.all_runs_success else "FAIL"

        if plain.all_runs_success and obf.all_runs_success:
            runtime_slowdown = f"{(obf.avg_time_ns / plain.avg_time_ns):.2f}x" if plain.avg_time_ns > 0 else "N/A"
            compile_slowdown = f"{(obf.avg_compile_time / plain.avg_compile_time):.2f}x" if plain.avg_compile_time > 0 else "N/A"
        else:
            runtime_slowdown = "N/A"
            compile_slowdown = "N/A"

        plain_comp_str = f"{plain.avg_compile_time:.2f}s"
        obf_comp_str = f"{obf.avg_compile_time:.2f}s"

        plain_size_str = format_size(plain.avg_binary_size) if plain.all_runs_success else "FAIL"
        obf_size_str = format_size(obf.avg_binary_size) if obf.all_runs_success else "FAIL"

        if (plain.all_runs_success and obf.all_runs_success
                and plain.avg_binary_size > 0 and obf.avg_binary_size > 0):
            size_ratio = f"{(obf.avg_binary_size / plain.avg_binary_size):.2f}x"
        else:
            size_ratio = "N/A"

        if not plain.all_runs_success or not obf.all_runs_success:
            match_status = "FAIL"
        else:
            all_outputs = plain.outputs + obf.outputs
            unique_bodies = set(all_outputs)
            if len(unique_bodies) == 1:
                match_status = "[+]"
            else:
                if not plain.output_match or not obf.output_match:
                    match_status = "[-] (inconsistent)"
                else:
                    match_status = "[-]"

        table_data.append([
            plain.test_name,
            plain.target,
            plain_time_str,
            obf_time_str,
            runtime_slowdown,
            plain_comp_str,
            obf_comp_str,
            compile_slowdown,
            plain_size_str,
            obf_size_str,
            size_ratio,
            match_status,
        ])

        if match_status == "FAIL":
            report = [f"=== {plain.test_name} ({plain.target}) [FAILED] ==="]
            if not plain.all_runs_success:
                report.append("  Plain build/run failed:")
                for err in plain.error_messages:
                    report.append(f"    - {err.strip()}")
            if not obf.all_runs_success:
                report.append("  Obfuscated build/run failed:")
                for err in obf.error_messages:
                    report.append(f"    - {err.strip()}")
            mismatch_reports.append("\n".join(report))
            
        elif match_status != "[+]":
            report = [f"=== {plain.test_name} ({plain.target}) [MISMATCH] ==="]
            report.append(f"  Total unique output bodies across ALL runs (plain+obf): {len(unique_bodies)}")

            if not plain.output_match:
                report.append(f"  Plain runs inconsistent: {len(set(plain.outputs))} distinct bodies")
            if not obf.output_match:
                report.append(f"  Obfuscated runs inconsistent: {len(set(obf.outputs))} distinct bodies")

            plain_rep = Counter(plain.outputs).most_common(1)[0][0] if plain.outputs else ""
            obf_rep = Counter(obf.outputs).most_common(1)[0][0] if obf.outputs else ""

            if plain_rep != obf_rep:
                report.append("  Differing lines (most common plain vs most common obfuscated):")
                report.extend(_diff_lines(plain_rep, obf_rep))
            else:
                report.append("  (Most common plain and obfuscated bodies are identical,")
                report.append("   but other runs produced different bodies)")

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
        "Test", "Target",
        "Plain Time", "Obf Time", "Runtime Slowdown",
        "Plain Compile", "Obf Compile", "Compile Slowdown",
        "Plain Size", "Obf Size", "Size Ratio",
        "Match",
    ]
    print("\n" + tabulate(table_data, headers=headers, tablefmt=OUTPUT_FORMAT))
    print()


def print_statistics(summaries: Dict[str, List[TestSummary]]):
    total_comparisons = len([g for g in summaries.values() if len(g) == 2])
    failed_comparisons = 0
    full_matches = 0
    mismatches = 0
    
    runtime_slowdowns = []
    compile_slowdowns = []
    size_ratios = []

    for group in summaries.values():
        if len(group) != 2:
            continue
        plain = next((s for s in group if not s.obfuscated), None)
        obf = next((s for s in group if s.obfuscated), None)
        
        if not plain.all_runs_success or not obf.all_runs_success:
            failed_comparisons += 1
            continue

        if plain.avg_time_ns > 0:
            runtime_slowdowns.append(obf.avg_time_ns / plain.avg_time_ns)
        if plain.avg_compile_time > 0:
            compile_slowdowns.append(obf.avg_compile_time / plain.avg_compile_time)
        if plain.avg_binary_size > 0 and obf.avg_binary_size > 0:
            size_ratios.append(obf.avg_binary_size / plain.avg_binary_size)

        all_outputs = plain.outputs + obf.outputs
        if len(set(all_outputs)) == 1:
            full_matches += 1
        else:
            mismatches += 1

    valid_comparisons = total_comparisons - failed_comparisons
    avg_runtime = sum(runtime_slowdowns) / len(runtime_slowdowns) if runtime_slowdowns else 0
    avg_compile = sum(compile_slowdowns) / len(compile_slowdowns) if compile_slowdowns else 0
    avg_size = sum(size_ratios) / len(size_ratios) if size_ratios else 0
    match_rate = (full_matches / valid_comparisons) if valid_comparisons else 0

    print_separator("-")
    print("OVERALL STATISTICS")
    print_separator("-")
    print(f"Total test configurations: {total_comparisons}")
    print(f"Failed configurations (build or run failed): {failed_comparisons}")
    print(f"Successful comparisons: {valid_comparisons}")
    
    if valid_comparisons > 0:
        print(f"Average runtime slowdown: {avg_runtime:.2f}x")
        print(f"Average compile slowdown: {avg_compile:.2f}x")
        if size_ratios:
            print(f"Average binary size increase: {avg_size:.2f}x")
        print(f"Full output match rate (among successful): {match_rate:.1%}")
    if mismatches > 0:
        print(f"WARNING: {mismatches} tests where outputs did not match")
    print_separator()


def main():
    print_header("LeetObfuscator Benchmark Script")

    try:
        test_specs = [normalize_test_entry(e) for e in TEST_FILES]
    except (FileNotFoundError, ValueError) as e:
        print(f"ERROR: {e}")
        sys.exit(1)

    obfuscated_available = OBFUSCATED_COMPILER.exists()

    jobs = []
    for spec in test_specs:
        for target, config in TARGET_ENVIRONMENTS.items():
            for obfuscated in [False, True]:
                if obfuscated and not obfuscated_available:
                    continue
                jobs.append((spec, target, obfuscated, config))

    total_jobs = len(jobs)

    if MAX_WORKERS == 0:
        workers = min(os.cpu_count() or 4, total_jobs) if total_jobs > 0 else 1
    else:
        workers = max(1, MAX_WORKERS)
    workers = min(workers, total_jobs) if total_jobs > 0 else 1

    print("Configuration:")
    print(f"  Test entries: {len(test_specs)}")
    for spec in test_specs:
        extra = f"  [extra: {spec.extra_flags}]" if spec.extra_flags else ""
        print(f"    - {spec.name}{extra}")
    print(f"  Targets: {', '.join(TARGET_ENVIRONMENTS.keys())}")
    print(f"  Total configurations: {total_jobs}")
    print(f"  Parallel workers: {workers}" + (" (auto)" if MAX_WORKERS == 0 else ""))
    print(f"  Runs per configuration: {RUN_COUNT}")
    print(f"  Rebuild per run: {REBUILD_PER_RUN}")
    print(f"  Warmup compilation: {WARMUP_COMPILE}")
    print(f"  Compile timeout: {COMPILE_TIMEOUT}s")
    print(f"  Execute timeout: {EXECUTE_TIMEOUT}s")
    print(f"  Obfuscated compiler: {OBFUSCATED_COMPILER}")
    print(f"  Regular compiler: {REGULAR_COMPILER}")
    print()

    if not obfuscated_available:
        print(f"WARNING: Obfuscated compiler not found at {OBFUSCATED_COMPILER}")
        print("Skipping obfuscated tests.")
    else:
        print(f"Using obfuscated compiler: {OBFUSCATED_COMPILER}")

    BUILD_DIR.mkdir(exist_ok=True)

    all_results: List[TestResult] = []
    print_lock = threading.Lock()
    completed = 0

    def _run_and_report(job_index: int, job):
        nonlocal completed
        spec, target, obfuscated, config = job
        results, log = run_one_config(spec, target, obfuscated, config, job_index, total_jobs)
        with print_lock:
            completed += 1
            print(log)
            print(f"  --> finished ({completed}/{total_jobs})")
            print()
        return results

    if workers == 1:
        for i, job in enumerate(jobs, 1):
            all_results.extend(_run_and_report(i, job))
    else:
        print(f"Running {total_jobs} configurations with {workers} parallel workers...\n")
        with ThreadPoolExecutor(max_workers=workers) as executor:
            futures = {
                executor.submit(_run_and_report, i, job): job
                for i, job in enumerate(jobs, 1)
            }
            for future in as_completed(futures):
                try:
                    all_results.extend(future.result())
                except Exception as e:
                    job = futures[future]
                    with print_lock:
                        print(f"ERROR in worker for {job[0].name}/{job[1]}: {e}")
                        import traceback
                        traceback.print_exc()

    print_header("Processing Results")
    summaries = calculate_summaries(all_results)

    print_header("Benchmark Results")
    table_data, mismatch_reports = generate_comparison_table(summaries)

    if table_data:
        print_results_table(table_data)
        print_statistics(summaries)

        if mismatch_reports:
            print_header("Issues & Output Differences (Failures / Mismatches)")
            for report in mismatch_reports:
                print(report)
                print()
        else:
            print("All compared tests executed successfully and outputs matched.")
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
