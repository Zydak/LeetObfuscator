# LeetObfuscator
```text
██╗     ███████╗███████╗████████╗
██║     ██╔════╝██╔════╝╚══██╔══╝
██║     █████╗  █████╗     ██║
██║     ██╔══╝  ██╔══╝     ██║
███████╗███████╗███████╗   ██║
╚══════╝╚══════╝╚══════╝   ╚═╝

 ██████╗ ██████╗ ███████╗██╗   ██╗███████╗ ██████╗ █████╗ ████████╗ ██████╗ ██████╗
██╔═══██╗██╔══██╗██╔════╝██║   ██║██╔════╝██╔════╝██╔══██╗╚══██╔══╝██╔═══██╗██╔══██╗
██║   ██║██████╔╝█████╗  ██║   ██║███████╗██║     ███████║   ██║   ██║   ██║██████╔╝
██║   ██║██╔══██╗██╔══╝  ██║   ██║╚════██║██║     ██╔══██║   ██║   ██║   ██║██╔══██╗
╚██████╔╝██████╔╝██║     ╚██████╔╝███████║╚██████╗██║  ██║   ██║   ╚██████╔╝██║  ██║
 ╚═════╝ ╚═════╝ ╚═╝      ╚═════╝ ╚══════╝ ╚═════╝╚═╝  ╚═╝   ╚═╝    ╚═════╝ ╚═╝  ╚═╝

A simple obfuscator for C/C++ x64 and x86
```

This is made as an LLVM fork, so you need the actual source to obfuscate. It's not an arbitrary executable obfuscator.

If you want to check out the results I've made and obfuscated 2 crackmes with this.
- Crackme1 - very simple, a single xor of the user input and compare with the key. Without obfuscation it's 5 minutes to crack.
- Crackme2 - A little bit more complex one. If you're done with 1 feel free to try it too.

## Building

### Linux

Requirements:

- CMake
- Ninja
- Clang

```bash
git clone https://github.com/<yourname>/LeetObfuscator.git --recursive
cd LeetObfuscator

mkdir build
cd build

cmake ../leet-llvm-project/llvm -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DLLVM_USE_LINKER=mold -DLLVM_USE_SPLIT_DWARF=ON -DLLVM_ENABLE_ASSERTIONS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLLVM_ENABLE_PROJECTS=clang -DLLVM_TARGETS_TO_BUILD=X86 -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
ninja clang
```
the compiler will be inside `build/bin/`

### Windows
Building from Windows is currently not supported. But there is cross compiled binary in the releases so you can still use it on Windows, just can't build from source.

## Usage

Example:

Inside one .c/.cpp file include Leet.h and define `LEET_IMPLEMENTATION`

```cpp
#define LEET_IMPLEMENTATION
#include "Leet.h"
```

then just compile the source

```bash
./build/bin/clang++ ./test.cpp -o test -fno-exceptions
```

## Features

All of this was compiled with `-O3` flag and all screenshots come from IDA Pro 9.4. I also tested it with Binary Ninja and Ghidra, results were the same. But since I think IDA has the best decompiler and dissasembler I'll use the screenshots from that. 

### String Encryption
Encrypts strings at compile time and inserts a decrypt function at every use of the encrypted string. This completely disables the ability to search for any strings in the binary. And every string has its own unique key hardcoded in the decrypt function which makes dumping and decrypting them a lot harder.

<table>
  <tr>
    <td align="center">
      <img alt="" src="Pictures/STR.png" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="" src="Pictures/STRobf.png" /><br>
      <b>After
    </td>
  </tr>
</table>

### Mixed Boolean Arithmetic (MBA)
Replaces arithmetic operations with their MBA equivalents. It's basically impossible to see what the original operation did unless you run it through an MBA deobfuscator first. Of course this obfuscation is kinda weak because MBA is the oldest trick in the book, so there are many tools to deal with that, for example CoBRA, it will successfully deobfuscate this into the original expression:

