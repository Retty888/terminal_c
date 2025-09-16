# VS Code + OpenAI Codex + MCP Starter

## Быстрый старт
1. Установи расширение OpenAI Codex в VS Code.
2. Скопируй этот шаблон в корень репозитория.
3. `npm i` (для JS/TS), `pip install ruff pytest` (для Python).
4. Открой папку в VS Code — MCP-сервера поднимутся из `.vscode/settings.json`.
5. В чате агента попроси: «Сгенерируй план, сделай дифф с тестами, прогони lint/test».

## MCP инструменты (CLI)
- Node: `node mcp/repo-tools/server.js git_status|lint_js|test_js|format_js`
- Python: `python mcp/tests/server.py pytest_quick|ruff_check`
