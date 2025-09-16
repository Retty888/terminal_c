#!/usr/bin/env node
import { exec } from "node:child_process";

function run(cmd) {
  return new Promise((resolve, reject) =>
    exec(cmd, { maxBuffer: 1024 * 1024 * 20 }, (e, stdout, stderr) =>
      e ? reject(stderr || e.message) : resolve(stdout || "ok")
    )
  );
}

// минимальный «сервер-инструмент» совместимый с MCP-клиентом VS Code (простая обертка)
// Для демонстрации экспортируем CLI режим: node server.js <tool>
const tools = {
  git_status: () => run("git status --porcelain=v1"),
  lint_js: () => run("npm run lint"),
  test_js: () => run("npm run test --silent"),
  format_js: () => run("npx prettier -w .")
};

async function main() {
  const tool = process.argv[2];
  if (!tool || !tools[tool]) {
    console.log("tools:", Object.keys(tools).join(", "));
    process.exit(0);
  }
  try {
    const out = await tools[tool]();
    process.stdout.write(out);
  } catch (e) {
    process.stderr.write(String(e));
    process.exit(1);
  }
}
main();
