# Security Policy

## Reporting a Vulnerability

BitChat takes security seriously. If you discover a security vulnerability,
**please do not open a public issue.**

Instead, report it privately using one of these methods:

1. **GitHub Security Advisory** (preferred):
   Go to the [Security tab](../../security/advisories/new) and create a new advisory.

2. **Email**: Contact the maintainers directly.

## Scope

The following are in scope:
- Cryptographic weaknesses (key exchange, encryption, nonce handling)
- Memory safety issues (buffer overflows, use-after-free, information leaks)
- Authentication/identity bypass
- Message deduplication bypass leading to DoS
- Wire protocol vulnerabilities

## Response Timeline

- **Acknowledgment**: Within 48 hours
- **Assessment**: Within 7 days
- **Fix**: Depends on severity, but critical issues are prioritized immediately

## Disclosure Policy

We follow coordinated disclosure. We ask that you:
- Allow reasonable time for a fix before public disclosure
- Do not exploit the vulnerability beyond what is necessary to demonstrate it
