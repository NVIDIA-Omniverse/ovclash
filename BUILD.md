# Getting ovclash 0.2.2

ovclash ships as prebuilt packages; there is no source to build. The Python wheels are on
[PyPI](https://pypi.org/project/ovclash/0.2.2/), and the SDK archives are attached to the
[v0.2.2 release](https://github.com/NVIDIA-Omniverse/ovclash/releases/tag/v0.2.2) on GitHub.

## Requirements

- Windows x86-64, Linux x86-64 or Linux AArch64. The Linux packages need glibc 2.35 or newer, as
  on Ubuntu 22.04.
- Python 3.10 or newer.
- On Windows, the
  [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).

## Python wheel

```bash
python -m pip install ovclash==0.2.2
```

pip picks the wheel for your platform from [PyPI](https://pypi.org/project/ovclash/0.2.2/#files)
and also installs `ovstage`, the runtime through which ovclash reads USD, at the exact version the
wheel pins. To install a downloaded wheel instead, pass its file to `python -m pip install`.

| Platform | Wheel |
| --- | --- |
| Windows x86-64 | [ovclash-0.2.2-py3-none-win_amd64.whl](https://files.pythonhosted.org/packages/py3/o/ovclash/ovclash-0.2.2-py3-none-win_amd64.whl) |
| Linux x86-64 | [ovclash-0.2.2-py3-none-manylinux_2_35_x86_64.whl](https://files.pythonhosted.org/packages/py3/o/ovclash/ovclash-0.2.2-py3-none-manylinux_2_35_x86_64.whl) |
| Linux AArch64 | [ovclash-0.2.2-py3-none-manylinux_2_35_aarch64.whl](https://files.pythonhosted.org/packages/py3/o/ovclash/ovclash-0.2.2-py3-none-manylinux_2_35_aarch64.whl) |

`python -m ovclash --help` checks the install.

## SDK archive

The SDK archive holds the C and C++ headers and libraries, the CMake and pkg-config files, the
Python packages, the samples and the documentation.

| Platform | SDK archive |
| --- | --- |
| Windows x86-64 | [ovclash-0.2.2-windows-x86_64.zip](https://github.com/NVIDIA-Omniverse/ovclash/releases/download/v0.2.2/ovclash-0.2.2-windows-x86_64.zip) |
| Linux x86-64 | [ovclash-0.2.2-linux-x86_64.tar.gz](https://github.com/NVIDIA-Omniverse/ovclash/releases/download/v0.2.2/ovclash-0.2.2-linux-x86_64.tar.gz) |
| Linux AArch64 | [ovclash-0.2.2-linux-aarch64.tar.gz](https://github.com/NVIDIA-Omniverse/ovclash/releases/download/v0.2.2/ovclash-0.2.2-linux-aarch64.tar.gz) |

The release also carries `SHA256SUMS.txt`, which lists the SHA-256 checksum of each archive.

The SDK archive does not bundle `ovstage`. Extract the archive, then install the version it pins
from the extracted `ovclash/` folder:

```bash
python -m pip install -r requirements.txt
```

The `README.md` in that folder covers linking against ovclash, building the samples and the
documentation.
