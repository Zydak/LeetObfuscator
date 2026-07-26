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

A simple obfuscator for binaries made in C++ and LLVM.
```

## Features

### String Encryption
Encrypts strings at compile time and inserts a decrypt function at every use of the encrypted string.

Here a simple `Hello World!` function:

 <img width="470" height="119" alt="HelloWorld" src="https://github.com/user-attachments/assets/ce726311-ce62-4222-a539-dc489ec20725" />

<table>
  <tr>
    <td align="center">
      <img alt="Before" src="https://github.com/user-attachments/assets/70b60aca-85df-4e94-8623-e3ff57015720" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="After" src="https://github.com/user-attachments/assets/eaa5dc73-c40c-413f-8736-cb27ab8ae4aa" /><br>
      <b>After
    </td>
  </tr>
</table>
        
As you can see the constant got replaced by `leet_decrypt_string` function, that function will decrypt the string from memory and store it in a variable. Real contents sit encrypted in the rodata section, so it's impossible to simply search for strings in a binary now. Also the encryption key is unique for each string, so even if you find out how the decryption works, every string has its own function. Of course you can just place a breakpoint on this function and see the contents clearly after decryption, but the point of this is not some cryptographics safety but preventing people from dumping and searching strings in the binary.

<img alt="Data" src="https://github.com/user-attachments/assets/92b2ac7c-f74c-4599-8292-29ce51a4be7c" />

### Mixed Boolean Arithmetic (MBA)
Replaces arithmetic operations with their MBA equivalents. It's basically impossible to see what the original operation did unless you run it through an MBA deobfuscator first.
  
A simple Foo function:

<img alt="Xor" src="https://github.com/user-attachments/assets/9cb2185e-6545-4f0b-bc88-53e675ce5521" />

<table>
  <tr>
    <td align="center">
      <img alt="Before" src="https://github.com/user-attachments/assets/e9566f0b-4a21-40d4-ad2c-7aa55f6a4c9f" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="After" src="https://github.com/user-attachments/assets/df8401c6-627c-4883-ab4b-2a30329f81c5" />
      <b>After
    </td>
  </tr>
</table>

As you can see there's simply no way to see what the original operation was, and this is the result after running Goomba plugin.

### Control Flow Flattening
Definitely the strongest and most useful pass, it collects all the blocks inside a function and makes one giant state machine out of them, it creates a jump table at the beginning of the function and places all the block pointers inside it.

<table>
  <tr>
    <td align="center">
      <img alt="Before" src="https://github.com/user-attachments/assets/81f4f0e6-e5a7-4118-acbd-bb9b70ac2c8e" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="After" src="https://github.com/user-attachments/assets/fa0444af-9454-4f9f-9fa9-3b73454fe8e0" />
      <b>After
    </td>
  </tr>
</table>

If we take our previous hello world function it completely destroys it's control flow. The only thing we see is an entry block with an indirect jump to the dispatcher which IDA pro can't seem to resolve, you can't even tell what was this function supposed to do. Here's how the control flow actually looks because IDA won't show us:

<table>
  <tr>
    <td align="center">
      <img alt="FooNoObf" src="https://github.com/user-attachments/assets/c07cc14a-30db-4c45-8a69-fcb5776044f1" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="FooObf" src="https://github.com/user-attachments/assets/15202227-5620-4406-a692-2c573aed9cb9" /><br>
      <b>After
    </td>
  </tr>
</table>

As you can see before the pass graph looks 1:1 like what we've seen in IDA pro. But after the pass it's as flat as my butt, it just constantly jumps between the dispatcher and blocks with nobody having a clear idea what's going on. You can't even tell which block executes first because all indices into the jump table are encoded at compile time and decoded at runtime.

### Combining Every Pass

Of course if you combine every pass you can completely destroy the dreams of a casual reverse engineer, because the big three (IDA pro, Binary Ninja, Ghidra) are completely unable to see anything. Figuring this out is virtually impossible with just a decompiler. Here's the previous function with `(x^y)*10`, it's a lot larger this time because of all the passes, and nobody is able to tell what's going on in there.

<img alt="FooAll" src="https://github.com/user-attachments/assets/fcc4c218-604c-4fc0-bfc1-345b0fd31853" />

(Btw everything in this section was compiled with `-O3` flag and verified with all of the three big boy decompilers (IDA pro, Binary Ninja, Ghidra), the decompilation results were the same in each one.)

Of course this example is blown out of proportion, if you're gonna obfuscate every simple operation to this magnitude your application will be slow as hell and big as hell. If we run this little monster through a small stress test it's around 5 times slower than running without any obfuscation.

<img alt="StressTest" src="https://github.com/user-attachments/assets/361f0d98-90d2-4b76-bc91-e0815d4e1f39" />


`clang++ -fpass-plugin=./LeetObfuscator.so ./test.cpp -o testProj -O2 -fno-exceptions` - 5000ms

`clang++ ./test.cpp -o testProj -O2 -fno-exceptions` - 950ms


That's why you can tune the obfuscator up and down however you like. After running it for the first time it will prompt to create a `leet.conf` file with default settings used for everything, you can also overwrite these settings for each function using annotations.

Currently supported ones are:

```
LEET_SKIP_FUNCTION - Completely skips obfuscation for this function
LEET_PARSE_FUNCTION - Forces obfuscation for this function
LEET_MAX_BLOCK_SIZE(size) - Sets max block size, every block above this size will be split into smaller blocks inside the dispatcher
LEET_MBA_EXPANSION_COUNT(count) - How many times to expand the instruction recursively
```

you can just slap these on a function and that's it:
```
LEET_MAX_BLOCK_SIZE(50)
int Foo(int x, int y)
{
    return (x ^ y) * 10;
}
```

So if you have any performance heavy function and there is no point in obfuscating it, just slap the skip annotation on it and be a happier person.

You'll gain the biggest performance gain by lowering the block split count, for the stress test max block count of 20 was used and this is how main function looks:

<img alt="Main" src="https://github.com/user-attachments/assets/801116e3-8876-4aa8-85c6-98040ee60beb" />

If you do default max block size of 100 then there should be close to no performance downside, but again, if you need to squize out every cycle out of a function just disable obfuscation for it.


## Building

### Linux
```bash
git clone https://github.com/<yourname>/LeetObfuscator.git
cd LeetObfuscator

