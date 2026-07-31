# Current J11 HMI application

Use this ELF with the unchanged hardware files in
`release/26G_HMI_J11_5b6ecdf/artifacts/`:

- `25g_2026g_hmi_j11.bit`
- `25g_2026g_hmi_j11.xsa`
- `ps7_init.tcl`

Program `HMI/26G.HMI` into the display separately. Do not use the old ELF in
the frozen release with the current three-decimal HMI.

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| `hmi_candidate_app.elf` | 2,223,044 | `5021C9ACA3C277A134650C7F4F42C1AADDD8DF15CD0EF198CDE778BCC224187E` |
| `../../HMI/26G.HMI` | 7,743,543 | `E8BA47E75D6E21D494C0E016C71AB861D3B4863A7DF87CB1ABE34B88AEFF8BFC` |

The ELF passed a clean Vitis 2020.2 platform/BSP/application build. The HMI
rendering and calibrated measurement behavior passed board testing before the
final precision-only display change; visually confirm the three fractional
digits after programming the current HMI.