```
./cobra-cli --mba "((x^y) - (((x^y)&0xFF)&0xA) + 10) * ((x^y) - ((x^y)|0xA) + 10) + ((x^y) - (((x^y)&0xFF)|0xFFFFFFF5) - 11) * (~(x^y) - (~(x^y)|0xA) + 10)" --bitwidth 32

10 * (x ^ y)
```

That's why I've made AAMBA pass.

<table>
  <tr>
    <td align="center">
      <img alt="" src="Pictures/MBA.png" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="" src="Pictures/MBAobf.png" /><br>
      <b>After
    </td>
  </tr>
</table>

### Architectural Hardening MBA (AAMBA)
Replaces operands of binary operations with `ADC(X, 255) - 255 - CF` and `SBB(X, 255) + 255 + CF`. Of course it always evaluates to `X`, but it makes the expression dependent on the carry flag. Unless a decompiler tracks the state of CF (which sometimes is impossible) it will get very confused and won't be unable to fold these expressions. It pairs very nicely with the previous MBA pass obfuscating the arithmetic even further. As you can see below the decompiler created some additional variables and uses a lot of `__PAIR64__` and `__CFADD__` calls, so it becomes a lot harder to paste that into tools like CoBRA, IDA's goomba plugin is also no help in simplifying this. Also, IDA's decompiler does track the carry flag to some degree, but combining this with control flow obfuscation makes tracking impossible without execution, so later passes will add up even more to this one.

<table>
  <tr>
    <td align="center">
      <img alt="" src="Pictures/MBA.png" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="" src="Pictures/AAMBAobf.png" /><br>
      <b>After
    </td>
  </tr>
</table>

### Control Flow Flattening
Definitely the strongest and most useful pass, it collects all the blocks inside a function and makes one giant state machine out of them, it creates a jump table at the beginning of the function and places all the block pointers inside it. Then instead of normal jump at the end of each block everything gets routed through the dispatcher which uses indirect jumps, these are almost impossible to resolve statically without any execution.

<img alt="" src="Pictures/DISPATCHERobf.png" />
<img alt="" src="Pictures/DispatcherGraph.png" />

### Anti Analysis pass
It creates a bunch of bogus blocks containing invalid assembly. This throws disassemblers immensely because if the disassembler encounters a technically invalid byte that never gets executed, it will still try to make sense of it. So if the byte is incomplete, it will create an instruction from whatever bytes happen to be after it, that creates a desynch essentially destroying every instruction after that. On Windows it's able to somewhat get through this, in rare cases it will be able generate a graph and decompile what it can (tho it will be broken and incomplete), while on Linux it completely breaks the graph view and disables decompilation.

<table>
  <tr>
    <td align="center">
      <img alt="" src="Pictures/AntiAnalysisWin64.png" /><br>
      <b>Windows
    </td>
    <td align="center">
      <img alt="" src="Pictures/AntiAnalysisLin64.png" /><br>
      <b>Linux
    </td>
  </tr>
</table>

On top of that, if the pass sees any instruction starting with `0xFF`, it inserts a single `0xEB` byte before it. This will create `JMP RIP+1`, so control flow is unchanged (RIP simply advances one byte into the original instruction), but disassemblers become desynchronized.
Instructions beginning with 0xFF are mostly `INC/DEC` and indirect `JMP/CALL`. Sadly most of the ordinary calls and jumps are relative (`0xE8/0xE9/0xEB`) and stay unaffected. But the technique is especially useful around with the dispatcher pass since everything there uses indirect jumps. It's not as useful for calls tho, the only calls that are affected by this are the indirect ones, typically virtual calls, calls through function pointers, and some external/library calls.

<img alt="" src="Pictures/AntiAnalysisLin64EB.png" />

### Anti Aliasing Pass
Throws every stack local in a function into a one big shared stack buffer to which indices are computed at runtime. This way decompilers can't alias variables which makes accesses to the same variable multiple times show up as accessing different values. This plays really nicely with the dispatcher since it demotes registers to stack, so there will a lot of these stack slots.