mkdir build
cd build

cmake ..
make
```


Requirements:

- LLVM
- CMake
- C++17 compiler

### Windows
I'm not gonna lie I do not know how to build this on windows, the cmake technically does support it, but after building every single LLVM function would instantly crash on my PC, I do not know why. This is on my TODO list for now. If you know what you're doing and want to give it a shot you can try building it by downloading llvm binaries from [releases on their github](https://github.com/llvm/llvm-project/releases/tag/llvmorg-22.1.8) and then running:

```
git clone https://github.com/<yourname>/LeetObfuscator.git
cd LeetObfuscator

mkdir build
cd build

cmake .. -DLLVM_DIR="C:\Dev\LLVM\lib\cmake\llvm" # Your path where you downloaded the binaries
```

then open the generated solution in visual studio, build, and try running with

`clang++ -fpass-plugin=./LeetObfuscator.dll ./test.cpp -o test.exe -fno-exceptions`

But most likely you'll get the same errors as I did, so it's probably easier to run this in WSL and just cross compile your binary to windows.

## Usage

Example:

```bash
clang++ -fpass-plugin=./LeetObfuscator.so ./test.cpp -o test -fno-exceptions
```

## Current Limitations

- Has to be compiled with -fno-exceptions flag, so obviously no exceptions in the compiled code
- Not every single operation is supported, so it might skip obfuscating some code paths to avoid crashing.
- So basically work in progress, don't expect it to work on larger binaries. But I think It's sufficient for simple programs or crackmes. It's a small project, please don't expect much, if you encounter any issues or crashes please create an issue.
