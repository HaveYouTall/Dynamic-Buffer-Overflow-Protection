# Dynamic-Buffer-Overflow-Protection
Provide a pintool for protecting Buffer Overflow dynamically.

## Usage

### Compilation

- For 64-bit ELF
  `make obj-intel64/Buffer_Overflow_Protector.cpp`
- For 32-bit ELF
  `make obj-intel32/Buffer_Overflow_Protector.cpp`
 
### Run

`pin -t obj-intel64/Buffer_Overflow_Protector.so -- ./some-program`

or

`pin -t obj-intel32/Buffer_Overflow_Protector.so -- ./some-program`
  
  
## Update

Added `debug_info.cpp` for more debug information when protecting a program.

Debug_info could show functions corresponding address, and it can also protect any program from suffering BOF.

### Compilation
- For 64-bit ELF
  `make obj-intel64/debug_info.cpp`
- For 32-bit ELF
  `make obj-intel32/debug_info.cpp`
  
 ### Run
 
 `pin -t obj-intel64/debug_info.so -- ./some-program`
 
 or
 
  `pin -t obj-intel32/debug_info.so -- ./some-program`
 
