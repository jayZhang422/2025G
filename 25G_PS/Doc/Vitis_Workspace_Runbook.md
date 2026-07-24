# Vitis workspace and platform recovery runbook

## Purpose

This runbook recovers a Vitis 2020.2 platform when an existing project cannot be imported after moving to a new workspace. It applies to the current Zynq-7020 design and is reusable for future projects after substituting the processor, OS, and XSA path.

## Known failure signatures

| Vitis message | Meaning | Correct response |
| --- | --- | --- |
| `No active platform present, please create one.` | The current workspace has no registered active platform. | Import or create a platform before running `platform config -updatehw`. |
| `platform read ... platform.spr` and `Hw specification ... does not have the HwDb` | The platform descriptor's absolute XSA handoff path is stale or invalid. | Do not edit the generated `.spr`; create a clean platform from the current XSA in a dedicated workspace. |
| New Platform Project has no selectable processor after selecting an XSA. | The GUI failed to parse or refresh the XSA metadata. | Use the XSCT script below, which validates the XSA through HSI first. |

The 2026-07-24 failure used the stale path `E:/7020_Project/25G/25G_PL/top.xsa`. The current XSA is `25G_PL/top.xsa` under the repository root.

## Safe workspace layout

Keep source and workspace separate in normal use. A workspace is machine-local generated state and must not be committed. For this repository, a local workspace can be placed under `VitisWorkspaces/<workspace-name>` because that directory is ignored by Git.

Do not run Vitis GUI and XSCT against the same workspace at the same time.

## Recovery command

1. Close Vitis completely.
2. Use a new or empty workspace directory. Do not reuse a directory containing an existing `Identification_platform` folder.
3. In PowerShell, run the preflight:

```powershell
& 'D:\Vivado_install\Vitis\2020.2\bin\xsct.bat' `
  'D:\DianSai\2023H_25\2025G_calvin\25G_PS\script\ps_create_platform_from_xsa.tcl' `
  --workspace 'D:/DianSai/2023H_25/2025G_calvin/VitisWorkspaces/2025G_A' --check
```

4. Expected preflight result:

```text
CHECK PASSED: XSA exposes ps7_cortexa9_0
```

5. Create the platform by removing `--check`:

```powershell
& 'D:\Vivado_install\Vitis\2020.2\bin\xsct.bat' `
  'D:\DianSai\2023H_25\2025G_calvin\25G_PS\script\ps_create_platform_from_xsa.tcl' `
  --workspace 'D:/DianSai/2023H_25/2025G_calvin/VitisWorkspaces/2025G_A'
```

The script creates:

```text
Identification_platform
processor: ps7_cortexa9_0
OS/domain: freertos10_xilinx / freertos10_xilinx_domain
XSA: <repository>/25G_PL/top.xsa
```

It also generates the platform BSP/domain. This is expected for a new platform, but it does not overwrite the legacy platform directory under `25G_PS`.

## Success criteria

- XSCT prints `DONE: active platform Identification_platform`.
- The new workspace contains `Identification_platform/platform.spr`.
- `platHandOff` in that descriptor resolves to the current repository `25G_PL/top.xsa`, not an old clone path.
- Generated `xparameters.h` contains the expected hardware identifiers, including `XPAR_AXI_DMA_ADC_*` and `XPAR_AXI_BRAM_CTRL_0_*`.

## After recovery

Open Vitis with the newly created workspace and import the application/system projects only after the platform is present. Do not manually edit generated BSP drivers, `xparameters.h`, Makefiles, or `platform.spr`.

If the XSA changes later, repeat this procedure in a fresh workspace or update an already active platform through Vitis/XSCT. Ensure the XSA, bitstream, PS initialization files, and BSP used for an ELF all originate from the same hardware export.

## BSP static-library generation

Domain generation alone may leave the exported library directory present but empty. Before importing or building an application, explicitly generate and archive the BSP libraries for the existing platform:

```powershell
& 'D:\Vivado_install\Vitis\2020.2\bin\xsct.bat' `
  'D:\DianSai\2023H_25\2025G_calvin\25G_PS\script\ps_generate_platform_libs.tcl' `
  --workspace 'D:/DianSai/2023H_25/2025G_calvin/VitisWorkspaces/2025G_A'
```

The export directory must contain both `bsplib/lib/libxil.a` and `bsplib/lib/libfreertos.a`. If either library is missing, application compilation may succeed but the final link will fail with `cannot find -lxil` or `cannot find -lfreertos`.