<table>
  <tr>
    <td align="center">
      <img alt="" src="Pictures/ANTIALIAS.png" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="" src="Pictures/ANTIALIASobf.png" /><br>
      <b>After
    </td>
  </tr>
</table>

### Nanomites Pass
I think the second most useful pass after the dispatcher. It obfuscates control flow through exceptions. It replaces all calls with `int3` traps. When the trap is triggered the control flow goes to the exception handler which adjusts `RIP` to the actual call. It also inserts invalid bytes right after the trap to desynchronize the disassembler.

<table>
  <tr>
    <td align="center">
      <img alt="" src="Pictures/NANOMITES.png" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="" src="Pictures/NANOMITESobf.png" /><br>
      <b>After
    </td>
  </tr>
</table>

## Combining Every Pass
By combining every pass static analysis becomes very hard without some extra tools that would deobfuscate this. Even if you somehow nop all the invalid bytes and you're able to decompile this to some pseudocode or at least get a graph view, you're still left with control flow obfuscation through exceptions and the dispatcher, and even if you break through that, there's a mountain of redundant MBAs, bogus blocks and string encryption obfuscating the actual operations. Here are screenshots of the main entry point of a simple program containing that simple xor Foo function from before in all of the three big boy disassemblers. As you can see they can't make much of it.

<table>
  <tr>
    <td align="center">
      <img alt="" src="Pictures/FinalIDA.png" /><br>
      <b>IDA Pro 9.4
    </td>
    <td align="center">
      <img alt="" src="Pictures/FinalBN.png" /><br>
      <b>Binary Ninja Personal 5.2
    </td>
    <td align="center">
      <img alt="" src="Pictures/FinalGhidra.png" /><br>
      <b>Ghidra 12.1.2
    </td>
  </tr>
</table>

## Performance
Of course inserting all this bullshit into the binary will slow it down immensely, over 200 times slower on the default settings on average:

<img alt="" src="Pictures/NanomitesOn.png" /><br>


Though it's not as bad as it looks because of 2 things. 1. almost 95% of the performance cost here is caused by nanomites, because well, interrupts are just slow. Exception has to leave to the kernel and come back to the app, that takes time. Without nanomites it's down to being only 7.5x slower:

<img alt="" src="Pictures/NanomitesOff.png" /><br>

So I highly advise to just mark the functions and calls you want to obfuscate with nanomites manually instead of just setting it to all, obfuscating every call inside a binary is pointless and costs a lot. And the reason number 2. most of the time you don't really care about the performance of the things you want to hide. This obfuscator has ability to get selectively enabled and disabled. So you can disable it for the performance critical sections of your code and enable it wherever it's actually needed. Nobody cares whether your license check takes 1ms or 0.01ms, it's still unnoticeable for a human.

## Configuration
You can tweak the settings of this obfuscator by either messing with the config file or marking functions with annotations in the code.

### Config File
When running for the first time compiler will prompt you to create a config file. Inside it you'll be able to change settings globally and per pass.

#### Parameter Priority
Parameters can be set at three levels, with each level overriding the previous:
1. Global settings in the config file (apply to all passes)
2. Per pass settings in the config file (override global for that specific pass)
3. Per function annotations in the code (override both global and per pass settings)

#### Available Parameters Per Pass

**StringEncryptionPass**
- `defaultParseMode`: Whether to apply the pass by default (all or none).
- `runtimeSeed`: Seed for the random number generator.
- `probability`: Percentage chance (0-100) to apply string encryption to a string.

**MBAPass**
- `defaultParseMode`: Whether to apply the pass by default (all or none).
- `runtimeSeed`: Seed for the random number generator.
- `minFunctionSize`: Minimum function size in instructions to be obfuscated.
- `maxFunctionSize`: Maximum function size in instructions to be obfuscated.
- `minBlockSize`: Minimum basic block size in instructions to be obfuscated.
- `maxBlockSize`: Maximum basic block size in instructions to be obfuscated.
- `probability`: Percentage chance (0-100) to apply MBA to an operation.
- `expansionCount`: Number of MBA expansions to apply.

