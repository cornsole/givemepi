# givemepi

---

# Build

## Requirements

- CMake 3.20+
- C++20 compiler
- Git


## Build Location

All build commands must be executed from the project root.

Project root:

```

givemepi/
├── CMakeLists.txt
├── src/
├── include/
├── tests/
└── third_party/

````


## Configure

Run:

```bash
cmake -S . -B build
````

## Build

Run:

```bash
cmake --build build
```

## Clean Build

Remove previous build files:

```bash
rm -rf build
```

Reconfigure:

```bash
cmake -S . -B build
```

Build:

```bash
cmake --build build
```

## Output

After successful build:

```
build/pi-engine
```

will be generated.

## Verify Build

Expected result:

* No compiler errors
* No linker errors
* Executable generated

