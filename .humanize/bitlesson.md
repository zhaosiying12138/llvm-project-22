# BitLesson Knowledge Base

This file is project-specific. Keep entries precise and reusable for future rounds.

## Entry Template (Strict)

Use this exact field order for every entry:

```markdown
## Lesson: <unique-id>
Lesson ID: <BL-YYYYMMDD-short-name>
Scope: <component/subsystem/files>
Problem Description: <specific failure mode with trigger conditions>
Root Cause: <direct technical cause>
Solution: <exact fix that resolved the problem>
Constraints: <limits, assumptions, non-goals>
Validation Evidence: <tests/commands/logs/PR evidence>
Source Rounds: <round numbers where problem appeared and was solved>
```

## Entries

<!-- Add lessons below using the strict template. -->

## Lesson: csv-output-must-force-lf-for-tracked-results
Lesson ID: BL-20260420-csv-lf
Scope: Python benchmark/report scripts that commit generated CSV artifacts
Problem Description: Generated CSV files fail `git diff --check` with trailing whitespace when they are tracked as result artifacts.
Root Cause: Python `csv` writers default to CRLF row terminators; Git reports the carriage return before LF as trailing whitespace in diffs.
Solution: Pass `lineterminator="\n"` to `csv.DictWriter` or equivalent writers before regenerating tracked CSV outputs.
Constraints: This applies to tracked text CSV artifacts; binary or intentionally CRLF fixtures should document and exclude the rule explicitly.
Validation Evidence: `git diff --cached --check` passed after changing the writer and regenerating `raw_samples.csv` and `summary.csv`.
Source Rounds: 0
