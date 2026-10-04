---
name: paper-results
description: Build the paper-facing results package (LaTeX table fragments, figures, provenance, hand-off notes) deterministically from schema-valid raw evidence records, with correct claim levels and explicit "not available" cells for missing evidence. Use when the user asks for results, tables, figures, or numbers for the BoundFin paper.
---

# Build the paper results package

Output goes to `evidence/derived/paper/<version>/`, where `<version>` is a new directory (for example `v0001`). Never modify an earlier version. Never edit `../dsl_research_paper/` (hook-enforced). The manuscript is updated from a session in that repository, using this package.

## 1. Inventory and validate inputs

- List the raw records under `evidence/raw/` that are relevant to each requested output.
- Validate each one against its schema in `schemas/`. Exclude invalid records from the package, list them in the package, and report them. Do not repair them.
- Determine each record's toolchain and machine from its recorded identity.

## 2. Decide eligibility per output

Use `EXPERIMENT_IMPLEMENTATION_SPEC.md` §7.5 and `CLAUDE.md` §1:

| Output | Eligible when |
|---|---|
| G1/G2 coverage, counterexample tables | correctness records exist for the declared suite/domain |
| Exact/upper/capacity bounds, slack/tightness, memory columns | analyzer certificates and evaluator traces for the same artifact hashes |
| K1 known-answer and closed-form cross-check | K1 records; the closed forms come from paper §9.3 |
| RQ3/RQ4/RQ5, tails, D0–D5, counters, disassembly tables | frozen artifacts on the experimental machine, gates passed, Phase 15 freeze done, pilot rows excluded |

An ineligible output becomes a stub table whose cells read `not available: <reason>`. Development-host timings are never tabulated.

## 3. Generate

- All computation lives in versioned scripts under `analysis/`. Python is permitted there only. The scripts read raw records and write derived files. No number is typed by hand.
- Statistical procedures must be exactly those preregistered (paper §9.7; DEC-011). A failed diagnostic yields "unresolved"; never substitute another method silently.
- Keep MS, PO, and SIMD families separate. Keep exact, upper, and capacity bounds separate. Keep cumulative, peak, frame, output, and total memory separate.

## 4. Provenance and determinism

- Write `provenance.json` containing: input record paths with SHA-256, script paths with SHA-256, the analysis environment (interpreter and library versions), the command line, seeds, and output SHA-256.
- Regenerate into a scratch directory and compare hashes. A mismatch is a failure to report, not to paper over.

## 5. Write `HANDOFF.md`

- For each fragment: the target paper location (for example `sections/10-results-stubs.tex`, Results subsection; or Table `hypothesis-criteria` outcomes), and its claim level.
- The proof-obligation status table copied unchanged from `spec/paper/proof-obligations.md`. Tests never change it.
- Every `SPEC_AMENDMENTS.md` entry marked "Propagate: Yes" that the manuscript has not yet absorbed.
- Deviations, invalid runs, exclusions, and missing data.

## 6. Report

Summarise to the user what was generated, what is a stub and why, and the package path. Wording follows `CLAUDE.md` §7.
