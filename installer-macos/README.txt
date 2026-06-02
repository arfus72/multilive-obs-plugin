Multi Live OBS Plugin - macOS

Installation streamer:
1. Ferme OBS Studio.
2. Decompresse le paquet macOS.
3. Double-clique "Install Multi Live OBS.command".
4. Si macOS bloque l'ouverture, clic droit > Ouvrir.
5. Redemarre OBS Studio.

Securite:
- Le paquet ne contient aucun token streamer.
- install-report-macos.txt liste les SHA256 apres installation.
- macOS verifiera la signature du bundle si le paquet est signe/notarise.

Build requis pour produire le payload macOS:
- macOS 12+
- Xcode 16+
- CMake 3.30+
- OBS Plugin Template preset macos

Pour une distribution propre hors avertissement Gatekeeper:
- signer avec Developer ID Application;
- notariser le paquet avec Apple notarytool;
- stapler le ticket de notarisation.
