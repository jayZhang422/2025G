# 2025G PS Style

- Preserve public names and existing parameter defaults.
- Use BSP XPAR_* symbols, typed structs, explicit bounds, and explicit status returns.
- Keep raw hardware access in HAL/driver modules; algorithms must not use PL base addresses directly.
- Do not edit generated BSP sources or generated Debug/Release makefiles.
- Keep reusable instructions repository-relative; root CODEX_START.md is the only new-session entry.
