# Agent Contract (Codex + MCP)

## Goals
- Точные правки с сохранением стиля.
- Покрытие тестами ≥80%.
- Строгая типизация и авто-формат.

## Tools (через MCP)
- repo-tools: git_status, lint_js, test_js, format_js
- tests-runner: pytest_quick, ruff_check

## Constraints
- Секреты — только из .env / VS Code Secrets.
- Миграции/infra — только по задаче (отдельные PR).

## Review checklist
- Линтер/форматтер ✓
- Тесты обновлены/новые ✓
- Conventional Commits ✓
