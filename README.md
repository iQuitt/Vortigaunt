
# Vortigaunt <img align="right" width="128" height="128" src="https://i.ibb.co/ps8QRRs/yeerrrr.png" alt="VortigauntTool icon" />

[![License](https://img.shields.io/badge/license-GPLv3-blue.svg)](LICENSE)
[![GitHub Stars](https://img.shields.io/github/stars/iQuitt/Vortigaunt?style=social)](https://github.com/iQuitt/Vortigaunt)
[![GitHub Issues](https://img.shields.io/github/issues/iQuitt/Vortigaunt)](https://github.com/iQuitt/Vortigaunt/issues)
[![Version](https://img.shields.io/badge/version-0.13.3b-green.svg)](https://github.com/iQuitt/Vortigaunt/releases)
![Qt Version](https://img.shields.io/badge/Qt-6.10.1-41CD52?style=flat&logo=qt&logoColor=white)

## What is Vortigaunt?

Vortigaunt is a Porting tool for Goldsource engine.

This Project Main Purpose is Convert game assets from various engines to GoldSrc format for easy porting and modding.

## Features

### Model Exports
- **LTB to SMD** - Export LithTech Models to Valve SMD with Animations and Bone [Watch Video](https://youtu.be/I2yra8eqjds?t=18)
- **GR2 to SMD** - Export Granny2 Models to Valve SMD with  Animations, Bone and Texture [Watch Video](https://youtu.be/I2yra8eqjds?t=200)
  - Tested with Metin2 assets
  - More games coming soon
- **Auto-Rig System** - Intelligent bone assignment using CS 1.6 skeleton reference [Watch Video](https://www.youtube.com/watch?v=dmAm790ER0c)

### Textures
- **DTX Viewer & Extract** - View and Extract LithTech Engine textures
- **WAD Maker** - Create and Edit Wad file. You can add JPG/DDS/PNG files to WAD file. Vortigaunt will convert to BMP. [Open Image](https://i.hizliresim.com/niplzjf.png)

### Game Archive Files
- **REZ File** - Extract and browse  LithTech REZ archives (Tested only Crossfire)
- **PAK File** - Extract and browse PAK files from Counter Strike Online
- **XFS File** - Extract and browse XFS files from Wolfteam (it May work other Softynx games etc: Rakion) [Watch Video](https://youtu.be/lNnPhTMf2fs)

### Sprite Management
- **Sprite Viewer** - View, edit, and create GoldSrc sprites [Watch Video](https://youtu.be/q3DvdgdLPls)
- **CSO/CSN/CSOL Sprite Fix** - Extract DDS-based sprites (v3) to GoldSrc format (v2)
- **LithTech Sprite View** - View Lithtech Engine sprite and Extract as Goldsrc Sprite [Watch Video](https://youtu.be/8bBcSBga1ag)

### Game-Specific Features
- **League of Legends Integration** - Download champion models without installing the game Via [Khada](https://modelviewer.lol/)
  - The logic is simple. Vortigaunt will download and start displaying your chosen model from Khada, and you can quickly convert it to SMD, including textures, bones, and animations. [Watch Video](https://youtu.be/45ddtHjq5Cg)

### Goldsrc Audio Convert
- Convert MP3/OGG to WAV (16 Bit Resolution, 22050 Sampling rate and Mono Channel)

### Support Languages
- English
- Turkish
- More languages coming soon

## Support the Project

If you find Vortigaunt useful and want to support its development:

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-Support-yellow.svg?style=for-the-badge&logo=buy-me-a-coffee)](https://www.buymeacoffee.com/iQuitt)

## Requires for building
### Windows

| Component | Details |
|-----------|---------|
| **Visual Studio 2019/2022/2026** | [Download](https://visualstudio.microsoft.com/downloads/) - Select "Desktop development with C++" |
| **Qt 6.6+** | [Qt Online Installer](https://www.qt.io/download-qt-installer) - Select MSVC 2019/2022 64-bit + Qt Multimedia |
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
````
## Planned Features

- [ ] GTA:SA Asset Support
- [ ] Metin2 Script Effect 
- [ ] Convert FBX/OBJ/GR2 to Half life .MAP format
- [ ] Unity AnimationClip Support
- [ ] Unity-based Games Asset Support
- [ ] CS2 Asset Support
- [ ] Valorant Asset Support
---
**The Source Code Will be Released As soon As Possible**




