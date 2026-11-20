# ovclash

ovclash is a standalone clash-detection library for USD (Universal Scene Description) stages,
with C and Python APIs. It runs outside NVIDIA Omniverse Kit.

A query tests the meshes of a stage, or of two search sets of prim paths, for clashes: meshes
that touch, interpenetrate or come within a tolerance. ovclash reports each clashing pair,
saves results in SQLite or JSON stores, and exports HTML or JSON reports.

> **Note**
>
> ovclash is pre-release software. A minor or patch release can carry a breaking change.

## Get ovclash

This repository holds no source code. ovclash 0.2.2 ships as a Python wheel and as a C/C++ SDK
archive for Windows x86-64, Linux x86-64 and Linux AArch64. [BUILD.md](BUILD.md) lists the
downloads and the install steps.

## License

See [LICENSE.txt](LICENSE.txt).
