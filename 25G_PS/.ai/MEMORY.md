# 2025G PS Memory

## Persistent rules

- Read .ai/PLagriculture.md and active BSP xparameters.h before changing hardware-facing code.
- COMMIT_SEQ is always the final DDS control write.
- Invalidate DMA receive buffers after S2MM completion and before CPU access.
- Build an arbitrary waveform table while DDS is stopped, then atomically start it.
- Active main remains FreeRTOS Hello World until a verified replacement is built.
- Packaged IPs are independently verified but not integrated into the active top level.
- Use repository-root FPGA_CodexPrompt.md for each new session; do not hard-code a clone path.

- A Vitis workspace is machine-local generated state, not portable source. If platform read reports a missing HwDb after a workspace move, the platform descriptor may retain an obsolete absolute XSA handoff path. Close Vitis and create a new platform from the current XSA with script/ps_create_platform_from_xsa.tcl; do not hand-edit generated platform descriptors or BSP files.

- A newly created Vitis platform can have a valid export directory but no archived BSP libraries when only domain generation has run. Before application linking, run the parameterized platform library-generation script and verify the exported libxil.a and libfreertos.a files; otherwise gcc may compile all sources and fail only at the final link stage.

- If target compilation succeeds but final linking reports undefined references to sqrtf, sinf, cosf, floorf, or roundf, add m in the application linker Libraries (-l) setting. This is a linker configuration issue; do not modify the source or BSP.
