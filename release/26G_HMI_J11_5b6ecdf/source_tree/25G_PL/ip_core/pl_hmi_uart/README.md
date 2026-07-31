# Portable J11 HMI UARTLite

Use [`usage/README.md`](usage/README.md) for the contract and integration steps.

The portable source is under `src/`; it wraps the Vivado 2020.2 vendor
`axi_uartlite:2.0` IP and deliberately has no generated `.xci`, `.gen`, `.runs`,
or `component.xml` of its own. The parent Block Design owns address assignment
and the J11 board constraints.
