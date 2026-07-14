
# Vortigaunt

[![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)
[![GitHub Stars](https://img.shields.io/github/stars/iQuitt/Vortigaunt?style=social)](https://github.com/iQuitt/Vortigaunt)
[![GitHub Issues](https://img.shields.io/github/issues/iQuitt/Vortigaunt)](https://github.com/iQuitt/Vortigaunt/issues)
[![Version](https://img.shields.io/badge/version-1.1.2b-green.svg)](https://github.com/iQuitt/Vortigaunt/releases)
![Qt Version](https://img.shields.io/badge/Qt-6.8+-41CD52?style=flat&logo=qt&logoColor=white)
[![Discord](https://img.shields.io/discord/1463565216485867622?style=flat&logo=discord&logoColor=white&label=discord&color=5865F2)](https://discord.gg/PZ9JzgHHKa)


## What is Vortigaunt?

Vortigaunt is a Porting tool for Goldsource engine.

This Project Main Purpose is Convert game assets from various engines to GoldSrc format for easy porting and modding.

## Features

### Model Exports
- **LTB to SMD** (Only Windows) - Export LithTech Models to Valve SMD with Animations and Bone [Watch Video](https://youtu.be/I2yra8eqjds?t=18)
- **GR2 to SMD** (Only Windows) - Export Granny2 Models to Valve SMD with  Animations, Bone and Texture [Watch Video](https://youtu.be/I2yra8eqjds?t=200)
  - Tested with Metin2 assets
  - More games coming soon
- **Auto-Rig** - A Bone Assingment System [Watch Video](https://www.youtube.com/watch?v=dmAm790ER0c)

### Textures
- **DTX Viewer & Extract** (Only Windows) - View and Extract LithTech Engine textures
- **WAD Maker** - Create and Edit Wad file. You can add JPG/DDS/PNG files to WAD file. Vortigaunt will convert to BMP. [Open Image](https://i.hizliresim.com/niplzjf.png)
- **VTF Viewer** - View and Extract Valve VTF files

### Game Archive Files
- **REZ File** - Extract and browse LithTech REZ archives (Tested only Crossfire)
- **PAK File** - Extract and browse PAK files from Counter Strike Online
- **XFS File** - Extract and browse XFS files from Wolfteam (it May work other Softynx games etc: Rakion) [Watch Video](https://youtu.be/lNnPhTMf2fs)
- **GMA File** - Extract and browse Garry's Mod GMA files
- **VPK File** - Extract and browse Valve VPK files

### Sprite Management
- **Sprite Viewer** - View, edit, and create GoldSrc sprites [Watch Video](https://youtu.be/q3DvdgdLPls)
- **CSO/CSN/CSOL Sprite Fix** - Extract DDS-based sprites (v3) to GoldSrc format (v2)
- **LithTech Sprite View** (Only Windows) - View Lithtech Engine sprite and Extract as Goldsrc Sprite)

### Other
- **League of Legends Integration** - Download champion models without installing the game Via [Khada](https://modelviewer.lol/)
  - The logic is simple. Vortigaunt will download and start displaying your chosen model from Khada, and you can quickly convert it to SMD, including textures, bones, and animations. [Watch Video](https://youtu.be/45ddtHjq5Cg)

- Convert MP3/OGG to WAV (16 Bit Resolution, 22050 Sampling rate and Mono Channel)
- DTX Thumbnail Extension.
	- View DTX files as thumbnails in Windows Explorer
	- View VTF Files as thumbnails in Windows Explorer

### Support Languages
- English
- Turkish
- Spanish (Thanks to [Maikolartzx](https://hizliresim.com/tcbinbd) for Spanish Support)
- Polish  (Thanks to [Majne](https://hizliresim.com/fxnpkbr) for Polish Support)

## Support the Project

If you find Vortigaunt useful and want to support its development:

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-Support-yellow.svg?style=for-the-badge&logo=buy-me-a-coffee)](https://www.buymeacoffee.com/iQuitt)

## Requires for building
### Windows

| Component | Details |
|-----------|---------|
| **Visual Studio 2019/2022/2026** | [Download](https://visualstudio.microsoft.com/downloads/) - Select "Desktop development with C++" |
| **Qt 6.8+** | [Qt Online Installer](https://www.qt.io/download-qt-installer) - Select MSVC 2019/2022 64-bit + Qt Multimedia |
| **CMake 3.15+** | [Download](https://cmake.org/download/) or included with Visual Studio |

### Linux (Ubuntu/Debian)
```bash
sudo apt install -y \
  build-essential cmake ninja-build \
  qt6-base-dev qt6-tools-dev qt6-multimedia-dev \
  qt6-l10n-tools libgl1-mesa-dev libxkbcommon-dev
```


## Building
Clone The Source Code: ````git clone https://github.com/iQuitt/Vortigaunt````
### Windows 
```bash

cmake -B build -G "Visual Studio 17 2022" -A x64

cmake --build build --config Release

````

### Linux
```bash
mkdir build && cd build

cmake .. -DCMAKE_BUILD_TYPE=Release

cmake --build . -j$(nproc)


Special Thanks To:
- Luís Leite for Counter Strike Online PAK (https://git.sr.ht/~leite/cso-pak)
- Kungfulon for Crossfire REZ  (https://gist.github.com/kungfulon/dfa49323eb7a55db964f10174e57c19f)
- Luigi Auriemma for XFS logic (https://aluigi.altervista.org/bms/xenesis.bms)
- YoungFine0825 for LTB2FBX and DTX to TGA (https://github.com/YoungFine0825/LTB2FBX)
	




