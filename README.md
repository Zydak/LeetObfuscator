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

A simple obfuscator for binaries made in c++ and LLVM.
```

## Features

---

- **String Encryption**
  - Encrypts string literals at compile time.
  - Decrypts them only when needed at runtime.
  - Makes static string extraction a lot more difficult.

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
        
As you can see the constant got replaced by `leet_decrypt_string` function, that function will decrypt the string from memory and store it in a variable. Real contents sit encrypted in the rodata section, so it's impossible to simply search for strings in a binary now. Also the encryption key is unique for each string, so even if you find out how the decryption works, every string has it's own function. Of course you can just place a breakpoint on this function and see the contents clearly after decryption, but the point of this is not some cryptographics safety but preventing people from dumping and searching strings in the binary.

<img alt="Data" src="https://github.com/user-attachments/assets/92b2ac7c-f74c-4599-8292-29ce51a4be7c" />

---

- **Mixed Boolean Arithmetic (MBA)**
  - Replaces arithmetic expressions with equivalent MBA identities.
  - It's pretty annoying to read stuff obfuscated by this.
  
  A simple Foo Function:

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

As you can see there's simply no way to see what was the original operation, and this is the result after running Goomba plugin.


---

- **Control Flow Flattening**
  - Transforms function control flow into a dispatcher state machine with a jump table containing all block addresses.
  - Destroys the control flow.

Definitely the strongest and most usefull pass, it collects all the blocks inside a function and makes one giant state machine out of them, it creates a jump table at the begining of the function and places all the block pointers inside it.

<table>
  <tr>
    <td align="center">
      <img alt="Before" src="https://github.com/user-attachments/assets/81f4f0e6-e5a7-4118-acbd-bb9b70ac2c8e" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="After" src="https://github.com/user-attachments/assets/c4d1f012-b680-483d-804b-799fb2561946" /><br>
      <b>After
    </td>
  </tr>
</table>

As you can see if the take our previous hello world function it completely destroys it's control flow. The only thing we see is an entry block with an indirect jump to the dispatcher which IDA pro can't seem to resolve, you can't even tell what is this function supposed to do anymore. Here's how the control flow actually looks because IDA won't show us:

<table>
  <tr>
    <td align="center">
      <img alt="FooNoObf" src="https://github.com/user-attachments/assets/c07cc14a-30db-4c45-8a69-fcb5776044f1" /><br>
      <b>Before the pass
    </td>
    <td align="center">
      <img alt="FooObf" src="https://github.com/user-attachments/assets/3281bd4f-6ae1-455c-a268-ff94a3eb2855" /><br>
      <b>After
    </td>
  </tr>
</table>

As you can see before the pass graph looks 1:1 like what we've seen in IDA pro. But after the pass it's as flat as my butt, it just constantly jumps between the dispatcher and blocks with no clear idea what's going on. You can't tell what exactly is happening and which block even executes first because all indices into the jump table are hashed and unhashed at runtime.


---

**Combining Every Pass**
Of course if you combine every pass you can completely destroy the dreams of a casual reverse engineer, because the big three (IDA pro, Binary Ninja, Ghidra) are completely unable to tell what's going. Figuring this out is virtually impossible with just a decompiler. Here's the previous function with `(x^y)*10`, it's a lot larger this time because of all the passes, and nobody is able to tell what's going on in there.

<img alt="FooAll" src="https://github.com/user-attachments/assets/0e9ade44-c8f8-4c64-8437-17c1dd5346ae" />

(Btw everything in this section was compiled with `-O2` flag and verified through all three decompilers (IDA pro, Binary Ninja, Ghidra) the decompilation results were the same in each one.)

Of course this example is blown out of proportion, if you're gonna obfuscate every simple operation to this magnitude your application will be slow as hell and big as hell. You can tune the obfuscator up and down however you like. After running it for the first time it will prompt to create a `leet.conf` file with default settings used for everything, you can also overwrite these settings for each function using annotations.

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

---

## Building

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

---

## Usage

Example:

```bash
clang++ -fpass-plugin=./LeetObfuscator.so ./test.cpp -o test -fno-exceptions
```

---

## Current Limitations

- Has to be compiled with -fno-exceptions flag, so obviously no exceptions in the compiled code
- Not every LLVM IR construct is supported.
- Complex C++ templates may fail.
- Basically work in progress, don't expect it to work on larger binaries. But I think It's sufficient for simple programs or crackmes.
