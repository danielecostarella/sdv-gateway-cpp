# ADR-003: Data-Driven VSS Mapping over Code Generation

**Status:** Accepted  
**Date:** 2026-04-06

## Context

The gateway must translate raw CAN frames into VSS-typed signals. The
translation requires: CAN identifier, byte offset, byte length, scale
factor, offset, and target VSS path.

Two approaches are viable:

1. **Code generation** — parse a DBC file at build time and emit C++ structs
   for each signal. Type-safe at compile time, but requires a DBC toolchain
   and a rebuild to change any mapping.
2. **Data-driven JSON config** — load a mapping file at startup. No toolchain
   dependency, mappings are editable without recompilation.

## Decision

Use a **data-driven JSON mapping file** (`config/mapping.json`) loaded at
runtime during startup.

The schema is intentionally minimal — it covers the subset of DBC semantics
needed for the simulated signals — and is validated against a fixed structure
at load time. Unknown fields are rejected with a clear error.

A full DBC parser is explicitly out of scope: it would add significant
complexity for no observable benefit in a gateway operating a well-known,
bounded set of vehicle signals.

## Rationale

- **Flexibility:** signal mappings can be updated via configuration management
  (or OTA) without recompiling or redeploying the binary. This aligns with
  the SDV principle of separating software from vehicle-specific data.
- **Simplicity:** the schema maps directly to the decoder's data structures,
  making the loader trivial to implement and test.
- **Testability:** unit tests can inject arbitrary JSON strings without
  touching the filesystem, making the decoder independently testable.
- **Precedent:** the Eclipse Kuksa CAN provider (`kuksa-can-provider`) uses
  an equivalent JSON-based `vss_dbc.json` mapping for the same reason.

## Consequences

- Mapping errors (wrong type, missing field, out-of-range factor) are
  detected at startup, not at compile time. The gateway exits with a clear
  diagnostic if the config is invalid.
- The mapping file is part of the deployment artifact and should be version-
  controlled alongside the binary.
- Adding support for extended CAN frames (29-bit IDs) or CAN FD requires
  only a schema extension, not a code architecture change.
