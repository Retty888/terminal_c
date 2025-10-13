# Global Rules

## Instruction Hierarchy
- Follow instruction precedence: system > developer > user > repository > agent documentation.
- When instructions conflict, stop and ask the user to reconcile before acting.
- Record key assumptions or blockers instead of guessing outcomes.

## Communication
- Keep responses concise and actionable, referencing files as `path:line` when needed.
- Follow the CLI final message guidelines: lead with context, list tests, and add next steps only when useful.
- Ask for clarification only when requirements remain ambiguous after reasonable inference.

## Execution
- Use the `shell` tool with an explicit `workdir`; prefer `['bash','-lc','<cmd>']` unless a Windows-specific action needs PowerShell.
- Build multi-step plans for non-trivial tasks, skip planning for only the simplest work, and keep plans updated as progress is made.
- Summarize relevant command output instead of dumping long logs unless the user requests it.
- Work within the active sandbox; approvals are unavailable (`approval_policy=never`).

## Safety
- Never expose secrets; read sensitive values only from approved sources such as `.env` or VS Code Secrets.
- Avoid destructive actions (e.g., `rm`, `git reset`, force pushes) unless the user explicitly authorizes them.
- Preserve user-authored changes; do not revert or overwrite unrelated edits in a dirty worktree.

## Quality
- Make precise, minimal edits that respect the existing style and formatting rules of each file.
- Default to ASCII text unless a file already relies on another encoding.
- Run relevant linters and tests whenever behavior might change, and note any skipped validation with reasons.

## Tooling
- Prefer the provided MCP tools (`repo-tools`, `tests-runner`) to run linting, formatting, and tests when applicable.
- Use `rg` or `rg --files` for search tasks before falling back to slower tools.
- Capture validation steps, results, and residual risks in the final response.