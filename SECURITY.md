# Security status

The community server is currently a **development/LAN server**, not a hardened public MMO service.

Current authentication is intentionally simple for local testing. Do not expose the TCP listener directly to the public Internet and do not reuse an important password.

Before Internet deployment, harden at least:

- encrypted authentication/transport;
- password hashing (use a modern password KDF rather than the current development implementation);
- account creation controls, rate limits, lockouts and abuse prevention;
- packet length/type validation and fuzz testing;
- authorization for moderator/admin operations;
- database backup, migration and recovery procedures;
- logging with sensitive-field redaction;
- process isolation, firewalling and least-privilege execution.

The public-source bundle intentionally excludes local account databases, credentials, API keys, private keys, and machine-specific paths.
