Multi Live OBS Plugin - Linux

Installation streamer:
1. Ferme OBS Studio.
2. Decompresse le paquet Linux.
3. Ouvre un terminal dans ce dossier.
4. Lance:
   chmod +x install.sh
   ./install.sh
5. Redemarre OBS Studio.

Securite:
- Le paquet ne contient aucun token streamer.
- install-report-linux.txt liste les SHA256 apres installation.
- Tu peux scanner le dossier avec ClamAV:
  clamscan -r .

Build requis pour produire le payload Linux:
- Ubuntu 24.04 x86_64
- OBS Studio installe depuis le PPA OBS
- cmake, ninja-build, pkg-config, build-essential, jq, zsh
