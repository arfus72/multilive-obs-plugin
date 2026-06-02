Multi Live OBS Plugin - Installation streamer

Ce paquet installe le plugin OBS natif Multi Live.

Installation:
1. Ferme OBS Studio.
2. Clic droit sur "Installer Multi Live OBS.bat".
3. Choisis "Executer en tant qu'administrateur".
4. Laisse le scan Microsoft Defender se terminer.
5. Redemarre OBS Studio.

Ce que le plugin fait:
- garde la source "Multi Live Signature" visible et verrouillee;
- remet la bonne URL, taille et position;
- supprime les doublons Alert Box qui peuvent relire les follows;
- aide Multi Live a verifier que la signature est presente.

Securite:
- aucun token n'est inclus dans ce paquet;
- l'installateur genere install-report.txt avec les SHA256;
- Microsoft Defender scanne le paquet localement pendant l'installation.

Si Windows SmartScreen affiche un avertissement:
le paquet n'est pas encore signe par certificat editeur. Verifie le fichier
install-report.txt et le SHA256 fourni par Multi Live avant d'installer.
