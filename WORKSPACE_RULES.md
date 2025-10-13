# Workspace Rules

## Goals
- Deliver precise edits that preserve existing style and intent.
- Keep automated test coverage at or above 80 percent; add or update tests when coverage would fall.
- Enforce strict typing and run auto-formatters on any new or modified code.

## Tooling
- Launch repository helpers with `node mcp/repo-tools/server.js <tool>`; available tools: `git_status`, `lint_js`, `test_js`, `format_js`.
- Run `node mcp/repo-tools/server.js lint_js` and `node mcp/repo-tools/server.js format_js` before completing JS/TS updates.
- Run `node mcp/repo-tools/server.js test_js` or targeted `vitest` commands whenever behavior changes.
- For Python work, run `python mcp/tests/server.py pytest_quick` and `python mcp/tests/server.py ruff_check` as needed.
- Prefer `rg` and `rg --files` for navigation and search.

## Constraints
- Source secrets only from `.env` files or VS Code Secrets; never hard-code credentials.
- Apply database migrations or infrastructure changes only when explicitly requested for this task.
- Keep edits scoped to necessary files and preserve user-authored work already present in the tree.

## Validation Checklist
- Linting and formatting completed; note the commands run.
- Tests updated or added as needed, and report coverage status if available.
- Conventional Commit message prepared when creating commits.

## Workflow Notes
- Default to ASCII unless an existing file uses another encoding; respect `.editorconfig` (LF newlines, two-space indent).
- Capture validation steps, assumptions, and residual risks in the final response.
- Surface open questions promptly rather than guessing.
- Add concise comments only when logic would otherwise be opaque; keep changes minimal and purposeful.