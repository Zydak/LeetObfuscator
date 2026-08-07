#!/usr/bin/env python3
"""
Benchmark script for LeetObfuscator tests.
Compiles and runs tests with and without obfuscation, comparing checksums and timing.
"""

import subprocess
import os
import sys
import re
import time
import shutil
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass
from tabulate import tabulate

# ==================== CONFIGURATION ====================

# Test files to benchmark
TEST_FILES = [
    "BigSignaturesTest.cpp",
    "BranchingRecursionTest.cpp",
    "MultithreadingTest.cpp",
    "PerformanceStressTest.cpp",
    "StdContainersTest.cpp",
    "ControlFlowObfuscationTest.cpp",
    "StringManipulationTest.cpp",
    "FloatingPointMathTest.cpp",
    "BitwiseOperationsTest.cpp",
]

# Optimization levels to test
OPTIMIZATION_LEVELS = ["O3"]

# Number of runs per test per optimization level
RUN_COUNT = 100

# Rebuild for each run (set to False to build once and run multiple times)
REBUILD_PER_RUN = False

# Add warmup compilation to handle first-run slowness
WARMUP_COMPILE = False

# Base compiler flags
BASE_FLAGS = "-fno-exceptions"

# Additional compiler flags (can be customized)
EXTRA_FLAGS = ""

# Compilation timeout in seconds
COMPILE_TIMEOUT = 300  # 5 minutes

# Execution timeout in seconds  
EXECUTE_TIMEOUT = 120  # 2 minutes

# Paths
SCRIPT_DIR = Path(__file__).parent
TESTS_DIR = SCRIPT_DIR
BUILD_DIR = SCRIPT_DIR / "benchmark_build"
OBFUSCATED_COMPILER = SCRIPT_DIR / "../../build/bin/clang++"
REGULAR_COMPILER = "clang++"

# Output format
OUTPUT_FORMAT = "grid"  # Options: "simple", "grid", "pipe", "html", "latex"

# ==================== DATA STRUCTURES ====================

@dataclass
class TestResult:
    """Stores results for a single test run."""
    test_name: str
    optimization: str
    obfuscated: bool
    run_number: int
    checksum: str
    time_ns: int
    compile_time: float
    success: bool
    error_message: str = ""

@dataclass
class TestSummary:
    """Summary statistics for a test configuration."""
    test_name: str
    optimization: str
    obfuscated: bool
    avg_time_ns: float
    avg_compile_time: float
    checksums: list
    checksum_match: bool = True
    all_runs_success: bool = True

# ==================== UTILITY FUNCTIONS ====================

def print_separator(char="=", length=80):
    """Print a separator line."""
    print(char * length)

def print_header(text):
    """Print a formatted header."""
    print_separator()
    print(f" {text}")
    print_separator()

def extract_checksum_and_time(output: str) -> Tuple[Optional[str], Optional[int]]:
    """
    Extract checksum and timing from test output.
    Expected format:
    CHECKSUM: 0x6d0ac96e97e01af7
    1182504
    """
    lines = output.strip().split('\n')
    
    if len(lines) < 2:
        return None, None
    
    # Extract checksum from second-to-last line
    checksum_match = re.search(r'CHECKSUM: (0x[0-9a-fA-F]+)', lines[-2])
    checksum = checksum_match.group(1) if checksum_match else None
    
    # Extract time from last line
    try:
        time_ns = int(lines[-1].strip())
    except (ValueError, IndexError):
        time_ns = None
    
    return checksum, time_ns

