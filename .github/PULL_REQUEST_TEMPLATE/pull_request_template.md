## Summary
<!-- What does this PR do? 1-3 sentences. -->

## Changes
<!-- Bullet list of what changed. -->
-

## Related Issues
<!-- Link issues this PR addresses. -->
Closes #

## Testing
<!-- How was this tested? -->
- [ ] All existing tests pass (`ctest --verbose`)
- [ ] New tests added for new functionality
- [ ] Builds with zero warnings (`-Wall -Wextra -Wpedantic -Werror`)
- [ ] No memory leaks (tested with Valgrind if applicable)

## Architecture Checklist
- [ ] Domain layer has no upward dependencies
- [ ] New port interfaces use vtable pattern
- [ ] Public API uses only FFI-safe types
- [ ] Secret key material is wiped with `sodium_memzero()`
