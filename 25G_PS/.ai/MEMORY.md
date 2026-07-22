# 2025G PS Memory

## Persistent rules

- Read .ai/PLagriculture.md and active BSP xparameters.h before changing hardware-facing code.
- COMMIT_SEQ is always the final DDS control write.
- Invalidate DMA receive buffers after S2MM completion and before CPU access.
- Build an arbitrary waveform table while DDS is stopped, then atomically start it.
- Active main remains FreeRTOS Hello World until a verified replacement is built.
- Packaged IPs are independently verified but not integrated into the active top level.
- Use repository-root CODEX_START.md for each new session; do not hard-code a clone path.
