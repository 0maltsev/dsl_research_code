---
name: next-milestone
description: Advance the BoundFin implementation by exactly one verifiable PLAN.md milestone, with gate checks, tests-first implementation, spec audit, and STATUS/PLAN/traceability updates. Use when the user asks to continue the research, implement the next phase or step, or "keep going".
---

# Advance one milestone

Follow `CLAUDE.md` §3–§5 and `AGENTS.md`. Do one milestone per invocation unless the user asks for more.

## 1. Orient

- Read `STATUS.md` (current phase, in-progress milestone, next permitted work, open blockers).
- Read the current phase in `PLAN.md`: inputs, deliverables, tests, acceptance, dependencies, stop conditions.
- Confirm that the previous milestone's acceptance is recorded in `STATUS.md` with fresh evidence. If it is not, re-verify it before going further.

## 2. Gate check

- List every decision-log entry that blocks this phase, directly or through dependencies (see `CLAUDE.md` §3 for the track rules).
- If any blocking entry is still `AUTHOR DECISION REQUIRED`, do not start. Apply the stop-and-ask protocol (`CLAUDE.md` §5): ask with AskUserQuestion, listing the logged alternatives with the recommendation first. Then stop, or switch to an unblocked milestone if one exists.
- Track P work also requires that G1/G2 passed for the exact artifact hashes involved.

## 3. Scope

- Choose the smallest independently verifiable milestone (`N.k`). It must have its own acceptance test.
- In `STATUS.md`, record it as in progress with its acceptance criterion.

## 4. Read and scan for ambiguity

- Read only the documents mapped to this phase in `CLAUDE.md` §2. List the rule IDs the milestone implements.
- For each rule, confirm its behaviour is fully determined by the paper snapshot plus `docs/spec-freeze/` plus amendments. If not, stop and ask. Draft a proposed amendment only as "awaiting author approval".

## 5. Tests first

- Write the narrowest test that demonstrates each rule, named by rule ID (for example `E_FoldErr_body_error_after_k_iterations`). Run it and confirm it fails for the expected reason.
- Add the required negative and boundary cases from `docs/traceability.md` for these rules.

## 6. Implement

- Match the surrounding code. Respect module ownership (`docs/architecture.md`).
- When the milestone is a second implementation (BIR evaluator or validator, native oracle), delegate it to the `independent-oracle` subagent with the spec sections and the forbidden paths.

## 7. Verify

- Build, run the new tests, then the full suite. Run sanitizers (ASan/UBSan) where Phase 1 configured them.
- Validate every generated record against its schema in `schemas/`.
- Run `.claude/hooks/check-snapshot.sh session`.
- Record exact commands and outcomes. Report a failure as a failure, with its output.

## 8. Audit

- Launch the `spec-auditor` subagent with: the milestone ID, the rule IDs, the changed files (`git diff --stat` and `git status`), and the test commands with their outcomes.
- Fix every FAIL finding, then re-run the audit. Report PLAUSIBLE findings to the user rather than silently dismissing them.

## 9. Record

- `STATUS.md`: milestone completed, evidence (commands and results), new open issues, the next concrete step.
- `PLAN.md`: phase progress; mark a phase complete only when all its acceptance criteria and stop conditions are checked with fresh evidence.
- `docs/traceability.md`: the module paths and test families that now exist for each row touched.
- If the milestone revealed a contradiction or clarification, it must already be in `SPEC_AMENDMENTS.md` (author-approved) before dependent code is kept.

## 10. Report

Tell the user, at the correct claim level (`CLAUDE.md` §7):

- what was implemented;
- the tests and the commands that ran;
- what remains open, including any proof obligation whose status is unchanged;
- the next milestone or blocker.

Do not commit unless the user asked.
