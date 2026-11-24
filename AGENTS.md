# Mlash engineering contract

Read `docs/README.md`, `docs/architecture.md`, and `docs/project-status.md` before changes.
The product baseline is `docs/Mlash_Codex_Design_Spec_v2.0.md`; accepted deviations live in `docs/adr/`.

## Scope and implementation

- C++20, Qt 6 Widgets/Core/Network, yaml-cpp, CMake; no WebView, QML, or database. Windows TUN privilege service is explicitly authorized by the user and constrained by ADR0036; no other privileged service scope is implied.
- Windows first. Other platform adapters return explicit Unsupported results.
- AppController owns application state and serializes mutations. UI only calls its commands.
- Network and process control are asynchronous. File operations, YAML, and OS configuration run on bounded workers.
- Check actual OS/Core state after writes. Separate intent, observed state, readiness, and node health.
- Restore the proxy only if its complete current snapshot matches our last written snapshot.
- Persist recovery intent before changing the OS. Failed recovery retains the journal.
- Preserve source YAML, candidate, active, and last known good. Never overwrite the running version on download.
- Secrets, subscription tokens, and raw YAML never enter application logs. Core logs are redacted before display/storage.
- The thirteen architecture principles in `docs/engineering.md` are binding. Principles 1–6 govern placement, machine-checked boundaries, measurable benefits, slice size, test localization and visible debt; principles 7–13 govern the seven-stage design flow, layered loose coupling with protocol integration, one module per file, abstraction first, architected UI containers with token-only styling, a machine-checked feature-inventory-to-UI mapping, and in-file `// CONTRACT` headers for necessary restrictions. Read that section before designing anything new.
- New capability follows the fixed seven stages: feature definition, technology selection, architecture design, module design, protocol design (the persistence equivalent here), interface design, security design. Each stage owes a deliverable in its owning document and the next stage waits for it.
- UI and engine sides are both layered; modules couple only through explicit contracts and documented protocols, never through shared internal state or cross-layer implementation access. One module owns at least one file, and abstraction precedes implementation: define the contract and its test double before writing a second copy of the same logic.
- UI is architected as nested containers (page, section, toolbar, list or form, item). A container that is independently addressable, independently verifiable, or reusable must own its own file; reusable components must never be copied. Colors, type sizes, spacing, and radii come only from design tokens and `UiTheme`, and the token checker is part of the automated gate.
- The feature inventory carries one ID space and a bidirectional mapping to UI structure: every feature names exactly one primary container plus any secondary entries, and the entry column uses route identifiers rather than prose, so a script can compare it with the route table and fail on anything unpaired.
- Files matching one of the trigger conditions (irreversible writes, cross-thread or cross-process state, secrets, cross-version persistence or protocol formats, implicit ordering) carry a `// CONTRACT` header listing owner, invariant, forbidden operations, lifecycle, the guarding CTest names, and the owning spec. Never restate machine-checked facts such as dependency directions there; point at the check instead. `scripts/check_contract_headers.py` enforces the fields and the test references.

## Directory ownership

`src/app`: orchestration and state; `src/core`: API/process; `src/profile`: YAML/download/storage;
`src/platform`: OS APIs; `src/storage`: persistence; `src/diagnostics`: bounded logs; `src/ui`: Widgets.
Windows headers and native functions stay in `src/platform/windows`.
Tests use isolated temporary data. Build artifacts live in `build/`, dependencies in `build/_deps/`,
release staging in `dist/`, binaries in `cores/`. Do not edit adjacent reference repositories.

## Workflow and collaboration

1. Select one item in `docs/project-status.md` and mark it in progress.
2. Establish its observable acceptance criterion; implement the smallest coherent vertical slice.
   UI changes must first pass the principle review and design-before-code workflow in `docs/engineering.md` (UI先审视、先设计、后修改). Record the design before implementation; review the result against it afterward.
3. Build and run targeted tests; fix failures before the next slice.
4. Update the relevant design/contract document and the actual results in `docs/verification-report.md`.
5. Mark complete only with evidence; distinguish implemented, automated-tested, manually-tested, and release-qualified.

Humans and agents share the same module contracts. Assign one writer per file/module when working concurrently;
agree interface changes before editing consumers. Integration owner handles build, tests and status updates.
Delegation requires explicit user authorization; this contract does not authorize spawning agents.
Use `feature/<topic>` or `fix/<topic>` branches when Git is used; concise Conventional Commit subjects.
Do not commit build outputs, user configuration, journals, logs, subscriptions or secrets.

## Documentation

Chinese explanations and UI are preferred; code identifiers stay English. Maintain each fact in one owning document.
ADRs are numbered and append-only in intent; supersede rather than erase decisions. Do not rewrite the original spec.
API/state/schema changes update design and relevant regression tests in the same change.
Never claim Windows 10, clean-machine deployment, 72-hour stability or performance budgets without the actual run.

## Verification

Use `scripts/build.ps1`, `scripts/test.ps1`, `scripts/package.ps1`.
No tests may change the user's system proxy by default. Explicit `--system-proxy-test` is reserved for a controlled test VM.
Prefer meaningful failure/rollback/cancellation tests to implementation-mirroring tests.

## Platform-scoped publication

User-authorized policy: on a Windows development machine require Windows checks only; on macOS require macOS checks only. Push directly without waiting for PR or cross-platform CI approval. Release records must distinguish checks actually run from skipped platform checks. Windows distributable builds, package smoke checks and asset integrity validation still run to produce verified artifacts.