**BlockSplitterPass**
- `defaultParseMode`: Whether to apply the pass by default (all or none).
- `runtimeSeed`: Seed for the random number generator.
- `probability`: Percentage chance (0-100) to split a block.
- `blockSplitSize`: Target size to split blocks at.

**DispatcherPass**
- `defaultParseMode`: Whether to apply the pass by default (all or none).
- `runtimeSeed`: Seed for the random number generator.
- `minFunctionSize`: Minimum function size in instructions to be obfuscated.
- `maxFunctionSize`: Maximum function size in instructions to be obfuscated.
- `probability`: Percentage chance (0-100) to apply control flow flattening.

**AntiAnalysisPass**
- `defaultParseMode`: Whether to apply the pass by default (all or none).
- `runtimeSeed`: Seed for the random number generator.
- `minFunctionSize`: Minimum function size in instructions to be obfuscated.
- `maxFunctionSize`: Maximum function size in instructions to be obfuscated.
- `minBlockSize`: Minimum basic block size in instructions to be obfuscated.
- `maxBlockSize`: Maximum basic block size in instructions to be obfuscated.
- `probability`: Percentage chance (0-100) to apply anti analysis.
- `bogusInsertPosition`: Where to insert bogus blocks (start or random).

**AAMBAPass**
- `defaultParseMode`: Whether to apply the pass by default (all or none).
- `runtimeSeed`: Seed for the random number generator.
- `minFunctionSize`: Minimum function size in instructions to be obfuscated.
- `maxFunctionSize`: Maximum function size in instructions to be obfuscated.
- `minBlockSize`: Minimum basic block size in instructions to be obfuscated.
- `maxBlockSize`: Maximum basic block size in instructions to be obfuscated.
- `probability`: Percentage chance (0-100) to apply AAMBA to an operation.

**AntiAliasingPass**
- `defaultParseMode`: Whether to apply the pass by default (all or none).
- `runtimeSeed`: Seed for the random number generator.
- `minFunctionSize`: Minimum function size in instructions to be obfuscated.
- `maxFunctionSize`: Maximum function size in instructions to be obfuscated.
- `probability`: Percentage chance (0-100) to apply anti aliasing.

**NanomitesPass**
- `defaultParseMode`: Whether to apply the pass by default (all or none).
- `runtimeSeed`: Seed for the random number generator.
- `minFunctionSize`: Minimum function size in instructions to be obfuscated.
- `maxFunctionSize`: Maximum function size in instructions to be obfuscated.
- `probability`: Percentage chance (0-100) to apply nanomites to a call.

Example config file:
```
# GLOBAL SETTINGS
# These apply to all passes unless overridden in individual pass configurations
#
defaultParseMode=all
runtimeSeed=0
minFunctionSize=20
maxFunctionSize=0
minBlockSize=0
maxBlockSize=0

# PASS CONFIGURATIONS
# Each pass can override global settings or add pass specific parameters
# Parameters not specified here will use the global values above or the default ones
# Btw order matters here, the only exception is StringEncryptionPass which will always run first
#
passes=
    StringEncryptionPass(),
    MBAPass(expansionCount=2, probability=50),
    BlockSplitterPass(blockSplitSize=50),
    AntiAnalysisPass(probability=25),
    DispatcherPass(),
    MBAPass(expansionCount=1),
    AAMBAPass(probability=35),
    AntiAliasingPass(),
    AntiAnalysisPass(bogusInsertPosition=start,probability=25),
    NanomitesPass();
```

### Annotations
You can also mark individual functions and calls in the code with annotations. Inside `Leet.h` there are macros for every possible setting, you can just slap them on a function and it will be read by the obfuscator.

#### Annotation Macros Per Pass

**StringEncryptionPass**
- `LEET_STRING_ENCRYPTION_PROBABILITY(value)`: Set the probability (0-100) of encrypting a string.

