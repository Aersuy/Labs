# CLAUDE.md

Project guidelines for Claude when writing or modifying code in this repository.

**Priority:** The Function Requirements below are the most important rules in this file. They are mandatory for all code written or modified. If any other guideline conflicts with them, the Function Requirements win. Verify them first, before any other check, and never skip them to keep a change small.

## Function Requirements (highest priority)

### 1. Size limit
- A function must be **no more than 25 lines** (Excluding blank spaces and brackets).
- Do not delete functionality or compress code (e.g. cramming logic onto one line) to meet the limit. If a function is too long, **split it into multiple smaller functions**.
- Every extracted function must have a meaningful name and a single responsibility. The 25-line limit takes precedence over the preference to avoid helpers (see "Complexity and Duplication").

### 2. Single level of abstraction
- A function operates on **at most one level of abstraction**.
- Do not mix high-level orchestration (e.g. "process the order") with low-level details (e.g. string parsing, index arithmetic) in the same function.

### 3. Single responsibility
- A function does **at most one logical thing**.
- The same principle applies to **classes**: a class has one reason to change.
- If you need "and" to describe what a function or class does, split it.

### 4. Purity placement in the call stack
- **Non-deterministic functions**, and those that **depend on external dependencies** (I/O, network, database, filesystem, clock, randomness, environment), belong **as high as possible** on the call stack.
- **Deterministic functions** (same input -> same output, no side effects) belong **as low as possible** on the call stack.
- Pass external values (time, random seeds, fetched data) down into deterministic functions as arguments rather than fetching them inside.

## Example

Bad: mixes I/O, parsing, and business logic.

```python
def process_report(path):
    with open(path) as f:
        rows = [line.split(",") for line in f]
    total = sum(float(r[2]) for r in rows)
    print(f"Total: {total}")
```

Good: I/O at the top, deterministic logic below, one thing per function.

```python
def process_report(path):                 # impure orchestration (top)
    text = read_file(path)
    rows = parse_rows(text)
    print(format_total(compute_total(rows)))

def read_file(path):                      # external dependency, isolated
    with open(path) as f:
        return f.read()

def parse_rows(text):                     # pure
    return [line.split(",") for line in text.splitlines()]

def compute_total(rows):                  # pure
    return sum(float(r[2]) for r in rows)

def format_total(total):                  # pure
    return f"Total: {total}"
```

## Checklist Before Finishing a Change
Check the function requirements first; they take priority over everything else here.
- [ ] Every function is 25 lines or fewer
- [ ] Each function stays at one level of abstraction
- [ ] Each function and class does one logical thing
- [ ] Impure code is near the top of the call stack; pure code is near the bottom
- [ ] No functionality was removed or compacted to satisfy these rules

## Naming and Readability
- Use descriptive names that communicate intent rather than implementation details.
- Avoid vague names such as data, item, manager, helper, process, and handler when a more precise name is available.
- Avoid abbreviations unless they are widely established in the project or domain.
- Use names that make boolean values read like questions, such as isValid, hasPermission, or canRetry.
- Prefer positive conditions over double negatives.
- Prefer early returns and guard clauses to deeply nested conditionals.
- Use named intermediate variables when they make a non-trivial expression easier to understand.
- Avoid comments that merely restate the code. Explain intent, constraints, invariants, and non-obvious decisions.
- Keep comments accurate when changing code. Remove or update comments that no longer describe the implementation.
- Prefer straightforward control flow over clever, compressed, or unnecessarily generic solutions.
- Do not introduce abstractions unless they reduce duplication, clarify intent, isolate a meaningful responsibility, or make testing easier.

## Architecture and Dependency Management
- Follow the existing architecture and established conventions before introducing new patterns.
- Make dependencies explicit through constructor or function parameters rather than hidden global state.
- Depend on abstractions when they provide a meaningful boundary, enable substitution, or simplify testing. Do not create interfaces for every class by default.
- Keep business logic independent of infrastructure details wherever practical.
- Keep object creation and dependency wiring at the application boundary.
- Avoid service locators, unnecessary dependency containers, and runtime dependency resolution inside business logic.
- Avoid circular dependencies between modules.
- Prefer composition over inheritance unless inheritance expresses a genuine subtype relationship.
- Avoid speculative generalization, premature optimization, and abstractions designed for hypothetical future requirements.
- Reuse existing project abstractions before introducing new ones.
- Do not introduce a new dependency when the existing standard library or project utilities can solve the problem cleanly.
- When a design choice has significant trade-offs, explain the rationale briefly.

## Testing Requirements
- Add or update tests whenever behavior is added, changed, or fixed.
- Always add and run regression tests.
- Test observable behavior and meaningful contracts rather than implementation details.
- Cover normal operation, boundary conditions, invalid inputs, and relevant failure paths.
- Keep tests deterministic. Do not rely on real network services, the current time, random values, or external state unless the test explicitly requires integration testing.
- Inject external dependencies or use suitable test doubles when isolation is necessary.
- Prefer small, independent tests with descriptive names that explain the expected behavior.
- Avoid redundant tests that assert the same behavior without adding meaningful coverage.
- Do not weaken, delete, or bypass a test merely to make a change pass.
- Run the tests after modifying code. Run the full test suite when practical.
- Report which tests were run, which passed, and any tests that could not be executed.
- Never claim that code was tested or verified unless the corresponding checks were actually performed.

