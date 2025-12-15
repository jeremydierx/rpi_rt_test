# Guide de Cross-Compilation pour Raspberry Pi 4

Ce guide détaille la configuration d'un environnement de cross-compilation pour Raspberry Pi 4 (ARM64) depuis WSL2 Ubuntu.

## Introduction

### Qu'est-ce que la cross-compilation ?

La cross-compilation consiste à compiler un programme sur une machine (l'hôte) pour l'exécuter sur une machine différente (la cible).

```
┌─────────────────────┐                    ┌─────────────────────┐
│   HÔTE (WSL2)       │                    │   CIBLE (RPi4)      │
│   x86_64            │  ──── SCP ────▶    │   aarch64           │
│   Ubuntu 22.04      │                    │   Ubuntu 24.04      │
│                     │                    │                     │
│   Compilation       │                    │   Exécution         │
│   5-15 secondes     │                    │                     │
└─────────────────────┘                    └─────────────────────┘
```

### Avantages

| Avantage | Description |
|----------|-------------|
| Rapidité | Compilation 5-10x plus rapide que sur le Pi |
| Ressources | Le Pi reste disponible pour l'exécution |
| Confort | Utilisation de votre IDE favori (VS Code) |
| Productivité | Cycle édition-compilation-test plus court |

## Prérequis

### Environnement WSL2

Ce guide suppose que vous avez déjà WSL2 installé avec Ubuntu 22.04 LTS.

```bash
# Vérifier votre version WSL
wsl --version

# Vérifier votre distribution
lsb_release -a
```

### Outils de base

```bash
# Mise à jour du système
sudo apt update && sudo apt upgrade -y

# Outils de build essentiels
sudo apt install -y build-essential cmake git
```

## Installation de la Toolchain

### Toolchain ARM64 (aarch64)

```bash
# Installation des compilateurs croisés
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Vérification de l'installation
aarch64-linux-gnu-gcc --version
aarch64-linux-gnu-g++ --version
```

Sortie attendue :
```
aarch64-linux-gnu-gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0
```

### Outils binaires associés

```bash
# Outils complémentaires (normalement installés avec la toolchain)
sudo apt install -y binutils-aarch64-linux-gnu

# Vérification
aarch64-linux-gnu-objdump --version
aarch64-linux-gnu-readelf --version
```

## Compilation du projet

### Méthode 1 : Script de déploiement (recommandé)

Le script `deploy.sh` automatise tout le processus :

```bash
# Rendre le script exécutable (une seule fois)
chmod +x scripts/deploy.sh

# Compiler
./scripts/deploy.sh

# Compiler et déployer sur le Pi
RPI_HOST=ubuntu@192.168.1.100 ./scripts/deploy.sh --deploy
```

### Méthode 2 : CMake manuel

```bash
# Configuration avec le fichier toolchain
cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-rpi4-aarch64.cmake

# Compilation (utilise tous les cœurs disponibles)
cmake --build build -j$(nproc)

# Le binaire est dans build/rt_tuto
```

### Méthode 3 : Compilation directe (sans CMake)

Pour une compilation rapide sans CMake :

```bash
aarch64-linux-gnu-g++ \
    -O3 \
    -march=armv8-a+crc \
    -mtune=cortex-a72 \
    -std=c++17 \
    -Wall -Wextra \
    -pthread \
    src/rt_tuto.cpp \
    -o rt_tuto
```

## Vérification du binaire

### Vérifier l'architecture

```bash
# Vérifier le type de fichier
file build/rt_tuto
```

Sortie attendue :
```
build/rt_tuto: ELF 64-bit LSB executable, ARM aarch64, version 1 (SYSV), 
dynamically linked, interpreter /lib/ld-linux-aarch64.so.1, 
BuildID[sha1]=..., for GNU/Linux 3.7.0, not stripped
```

Points importants :
- `ELF 64-bit` : Format exécutable Linux
- `ARM aarch64` : Architecture ARM 64-bit (ce qu'on veut)
- `dynamically linked` : Utilise les bibliothèques du système cible

### Vérifier les headers ELF

```bash
aarch64-linux-gnu-readelf -h build/rt_tuto
```

Vérifier que `Machine` affiche `AArch64`.

### Vérifier les dépendances

```bash
aarch64-linux-gnu-readelf -d build/rt_tuto | grep NEEDED
```

Sortie typique :
```
 0x0000000000000001 (NEEDED)  Shared library: [libstdc++.so.6]
 0x0000000000000001 (NEEDED)  Shared library: [libgcc_s.so.1]
 0x0000000000000001 (NEEDED)  Shared library: [libpthread.so.0]
 0x0000000000000001 (NEEDED)  Shared library: [libc.so.6]
```

Ces bibliothèques sont présentes par défaut sur Ubuntu pour Raspberry Pi.

## Déploiement sur le Raspberry Pi

### Configuration SSH

Pour faciliter le déploiement, configurez l'authentification par clé SSH :

```bash
# Générer une clé SSH (si pas déjà fait)
ssh-keygen -t ed25519 -C "dev@wsl2"

# Copier la clé sur le Pi
ssh-copy-id ubuntu@raspberrypi.local
# ou
ssh-copy-id ubuntu@192.168.1.100

# Tester la connexion
ssh ubuntu@raspberrypi.local "echo Connexion OK"
```

### Alias SSH (optionnel)

Ajoutez dans `~/.ssh/config` :

```
Host rpi4
    HostName raspberrypi.local
    User ubuntu
    IdentityFile ~/.ssh/id_ed25519
```

Puis utilisez simplement `ssh rpi4`.

### Transfert du binaire

```bash
# Copier le binaire
scp build/rt_tuto ubuntu@raspberrypi.local:~/

# Ou via l'alias
scp build/rt_tuto rpi4:~/
```

### Exécution sur le Pi

```bash
# Se connecter
ssh rpi4

# Rendre exécutable (si nécessaire)
chmod +x rt_tuto

# Exécuter le tutoriel (nécessite sudo)
sudo ./rt_tuto

# Afficher l'aide
./rt_tuto --help
```

## Configuration VS Code

### Extension Remote - WSL

1. Installez l'extension "Remote - WSL" dans VS Code
2. Ouvrez le projet dans WSL : `code .` depuis le terminal WSL
3. VS Code détectera automatiquement l'environnement Linux

### Configuration IntelliSense

Créez `.vscode/c_cpp_properties.json` :

```json
{
    "configurations": [
        {
            "name": "Linux ARM64 (Cross-compile)",
            "includePath": [
                "${workspaceFolder}/**",
                "/usr/aarch64-linux-gnu/include/**"
            ],
            "defines": [
                "__ARM_ARCH_8A__",
                "__aarch64__",
                "CROSS_COMPILED",
                "TARGET_RPI4"
            ],
            "compilerPath": "/usr/bin/aarch64-linux-gnu-g++",
            "cStandard": "c11",
            "cppStandard": "c++17",
            "intelliSenseMode": "linux-gcc-arm64"
        }
    ],
    "version": 4
}
```

### Tâches de build

Créez `.vscode/tasks.json` :

```json
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Cross-Compile for RPi4",
            "type": "shell",
            "command": "./scripts/deploy.sh",
            "group": {
                "kind": "build",
                "isDefault": true
            },
            "problemMatcher": ["$gcc"],
            "presentation": {
                "echo": true,
                "reveal": "always",
                "panel": "shared"
            }
        },
        {
            "label": "Deploy to RPi4",
            "type": "shell",
            "command": "./scripts/deploy.sh --deploy",
            "group": "build",
            "problemMatcher": []
        },
        {
            "label": "Clean Build",
            "type": "shell",
            "command": "rm -rf build bin && ./scripts/deploy.sh",
            "group": "build",
            "problemMatcher": ["$gcc"]
        }
    ]
}
```

Utilisez `Ctrl+Shift+B` pour compiler.

## Performances et optimisations

### Emplacement des fichiers

Les performances de compilation sont **bien meilleures** si vos fichiers sont dans le système de fichiers WSL2 natif :

| Emplacement | Performance |
|-------------|-------------|
| `~/projects/` | Rapide (natif ext4) |
| `/mnt/c/Users/...` | Lent (conversion NTFS) |

```bash
# Déplacer un projet existant
mv /mnt/c/Users/vous/projet ~/projects/
```

### Options de compilation optimisées

Le fichier `CMakeLists.txt` inclut déjà les optimisations pour le Raspberry Pi 4 :

- `-O3` : Optimisation maximale
- `-march=armv8-a+crc` : Instructions ARMv8-A avec CRC32
- `-mtune=cortex-a72` : Scheduling optimisé pour Cortex-A72

## Dépannage

### Erreur "aarch64-linux-gnu-gcc: command not found"

```bash
# Vérifier que la toolchain est installée
dpkg -l | grep aarch64

# Réinstaller si nécessaire
sudo apt install --reinstall gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

### Erreur "cannot execute binary file: Exec format error"

Le binaire est pour ARM64 mais vous essayez de l'exécuter sur x86_64.
C'est normal : le binaire doit être exécuté sur le Raspberry Pi.

```bash
# Vérifier l'architecture
file build/rt_tuto
# Doit afficher "ARM aarch64", pas "x86-64"
```

### Erreur de bibliothèque manquante sur le Pi

```bash
# Sur le Raspberry Pi, vérifier les dépendances
ldd ./rt_tuto

# Installer les bibliothèques manquantes
sudo apt install libstdc++6
```

### CMake ne trouve pas le compilateur

```bash
# Vérifier le chemin du compilateur
which aarch64-linux-gnu-g++

# Doit afficher: /usr/bin/aarch64-linux-gnu-g++
```

Si le chemin est différent, mettez à jour `toolchain-rpi4-aarch64.cmake`.

### Performances WSL2 lentes

1. Assurez-vous que vos fichiers sont dans `~`, pas dans `/mnt/c/`
2. Redémarrez WSL2 : `wsl --shutdown` puis relancez
3. Vérifiez la version WSL : `wsl --version`

## Ressources

- [CMake Cross-Compilation Documentation](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- [ARM GCC Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)
- [WSL2 Documentation](https://docs.microsoft.com/en-us/windows/wsl/)
- [VS Code Remote Development](https://code.visualstudio.com/docs/remote/wsl)