**MBAPass**
- `LEET_MBA_EXPANSION_COUNT(count)`: Set the number of MBA expansions to apply.
- `LEET_MBA_PROBABILITY(value)`: Set the probability (0-100) of applying MBA to an operation.

**BlockSplitterPass**
- `LEET_BLOCK_SPLITTER_PROBABILITY(value)`: Set the probability (0-100) of splitting a block.
- `LEET_BLOCK_SPLITTER_SPLIT_SIZE(value)`: Set the target size to split blocks at.

**DispatcherPass**
- `LEET_DISPATCHER_PROBABILITY(value)`: Set the probability (0-100) of applying control flow flattening.

**AntiAnalysisPass**
- `LEET_ANTI_ANALYSIS_PROBABILITY(value)`: Set the probability (0-100) of applying anti analysis.
- `LEET_ANTI_ANALYSIS_BOGUS_INSERT_POSITION(value)`: Set where to insert bogus blocks ("start" or "random").

**AntiAliasingPass**
- `LEET_ANTI_ALIASING_PROBABILITY(value)`: Set the probability (0-100) of applying anti aliasing.

**AAMBAPass**
- `LEET_AAMBA_PROBABILITY(value)`: Set the probability (0-100) of applying AAMBA to an operation.

**NanomitesPass**
- `LEET_NANOMITES_PROBABILITY(value)`: Set the probability (0-100) of applying nanomites to a call.
- `LEET_NANOMITE_CALL(func)`: Mark a specific call site for nanomite obfuscation.

#### Generic Pass Control Macros
These work with almost any pass and provide common control options:
- `LEET_SKIP_PASS(pass)`: Skip the specified pass for this function
- `LEET_FORCE_PASS(pass)`: Force the specified pass to run on this function
- `LEET_RUNTIME_SEED_PASS(pass, seed)`: Set the runtime seed for the specified pass
- `LEET_MIN_FUNCTION_SIZE(pass, size)`: Set the minimum function size for the specified pass
- `LEET_MAX_FUNCTION_SIZE(pass, size)`: Set the maximum function size for the specified pass
- `LEET_MIN_BLOCK_SIZE(pass, size)`: Set the minimum block size for the specified pass
- `LEET_MAX_BLOCK_SIZE(pass, size)`: Set the maximum block size for the specified pass

#### Apply to All Passes
These macros apply their setting to all passes at once:
- `LEET_SKIP_ALL`: Skip all passes for this function
- `LEET_FORCE_ALL`: Force all passes to run on this function
- `LEET_RUNTIME_SEED_ALL(seed)`: Set the runtime seed for all passes
- `LEET_MIN_FUNCTION_SIZE_ALL(size)`: Set the minimum function size for all passes
- `LEET_MAX_FUNCTION_SIZE_ALL(size)`: Set the maximum function size for all passes
- `LEET_MIN_BLOCK_SIZE_ALL(size)`: Set the minimum block size for all passes
- `LEET_MAX_BLOCK_SIZE_ALL(size)`: Set the maximum block size for all passes

Example usage:
```cpp
#include "Leet.h"

LEET_FORCE_PASS("DispatcherPass")
LEET_SKIP_PASS("StringEncryptionPass")
LEET_MBA_EXPANSION_COUNT(3)
LEET_MBA_PROBABILITY(80)
void importantFunction()
{
    LEET_NANOMITE_CALL(sensitiveFunction());
    nonSensitiveFunction();
}
```

## Current Limitations

- Works only for x64 and x86 both Windows and Linux.
- Has to be compiled with -fno-exceptions flag, so obviously no try catch in the obfuscated code.
- Basically work in progress. Don't expect this to work for bigger projects (it probably won't you can try tho). It's not very well tested, I have some tests written by LLMs because I had no actual projects on hand, but they hardly count as big applications. They're mostly single module files stress testing a specific part of C++. They caught a lot of errors, but again, a new ones will probably pop up on bigger binaries, so if you encounter any please open an issue and let me know.
