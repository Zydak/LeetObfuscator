// #include <cstdint>
// // This is a function template that StringEncryptionPass uses to create decryption functions
// // this file gets compiled into bitcode and then injected into a module the string pass is obfuscating
// // so that I don't have to emit all of it manually through llvm::IRBuilder which would be really annoying

// // empty function just so that LLVM doesn't change the key into a constant before the string pass runs
// extern "C" uint32_t __leet_get_key();

// extern "C" uint8_t* __leet_decrypt_string(uint8_t* enc, uint8_t* out, uint32_t len)
// {
//     const uint32_t key = __leet_get_key();
//     for (uint32_t i = 0; i < len; ++i) {
//         uint8_t keyByte = uint8_t((key >> (8 * (i % 4))) & 0xFF);
//         out[i] = enc[i] ^ keyByte;
//     }
//     return out;
// }