# Multi Live OBS Plugin - releases Linux/macOS

Le code du plugin est portable Windows/Linux/macOS. Les binaires doivent etre produits sur chaque OS cible.

## Windows

Artifact actuel:

- `dist/MultiLiveOBSPlugin-StreamerInstaller.zip`
- plugin: `multilive-signature-guard.dll`

## Linux Ubuntu x86_64

Prerequis build:

```bash
sudo add-apt-repository --yes ppa:obsproject/obs-studio
sudo apt update
sudo apt install --no-install-recommends \
  build-essential cmake ninja-build pkg-config jq zsh git \
  libgles2-mesa-dev libsimde-dev obs-studio \
  qt6-base-dev libqt6svg6-dev qt6-base-private-dev
```

Build:

```bash
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
./.github/scripts/package-ubuntu --target ubuntu-x86_64
```

Installer streamer:

- placer le `.so` compile dans `installer-linux/payload/`;
- zipper `installer-linux/`;
- le streamer lance `./install.sh`.

## macOS Universal

Prerequis build:

- macOS 12+
- Xcode 16+
- CMake 3.30+

Build:

```zsh
cmake --preset macos
cmake --build --preset macos
```

Package:

```zsh
CI=1 ./.github/scripts/package-macos
```

Installer streamer:

- placer `multilive-signature-guard.plugin` dans `installer-macos/payload/`;
- zipper `installer-macos/`;
- le streamer lance `Install Multi Live OBS.command`.

## Signature

Windows:

- certificat Code Signing OV/EV;
- signer la DLL et un futur installateur `.exe` avec `signtool`;
- publier SHA256 + rapport Defender/VirusTotal.

macOS:

- Apple Developer Program;
- Developer ID Application;
- `codesign`, `notarytool`, puis `stapler`.

Linux:

- publier SHA256;
- optionnel: paquet `.deb` signe avec une cle GPG de depot.
