---
description: Continue building the quant engine from TASKLIST.md until a gate, a blocker, or the end
argument-hint: [optional stage number, e.g. 3]
---

Continue the project autonomously.

1. Read CLAUDE.md, then TASKLIST.md, then NEEDS_HUMAN.md (act on any answered item), then the SPEC.md sections you need.
2. If an argument was given, start at that stage: $ARGUMENTS. Otherwise start at the first unchecked task.
3. Work task by task. After each task, run the real build and the relevant tests, and only then tick the box and append a short factual line to the progress log.
4. At each stage gate, run the gate commands. In `auto` mode, record the result and carry on to the next stage. In `confirm` mode, stop and write a summary to NEEDS_HUMAN.md.
5. Ask Sidh something only in the situations CLAUDE.md lists, via NEEDS_HUMAN.md. Otherwise decide, and log non-obvious decisions in DECISIONS.md.
6. When you stop, end with a summary of at most 10 lines: done, verified, blocked, next.