def compile_test(test_file: Path, output_path: Path, compiler: str, 
                 optimization: str, obfuscated: bool) -> Tuple[bool, float, str]:
    """
    Compile a test file.
    Returns (success, compile_time, error_message).
    """
    flags = f"-{optimization} {BASE_FLAGS} {EXTRA_FLAGS}"
    cmd = f"{compiler} {test_file} -o {output_path} {flags}"
    
    start_time = time.time()
    try:
        result = subprocess.run(
            cmd,
            shell=True,
            capture_output=True,
            text=True,
            timeout=COMPILE_TIMEOUT
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
    """
    Run a compiled test.
    Returns (success, output, error_message).
    """
    try:
        result = subprocess.run(
            str(executable_path),
            capture_output=True,
            text=True,
            timeout=EXECUTE_TIMEOUT
        )
        
        if result.returncode != 0:
            return False, "", result.stderr
        
        return True, result.stdout, ""
    except subprocess.TimeoutExpired:
        return False, "", f"Execution timeout after {EXECUTE_TIMEOUT}s"
    except Exception as e:
        return False, "", str(e)

def run_single_test(test_file: str, optimization: str, obfuscated: bool, 
                   run_number: int, output_path: Path, compiler: str) -> TestResult:
    """
    Compile and run a single test.
    """
    test_name = test_file.replace(".cpp", "")
    test_path = TESTS_DIR / test_file
    
    # Compile if rebuild is enabled or output doesn't exist
    compile_time = 0.0
    if REBUILD_PER_RUN or not output_path.exists():
        compile_success, compile_time, compile_error = compile_test(
            test_path, output_path, compiler, optimization, obfuscated
        )
        
        if not compile_success:
            return TestResult(
                test_name=test_name,
                optimization=optimization,
                obfuscated=obfuscated,
                run_number=run_number,
                checksum="",
                time_ns=0,
                compile_time=compile_time,
                success=False,
                error_message=f"Compilation failed: {compile_error}"
            )
    else:
        compile_success = True
    
    # Run
    run_success, output, run_error = run_test(output_path)
    
    if not run_success:
        return TestResult(
            test_name=test_name,
            optimization=optimization,
            obfuscated=obfuscated,
            run_number=run_number,
            checksum="",
            time_ns=0,
            compile_time=compile_time,
            success=False,
            error_message=f"Execution failed: {run_error}"
        )
    
    # Extract results
    checksum, time_ns = extract_checksum_and_time(output)
    
    if checksum is None or time_ns is None:
        return TestResult(
            test_name=test_name,
            optimization=optimization,
            obfuscated=obfuscated,
            run_number=run_number,
            checksum=checksum or "",
            time_ns=time_ns or 0,
            compile_time=compile_time,
            success=False,
            error_message="Failed to parse output"
        )
    
    return TestResult(
        test_name=test_name,
        optimization=optimization,
        obfuscated=obfuscated,
        run_number=run_number,
        checksum=checksum,
        time_ns=time_ns,
        compile_time=compile_time,
        success=True
    )

def calculate_summaries(results: List[TestResult]) -> Dict[str, List[TestSummary]]:
    """
    Calculate summary statistics for each test configuration.
    """
    summaries = {}
    
    # Group results by test name, optimization, and obfuscation status
    grouped = {}
    for result in results:
        if not result.success:
            continue
        
        key = (result.test_name, result.optimization, result.obfuscated)
        if key not in grouped:
            grouped[key] = []
        grouped[key].append(result)
    
    # Calculate summaries
    for (test_name, optimization, obfuscated), group_results in grouped.items():
        avg_time = sum(r.time_ns for r in group_results) / len(group_results)
        avg_compile = sum(r.compile_time for r in group_results) / len(group_results)
        
        # Collect all checksums
        checksums = [r.checksum for r in group_results]
        
        # Check if all checksums match
        all_match = len(set(checksums)) == 1
        
        summary = TestSummary(
            test_name=test_name,
            optimization=optimization,
            obfuscated=obfuscated,
            avg_time_ns=avg_time,
            avg_compile_time=avg_compile,
            checksums=checksums,
            checksum_match=all_match,
            all_runs_success=True
        )
        
        key = f"{test_name}_{optimization}"
        if key not in summaries:
            summaries[key] = []
        summaries[key].append(summary)
    
    return summaries

def generate_comparison_table(summaries: Dict[str, List[TestSummary]]) -> List[List]:
    """
    Generate comparison table data.
    """
    table_data = []
    
    for key, group in sorted(summaries.items()):
        if len(group) != 2:  # Need both obfuscated and non-obfuscated
            continue
        
        plain = next((s for s in group if not s.obfuscated), None)
        obf = next((s for s in group if s.obfuscated), None)
        
        if plain is None or obf is None:
            continue
        
        # Calculate slowdowns
        if plain.avg_time_ns > 0:
            runtime_slowdown = obf.avg_time_ns / plain.avg_time_ns
        else:
            runtime_slowdown = 0.0
        
        if plain.avg_compile_time > 0:
            compile_slowdown = obf.avg_compile_time / plain.avg_compile_time
        else:
            compile_slowdown = 0.0
        
        # Checksum match - compare all runs from both plain and obfuscated
        plain_checksums = set(plain.checksums)
        obf_checksums = set(obf.checksums)
        
        # Check if all checksums match across both configurations
        all_checksums = plain_checksums | obf_checksums
        all_match = len(all_checksums) == 1
        
        # Determine match status
        if not plain.checksum_match or not obf.checksum_match:
            checksum_match = "✗ (inconsistent)"
        elif all_match:
            checksum_match = "✓"
        else:
            checksum_match = "✗"
        
        # Show representative checksum
        rep_checksum = list(all_checksums)[0] if all_checksums else "N/A"
        checksum_display = rep_checksum[:16] + "..." if len(rep_checksum) > 16 else rep_checksum
        
        # If inconsistent, show count of unique checksums
        if len(all_checksums) > 1:
            checksum_display += f" ({len(all_checksums)} variants)"
        
        table_data.append([
            plain.test_name,
            plain.optimization,
            f"{plain.avg_time_ns/1_000_000:.2f}ms",
            f"{obf.avg_time_ns/1_000_000:.2f}ms",
            f"{runtime_slowdown:.2f}x",
            f"{plain.avg_compile_time:.2f}s",
            f"{obf.avg_compile_time:.2f}s",
            f"{compile_slowdown:.2f}x",
            checksum_match,
            checksum_display
        ])
    
    return table_data

def print_results_table(table_data: List[List]):
    """
    Print results in a formatted table.
    """
    headers = [
        "Test",
        "Opt",
        "Plain Time",
        "Obf Time",
        "Runtime Slowdown",
        "Plain Compile",
        "Obf Compile",
        "Compile Slowdown",
        "Match",
        "Checksums"
    ]
    
    print("\n" + tabulate(table_data, headers=headers, tablefmt=OUTPUT_FORMAT))
    print()

def print_statistics(summaries: Dict[str, List[TestSummary]]):
    """
    Print overall statistics.
    """
    total_tests = len([s for group in summaries.values() for s in group if s.obfuscated])
    successful_tests = len([s for group in summaries.values() for s in group if s.obfuscated and s.all_runs_success])
    
    # Calculate average slowdowns
    runtime_slowdowns = []
    compile_slowdowns = []
    checksum_matches = 0
    inconsistent_checksums = 0
    
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
        
        # Check if all checksums match across all runs
        plain_checksums = set(plain.checksums)
        obf_checksums = set(obf.checksums)
        all_checksums = plain_checksums | obf_checksums
        
        if len(all_checksums) == 1:
            checksum_matches += 1
        else:
            inconsistent_checksums += 1
    
    total_comparisons = len([g for g in summaries.values() if len(g) == 2])
    avg_runtime_slowdown = sum(runtime_slowdowns) / len(runtime_slowdowns) if runtime_slowdowns else 0
    avg_compile_slowdown = sum(compile_slowdowns) / len(compile_slowdowns) if compile_slowdowns else 0
    checksum_match_rate = checksum_matches / total_comparisons if total_comparisons > 0 else 0
    
    print_separator("-")
    print("OVERALL STATISTICS")
    print_separator("-")
    print(f"Total test configurations: {total_tests}")
    print(f"Successful runs: {successful_tests}")
    print(f"Average runtime slowdown: {avg_runtime_slowdown:.2f}x")
    print(f"Average compile slowdown: {avg_compile_slowdown:.2f}x")
    print(f"Checksum match rate: {checksum_match_rate:.1%}")
    if inconsistent_checksums > 0:
        print(f"WARNING: {inconsistent_checksums} tests with inconsistent checksums")
        print("Tests with inconsistent checksums:")
        for key, group in summaries.items():
            if len(group) != 2:
                continue
            plain = next((s for s in group if not s.obfuscated), None)
            obf = next((s for s in group if s.obfuscated), None)
            if plain is None or obf is None:
                continue
            plain_checksums = set(plain.checksums)
            obf_checksums = set(obf.checksums)
            all_checksums = plain_checksums | obf_checksums
            if len(all_checksums) > 1:
                print(f"  - {plain.test_name} ({plain.optimization}): {len(all_checksums)} different checksums")
    print_separator()

def main():
    """Main entry point."""
    print_header("LeetObfuscator Benchmark Script")
    
    # Print configuration
    print("Configuration:")
    print(f"  Test files: {len(TEST_FILES)}")
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
    
    # Check if obfuscated compiler exists
    if not OBFUSCATED_COMPILER.exists():
        print(f"WARNING: Obfuscated compiler not found at {OBFUSCATED_COMPILER}")
        print("Skipping obfuscated tests.")
        # Continue with plain tests only
    else:
        print(f"Using obfuscated compiler: {OBFUSCATED_COMPILER}")
    
    # Create build directory
    BUILD_DIR.mkdir(exist_ok=True)
    
    # Check if test files exist
    missing_files = [f for f in TEST_FILES if not (TESTS_DIR / f).exists()]
    if missing_files:
        print(f"ERROR: Missing test files: {', '.join(missing_files)}")
        sys.exit(1)
    
    # Run benchmarks
    all_results = []
    total_configs = len(TEST_FILES) * len(OPTIMIZATION_LEVELS) * 2  # *2 for obfuscated/plain
    current_config = 0
    
    for test_file in TEST_FILES:
        for optimization in OPTIMIZATION_LEVELS:
            for obfuscated in [False, True]:
                # Skip obfuscated if compiler not found
                if obfuscated and not OBFUSCATED_COMPILER.exists():
                    continue
                
                current_config += 1
                print(f"[{current_config}/{total_configs}] Testing {test_file} -{optimization} {'(obfuscated)' if obfuscated else '(plain)'}")
                
                test_name = test_file.replace(".cpp", "")
                # Create output filename (shared across runs if not rebuilding)
                output_name = f"{test_name}_{optimization}_{'obf' if obfuscated else 'plain'}"
                output_path = BUILD_DIR / output_name
                
                # Select compiler
                compiler = OBFUSCATED_COMPILER if obfuscated else REGULAR_COMPILER
                
                # Warmup compilation if enabled
                if WARMUP_COMPILE and obfuscated:
                    print(f"  Warmup compilation...")
                    warmup_path = BUILD_DIR / f"{output_name}_warmup"
                    warmup_success, warmup_time, warmup_error = compile_test(
                        TESTS_DIR / test_file, warmup_path, 
                        OBFUSCATED_COMPILER, optimization, obfuscated
                    )
                    if warmup_success:
                        print(f"  Warmup complete: {warmup_time:.2f}s")
                    else:
                        print(f"  Warmup failed: {warmup_error}")
                
                test_results = []
                compile_time = 0.0
                
                # Compile once if not rebuilding per run
                if not REBUILD_PER_RUN:
                    print(f"  Building once for {RUN_COUNT} runs...")
                    compile_success, compile_time, compile_error = compile_test(
                        TESTS_DIR / test_file, output_path, compiler, optimization, obfuscated
                    )
                    
                    if not compile_success:
                        print(f"  Build FAILED - {compile_error}")
                        # Add failure result for all runs
                        for run in range(1, RUN_COUNT + 1):
                            test_results.append(TestResult(
                                test_name=test_name,
                                optimization=optimization,
                                obfuscated=obfuscated,
                                run_number=run,
                                checksum="",
                                time_ns=0,
                                compile_time=compile_time,
                                success=False,
                                error_message=f"Compilation failed: {compile_error}"
                            ))
                        all_results.extend(test_results)
                        continue
                
                for run in range(1, RUN_COUNT + 1):
                    result = run_single_test(test_file, optimization, obfuscated, run, output_path, compiler)
                    
                    # Use the single compile time for all runs when not rebuilding
                    if not REBUILD_PER_RUN:
                        result.compile_time = compile_time if run == 1 else 0.0
                    
                    test_results.append(result)
                    
                    if not result.success:
                        print(f"  Run {run}: FAILED - {result.error_message}")
                    else:
                        compile_str = f"{result.compile_time:.2f}s" if result.compile_time > 0 else "cached"
                        print(f"  Run {run}: OK (checksum: {result.checksum[:16]}, compile time: {compile_str}, run time: {result.time_ns/1_000_000:.2f}ms)")
                
                all_results.extend(test_results)
    
    # Calculate summaries
    print_header("Processing Results")
    summaries = calculate_summaries(all_results)
    
    # Generate and print comparison table
    print_header("Benchmark Results")
    table_data = generate_comparison_table(summaries)
    
    if table_data:
        print_results_table(table_data)
        print_statistics(summaries)
    else:
        print("No results to display.")
    
    # Cleanup
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
