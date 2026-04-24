# Methodology Analysis Phase

The RLCR loop has reached its exit point.

**Exit reason**: maxiter - Reached max iterations (1) without completion
**Rounds completed**: 1 of 1

Before the loop fully exits, please perform a methodology improvement analysis. This analysis helps improve the Humanize development methodology itself -- it is NOT about the project you just worked on.

## Instructions

### 1. Spawn an Opus Agent for Sanitized Analysis

Use the Agent tool with `model: "opus"` to spawn an analysis agent. Give it this task:

**Agent prompt**: Read the development records in `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30`:
- All files matching `round-*-summary.md`
- All files matching `round-*-review-result.md`

Analyze these records from a **pure methodology perspective** and write your findings to `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/methodology-analysis-report.md`.

**CRITICAL SANITIZATION RULES** - The report MUST NOT contain:
- File paths, directory paths, or module paths
- Function names, variable names, class names, or method names
- Branch names, commit hashes, or git identifiers
- Business domain terms, product names, or feature names
- Code snippets or code fragments of any kind
- Raw error messages or stack traces
- Project-specific URLs or endpoints
- Any information that could identify the specific project

**Focus areas for analysis**:
- Iteration efficiency: Were rounds productive or did they repeat similar work?
- Feedback loop quality: Did reviewer feedback lead to meaningful improvements?
- Stagnation patterns: Were there signs of going in circles?
- Review effectiveness: Did reviews catch real issues or create false positives?
- Plan-to-execution alignment: Did execution follow the plan or drift?
- Round count vs. progress ratio: Was the number of rounds proportional to progress?
- Communication clarity: Were summaries and reviews clear and actionable?

**Output format**: Write a structured report with methodology improvement suggestions. Each suggestion should describe a general pattern observed and a concrete improvement to the RLCR methodology. If no improvements are found, write a brief note saying the methodology worked well for this session.

### 2. Read the Analysis Report

After the agent completes, read `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/methodology-analysis-report.md`. ALL subsequent user-facing content MUST be derived solely from this report -- do NOT reference raw development records directly.

### 3. Handle Results

**If no improvements found**: Briefly inform the user that the methodology analysis found no significant improvement suggestions. Then write a completion note to `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/methodology-analysis-done.md` and exit.

**If improvements found**:

a) Report to the user:
   - Brief summary of the exit reason (maxiter: Reached max iterations (1) without completion)
   - Methodology improvement suggestions from the report

b) Use `AskUserQuestion` to ask if the user would like to help improve Humanize by opening a GitHub issue with these suggestions. Emphasize:
   - This is completely voluntary
   - The content is fully sanitized (no project-specific information)
   - It helps improve the methodology for everyone

c) **If user declines**: Thank them, write completion marker to `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/methodology-analysis-done.md`, and exit.

d) **If user agrees**:
   - Draft a GitHub issue title and body from the analysis report
   - Show the draft via a second `AskUserQuestion` for the user to review and confirm
   - If confirmed: run `gh issue create --repo humania-org/humanize --title "..." --body "..."`
   - If `gh` is not available, provide the title and body so the user can create the issue manually
   - Write completion marker to `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/methodology-analysis-done.md` and exit

### 4. Completion Marker

You MUST write meaningful content to `/home/zhaosiying/codebase/compiler/llvm-project-22.1.3-vreg-pressure-aware-sched/.humanize/rlcr/2026-04-24_23-29-30/methodology-analysis-done.md` before exiting. This file signals that the analysis phase is complete. A brief summary of what was done (e.g., "Analysis complete, no suggestions" or "Analysis complete, issue filed") is sufficient.
