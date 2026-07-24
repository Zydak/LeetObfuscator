# LeetObfuscator

A simple obfuscator for binaries made in c++ and LLVM.

---

## Features

- **String Encryption**
  - Encrypts string literals at compile time.
  - Decrypts them only when needed at runtime.
  - Makes static string extraction a lot more difficult.

- **Control Flow Flattening**
  - Transforms function control flow into a dispatcher state machine with a jump table containing all block addresses.
  - Destroys the control flow.

- **Mixed Boolean Arithmetic (MBA)**
  - Replaces arithmetic expressions with equivalent MBA identities.
  - It's pretty annoying to read stuff obfuscated by this.

---

## Example

### Before

```cpp
if (password == "secret") {
    return 1;
}

return 0;
```

### After (conceptually)

```text
- "secret" is encrypted
- Control flow is flattened
- Arithmetic operations are replaced with MBA expressions
- Resulting LLVM IR is significantly less readable
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
- Optimizer interactions can occasionally break transformations.
- Basically work in progress, don't expect it to work on larger binaries. It's sufficient for simple programs or crackmes.