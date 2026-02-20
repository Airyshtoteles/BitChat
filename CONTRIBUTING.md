# Contributing to BitChat

Thank you for your interest in contributing to BitChat! This project aims to build
a censorship-resistant, fully offline P2P chat application, and every contribution
matters.

## Getting Started

### 1. Fork & Clone

```bash
git clone https://github.com/<your-username>/BitChat.git
cd BitChat
```

### 2. Install Dependencies

**Arch Linux:**
```bash
sudo pacman -S cmake libsodium sqlite
```

**Ubuntu/Debian:**
```bash
sudo apt install cmake libsodium-dev libsqlite3-dev pkg-config
```

**macOS:**
```bash
brew install cmake libsodium sqlite pkg-config
```

### 3. Build & Test

```bash
mkdir build && cd build
cmake ..
make
ctest --verbose
```

All tests **must pass** before submitting a PR.

## How to Contribute

### Reporting Bugs

Open an issue using the **Bug Report** template. Include:
- Steps to reproduce
- Expected vs actual behavior
- Platform and compiler version

### Suggesting Features

Open an issue using the **Feature Request** template. Describe:
- The problem you're trying to solve
- Your proposed solution
- Any alternatives you've considered

### Submitting Code

1. **Create a branch** from `main`:
   ```bash
   git checkout -b feat/your-feature-name
   ```

2. **Follow the coding style:**
   - C11 standard
   - 4-space indentation (no tabs)
   - `snake_case` for functions and variables
   - `bc_` prefix for all public symbols
   - `BC_` prefix for all macros/constants
   - Doxygen `/** */` comments for all public API functions
   - No compiler warnings (`-Wall -Wextra -Wpedantic -Werror`)

3. **Write tests** for any new functionality using the Unity framework.

4. **Keep commits atomic** — one logical change per commit.

5. **Open a Pull Request** against `main` with:
   - A clear description of what changed and why
   - Reference to related issues (e.g., `Closes #42`)

## Architecture Guidelines

BitChat follows **Clean Architecture**. Please respect the layer boundaries:

```
Domain (no dependencies)
  -> Port (abstract interfaces)
    -> Use Case (business logic, depends on Domain + Port)
      -> Infrastructure (implements Port interfaces)
        -> Bridge (FFI surface, depends on Use Case)
```

**Rules:**
- Domain layer **must not** import anything from upper layers.
- Use Case layer depends on Domain and Port only — never on Infrastructure.
- Infrastructure implements Port interfaces via vtable structs.
- Bridge exposes only FFI-safe types (primitives and byte arrays).

## Code Review

All PRs require at least one review. Reviewers will check:
- Correctness and test coverage
- Memory safety (no leaks, proper `free()`/`sodium_memzero()`)
- Clean Architecture boundary compliance
- Coding style consistency

## Security

If you discover a security vulnerability, **do not** open a public issue.
Instead, report it privately via GitHub's Security Advisory feature.

## License

By contributing, you agree that your contributions will be licensed under the
MIT License.