## Error Handling and Validation
- Validate untrusted input at system boundaries, such as API endpoints, command-line interfaces, file parsers, and external service integrations.
- Keep validation close to the boundary where data enters the system.
- Use the narrowest appropriate exception or error type.
- Never silently swallow exceptions or ignore failed operations.
- Do not use empty catch blocks or return success-shaped default values when an operation has failed.
- Catch errors only when the current layer can meaningfully handle, translate, recover from, or enrich them.
- Preserve the original error context when rethrowing or wrapping exceptions.
- Include relevant context in error messages without exposing secrets or sensitive data.
- Distinguish invalid input, expected absence, failed operations, and programming errors.
- Do not add defensive checks for impossible states without a concrete reason.
- Avoid redundant validation when a stronger invariant is already guaranteed by a trusted boundary.
- Make failure behavior explicit and consistent with the surrounding codebase.

## Type Safety and Contracts
- Prefer explicit, meaningful types over loosely typed values and unstructured dictionaries or objects.
- Use the type system to express valid states and constraints wherever practical.
- Avoid Any, unchecked casts, reflection, and dynamic typing unless there is a concrete justification.
- Avoid primitive obsession when a domain-specific type materially improves correctness or clarity.
- Define clear input, output, and failure contracts for public methods.
- Use immutable data and read-only interfaces when mutation is not required.
- Represent optional values explicitly instead of using magic values such as -1, empty strings, or zero to mean "missing."
- Handle nullable values explicitly and avoid suppressing nullability warnings without justification.
- Prefer exhaustive handling of enums, discriminated unions, and other closed sets of alternatives.
- Keep public APIs as small and stable as practical.

## Scope Control and Code Changes
- Understand the relevant code and its callers before modifying it.
- Make the smallest coherent change that fully solves the requested problem.
- Preserve existing behavior unless the task explicitly requires changing it.
- Do not refactor unrelated code, rename unrelated symbols, or reformat unrelated files.
- Do not introduce new frameworks, dependencies, design patterns, or configuration options without a clear need.
- Before modifying a public API, check its callers and assess compatibility implications.
- Before removing code, determine whether it is used directly, indirectly, through reflection, configuration, or external integrations.
- Do not duplicate existing functionality when an established implementation can be reused.
- Do not work around a failing test by weakening its assertions or changing unrelated behavior.
- Avoid changing generated files manually unless the task explicitly requires it.
- Inspect the final diff for accidental changes, unrelated edits, and inconsistencies.
- If a broader refactor appears necessary, explain why before expanding the scope of the change.

## Complexity and Duplication
- Prefer the simplest design that satisfies the current requirements.
- Follow the "Rule of Three" as a default: tolerate small amounts of duplication until a pattern has repeated enough to justify an abstraction.
- Do not extract a helper function solely to shorten another function, except where needed to meet the 25-line limit. Extracted functions must have a meaningful name and responsibility.
- Avoid deeply nested conditionals, long chains of boolean expressions, and complicated ternary expressions.
- Replace repeated business rules with a shared implementation when doing so establishes a clear source of truth.
- Avoid unnecessary mutable state and minimize the scope and lifetime of mutable variables.
- Avoid boolean parameters when they cause a function to perform different, unrelated operations.
- Avoid magic numbers and unexplained literals. Use named constants when they communicate domain meaning.
- Do not optimize performance speculatively. Optimize measured bottlenecks while preserving correctness and readability.
- Prefer standard algorithms and established language idioms over hand-written implementations when they are clearer.

## Documentation
- Document public APIs, non-obvious invariants, important architectural decisions, and behavior that cannot be inferred easily from the code.
- Prefer self-documenting code over excessive comments.
- Explain why a non-obvious decision exists rather than paraphrasing what the code does.
- Update relevant documentation when behavior, configuration, interfaces, or usage instructions change.
- Avoid adding documentation that merely duplicates implementation details and will quickly become stale.
- Add examples when they clarify how a public API or complex component should be used.
- Do not create additional documentation files unless they provide a clear benefit or the task requires them.
## IMPORTANT
- Avoid verbosity, every comment should avoid being multiple paragraphs.

## Formatting and Project Conventions
- Follow the existing project style and the applicable language conventions.
- Check for existing formatters, linters, analyzers, and configuration files before introducing new tooling.
- Use the project's configured formatting and linting tools rather than manually approximating their output.
- Do not reformat entire files when only a small section needs modification.
- Keep imports organized according to project conventions and remove unused imports.
- Follow established conventions for file names, class names, methods, constants, and module organization.
- Avoid introducing multiple competing styles or conventions in the same project.
- Keep comments, documentation, and examples consistent with the actual implementation.
- When a change violates an existing convention for a good reason, keep the exception local and explain it where necessary.

## Security
- Treat external input, file contents, network responses, and environment variables as untrusted unless validated.
- Never hardcode credentials, access tokens, private keys, or other secrets.
- Do not log passwords, tokens, sensitive personal data, or other confidential values.
- Use established cryptographic libraries rather than implementing cryptographic primitives from scratch.
- Avoid insecure defaults and fail securely when security-sensitive operations cannot be completed.
- Use parameterized queries and context-appropriate output encoding when interacting with databases or rendering user-controlled content.
- Avoid shell command construction from untrusted input.
- Do not disable security checks, certificate validation, authorization, or input validation merely to make an operation succeed.
- Follow the principle of least privilege.
- Do not introduce security-sensitive behavior without examining its relevant failure modes and trust boundaries.
