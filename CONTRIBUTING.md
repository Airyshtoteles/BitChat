# Contributing to BitChat

Thank you for your interest in contributing to BitChat! This project builds a
censorship-resistant, fully offline P2P mesh chat engine in pure C11. Every
contribution matters.

## Quick Setup

### 1. Fork & Clone

```bash
git clone https://github.com/<your-username>/BitChat.git
cd BitChat
git submodule update --init
```

### 2. Install Dependencies

**Ubuntu/Debian:**
```bash
sudo apt install cmake libsodium-dev libsqlite3-dev build-essential pkg-config
```

**Arch Linux:**
```bash
sudo pacman -S cmake libsodium sqlite
```

**macOS:**
```bash
brew install cmake libsodium sqlite pkg-config
```

### 3. Build & Test

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
ctest --verbose
```

All 23 tests **must pass** before submitting a PR.

---

## Pull Request Requirements

Every PR must satisfy **all** of the following before it can be merged:

### 1. Zero Compiler Warnings

The project compiles with strict flags. Your code must produce zero warnings:

```
-Wall -Wextra -Wpedantic -Werror
```

### 2. No Memory Leaks

All heap allocations must have a corresponding `free()`. Cryptographic secrets
must be wiped with `sodium_memzero()` before deallocation. Run your changes
under Valgrind or AddressSanitizer:

```bash
cmake .. -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer"
make && ctest
```

### 3. CI Must Be Green

GitHub Actions runs `cmake .. && make && ctest --output-on-failure` on every
push and PR. Your PR will not be reviewed until the CI pipeline passes.

### 4. Tests for New Functionality

New features and bug fixes must include corresponding unit tests using the
Unity test framework under `tests/`.

### 5. C11 Standard Only

The core logic is pure C11. No C++ extensions, no compiler-specific builtins,
no GNU extensions. This ensures portability across GCC, Clang, and cross-
compilation targets (Android NDK, embedded ARM).

---

## Coding Style

- **4-space indentation** (no tabs)
- **`snake_case`** for functions and variables
- **`bc_`** prefix for all public symbols
- **`BC_`** prefix for all macros and constants
- **Doxygen `/** */` comments** for all public API functions
- Keep lines under 100 characters where practical

---

## Architecture Guidelines

BitChat follows **Clean Architecture**. Respect the layer boundaries:

```
Domain (no dependencies)
  --> Port (abstract interfaces)
    --> Use Case (business logic, depends on Domain + Port)
      --> Infrastructure (implements Port interfaces)
        --> Bridge (FFI surface, depends on Use Case)
```

**Rules:**
- Domain layer **must not** import anything from upper layers.
- Use Case layer depends on Domain and Port only -- never on Infrastructure.
- Infrastructure implements Port interfaces via vtable structs.
- Bridge exposes only FFI-safe types (primitives and byte arrays).
- New transport adapters must implement `bc_transport_t` (see `port/transport.h`).
- New storage backends must implement `bc_storage_t` (see `port/storage.h`).

---

## Submitting Code

1. **Create a branch** from `main`:
   ```bash
   git checkout -b feat/your-feature-name
   ```

2. **Make atomic commits** -- one logical change per commit.

3. **Open a Pull Request** against `main` with:
   - A clear description of what changed and why
   - Reference to related issues (e.g., `Closes #42`)

---

## Reporting Bugs

Open an issue with:
- Steps to reproduce
- Expected vs actual behavior
- Platform and compiler version

## Security Vulnerabilities

If you discover a security vulnerability, **do not** open a public issue.
Report it privately via [GitHub Security Advisories](https://github.com/Airyshtoteles/BitChat/security/advisories).

## License

By contributing, you agree that your contributions will be licensed under the
[MIT License](LICENSE).
