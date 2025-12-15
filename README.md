# Tutoriel Temps Réel - Raspberry Pi 4

Guide complet d'installation d'un kernel temps réel et **tutoriel pratique** en C++ pour la programmation temps réel sous Linux (Raspberry Pi 4 - Ubuntu 24.04 LTS).

## Introduction

Ce projet fournit :

1. Un **script d'installation** automatisé pour configurer un kernel temps réel (RT_PREEMPT) sur Raspberry Pi 4
2. Un **tutoriel C++ pratique** (`rt_tuto`) démontrant l'utilisation des APIs temps réel Linux
3. Une **configuration de cross-compilation** depuis WSL2 pour un développement plus efficace
4. Des **instructions pour cyclictest** (outil de référence pour les mesures de performance)

### Objectif

Le code source `rt_tuto.cpp` est un **tutoriel pratique** commenté :

- Configuration de threads temps réel avec `SCHED_FIFO`
- Verrouillage mémoire avec `mlockall()`
- Épinglage sur un CPU isolé avec `pthread_setaffinity_np()`
- Implémentation d'une boucle périodique avec `clock_nanosleep()`
- Mesure de latences avec `CLOCK_MONOTONIC`

Le programme est volontairement simple pour servir de base aux projets temps réel. Pour des tests de stress approfondis, utilisez `cyclictest`.

## Prérequis

### Matériel

- Raspberry Pi 4 (2 GB RAM minimum, 4 GB recommandé)
- Carte microSD (16 GB minimum)
- Alimentation USB-C 5V/3A
- Connexion réseau (Ethernet recommandé pour les tests)

### Logiciel sur le Raspberry Pi

- Ubuntu 24.04 LTS Server pour Raspberry Pi (64-bit)
- Accès SSH configuré

### Poste de développement (pour cross-compilation)

- Windows 11 avec WSL2 (Ubuntu 22.04 LTS par exemple)
- Toolchain ARM64 : `gcc-aarch64-linux-gnu`
- CMake 3.16+

Voir le guide [CROSS_COMPILE.md](CROSS_COMPILE.md) pour les détails d'installation.

## Installation du Kernel Temps Réel

### Ce que fait le script

Le script `setup_realtime_rpi.sh` configure automatiquement :

1. **Installation du kernel RT** : Package `linux-raspi-realtime`
2. **Paramètres de boot** :
   - `isolcpus=2,3` : Isole les CPUs 2 et 3 du scheduler général
   - `rcu_nocbs=2,3` : Désactive les callbacks RCU sur ces CPUs
   - `nohz_full=2,3` : Désactive les ticks timer sur ces CPUs
   - `preempt=full` : Active la préemption complète
3. **Limites utilisateur** : `rtprio 99`, `memlock unlimited`
4. **Optimisations système** : Governor CPU en "performance", swap désactivé, optimisations réseau

### Installation

```bash
# Copier le script sur le Raspberry Pi
scp scripts/setup_realtime_rpi.sh ubuntu@raspberrypi:~/

# Se connecter au Raspberry Pi
ssh ubuntu@raspberrypi

# Rendre le script exécutable et l'exécuter
chmod +x setup_realtime_rpi.sh
sudo ./setup_realtime_rpi.sh

# Redémarrer pour activer le kernel RT
sudo reboot
```

## Validation post-installation

Après le redémarrage, vérifiez la configuration :

```bash
# Vérifier que le kernel RT est actif
uname -r
# Doit contenir "rt" ou "realtime", exemple: 6.8.0-2019-raspi-realtime

# Vérifier les CPUs isolés
cat /sys/devices/system/cpu/isolated
# Doit afficher: 2-3

# Vérifier les limites temps réel
ulimit -r
# Doit retourner: 99

# Vérifier le governor CPU
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
# Doit afficher: performance

# Lancer le script de validation complet
sudo /usr/local/bin/validate_realtime.sh
```

## Tutoriel pratique C++

### Objectif du tutoriel

Le programme `rt_tuto` est un **tutoriel pratique** qui démontre comment configurer et exécuter une tâche temps réel. Il illustre les trois piliers de la programmation temps réel sous Linux :

```
┌──────────────────────────────────────────────────────────────┐
│                 CONFIGURATION TEMPS RÉEL                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1️⃣  VERROUILLAGE MÉMOIRE (mlockall)                        │
│      ├─ MCL_CURRENT : pages actuelles                        │
│      └─ MCL_FUTURE : futures allocations                     │
│                                                              │
│  2️⃣  ORDONNANCEMENT SCHED_FIFO                              │
│      ├─ Priorité 80 (1-99)                                   │
│      └─ Préemption par priorité uniquement                   │
│                                                              │
│  3️⃣  AFFINAGE CPU (pthread_setaffinity_np)                  │
│      └─ CPU isolé 2 (isolcpus=2,3)                           │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│                 BOUCLE PÉRIODIQUE                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  • CLOCK_MONOTONIC : horloge stable                          │
│  • clock_nanosleep() : sommeil précis (TIMER_ABSTIME)        │
│  • Mesure de latence à chaque réveil                         │
│  • Période : 1 ms, 1000 itérations (~1 seconde)              │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Paramètres

| Paramètre | Valeur | Description |
|-----------|--------|-------------|
| Période | 1000 µs (1 ms) | Intervalle entre chaque réveil |
| Itérations | 1000 | Cycles de test (durée : ~1 seconde) |
| Priorité RT | 80 | Priorité SCHED_FIFO (1-99) |
| CPU isolé | 2 | CPU réservé aux tâches RT |

## Cross-Compilation depuis WSL2

### Installation rapide de la toolchain

```bash
# Dans WSL2 Ubuntu
sudo apt update
sudo apt install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu cmake build-essential
```

### Compilation

```bash
# Utiliser le script de déploiement
./scripts/deploy.sh

# Ou manuellement avec CMake
cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-rpi4-aarch64.cmake
cmake --build build -j$(nproc)

# Vérifier le binaire
file build/rt_tuto
# Doit afficher: ELF 64-bit LSB executable, ARM aarch64
```

Voir [CROSS_COMPILE.md](CROSS_COMPILE.md) pour le guide complet.

## Utilisation de cyclictest pour tester l’installation et les performances du kernel RT

### Installation

```bash
sudo apt install rt-tests
```

### Tests recommandés

**Test rapide sur CPU isolé (10 secondes) :**

```bash
sudo cyclictest -t1 -p 80 -a 2 -m -i 1000 -l 10000 -q
```

**Test approfondi (1 heure) :**

```bash
sudo cyclictest -t1 -p 80 -a 2 -m -i 1000 -l 3600000
```

**Test comparatif CPU isolé vs non-isolé :**

```bash
# CPU 0 (non isolé) - attendu: latences élevées
sudo cyclictest -t1 -p 80 -a 0 -m -i 1000 -l 10000 -q

# CPU 2 (isolé) - attendu: latences faibles
sudo cyclictest -t1 -p 80 -a 2 -m -i 1000 -l 10000 -q
```

### Options importantes

| Option | Description |
|--------|-------------|
| `-t1` | 1 thread de test |
| `-p 80` | Priorité SCHED_FIFO 80 |
| `-a 2` | Épingler sur CPU 2 (isolé) |
| `-m` | Verrouiller la mémoire (mlockall) |
| `-i 1000` | Intervalle 1000 µs (1 ms) |
| `-l 10000` | 10000 itérations |
| `-q` | Mode silencieux (affiche seulement le max) |

### Interprétation des résultats

Exemple de sortie :

```
# T: 0 ( 1234) P:80 I:1000 C:  10000 Min:   8 Act:  12 Avg:  14 Max:    47
```

La valeur **Max** (latence maximale) est la plus importante :

- **< 50 µs** : Configuration temps réel excellente
- **50-100 µs** : Configuration temps réel acceptable
- **> 100 µs** : Vérifier la configuration

### Résultats de référence

Tests réalisés sur **Raspberry Pi 4 (4 GB)** avec **Ubuntu 24.04.1 LTS** :

**Configuration :**
```bash
sudo cyclictest -t1 -p 80 -a 2 -m -i 1000 -l 10000 -q
```

**Résultats obtenus :**

| Métrique | Valeur | Verdict |
|----------|--------|---------|
| Latence min | 18 µs | ✅ |
| Latence moy | 23 µs | ✅ |
| **Latence max** | **29 µs** | ✅ **Excellent** |

**Points clés :**
- ✅ Latence maximale de **29 µs** : bien en dessous du seuil de 50 µs
- ✅ Distribution très serrée : **84% des mesures entre 23-24 µs**
- ✅ Déterminisme garanti : aucune latence > 36 µs sur 10 000 cycles

Ces résultats confirment que le Raspberry Pi 4 avec kernel temps réel est parfaitement adapté aux applications industrielles exigeantes (EtherCAT, contrôle moteur, robotique).

## Exécution du programme tuto de démonstration C/C++

### Déploiement sur le Raspberry Pi

```bash
# Depuis WSL2
scp bin/rt_tuto ubuntu@raspberrypi:~/

# Ou via le script de déploiement
./scripts/deploy.sh --deploy
```

### Exécution

```bash
# Sur le Raspberry Pi

# Démonstration complète des APIs temps réel
# (nécessite sudo pour SCHED_FIFO et mlockall)
sudo ./rt_tuto

# Afficher l'aide
./rt_tuto --help
```

### Exemple de sortie

```
╔══════════════════════════════════════════════════════════════╗
║    DÉMONSTRATION APIs TEMPS RÉEL LINUX - RASPBERRY PI 4     ║
╚══════════════════════════════════════════════════════════════╝

Informations système :
  • CPUs disponibles : 4
  • Période de test  : 1000 µs
  • Itérations       : 1000

╔══════════════════════════════════════════════════════════════╗
║           CONFIGURATION TEMPS RÉEL                           ║
╚══════════════════════════════════════════════════════════════╝

1️⃣  Verrouillage mémoire avec mlockall()
   ────────────────────────────────────────
   Objectif : Éviter les page faults qui causent des latences
              imprévisibles (plusieurs millisecondes).

   ✓ Mémoire verrouillée (MCL_CURRENT | MCL_FUTURE)

2️⃣  Configuration ordonnancement SCHED_FIFO
   ─────────────────────────────────────────
   Objectif : Garantir l'exécution prioritaire du thread.

   Configuration :
   • Politique : SCHED_FIFO (temps réel)
   • Priorité  : 80 (1-99, 99 = max)

   ✓ SCHED_FIFO activé avec priorité 80

3️⃣  Affinage sur CPU isolé
   ────────────────────────
   Objectif : Exécuter sur un CPU dédié sans interférences.

   Configuration :
   • CPU cible : 2 (isolé par isolcpus=2,3)
   • Méthode   : pthread_setaffinity_np()

   ✓ Thread affiné sur CPU 2 (isolé)

╔══════════════════════════════════════════════════════════════╗
║              EXÉCUTION TÂCHE PÉRIODIQUE                      ║
╚══════════════════════════════════════════════════════════════╝

Paramètres :
  • Période     : 1000 µs (1 ms)
  • Itérations  : 1000
  • Durée totale: ~1 seconde(s)

Démarrage de la boucle périodique...
(Affichage tous les 100 cycles)

  Cycle  100/1000 - Latence courante:    12 µs
  Cycle  200/1000 - Latence courante:    15 µs
  ...

✓ Tâche périodique terminée

╔══════════════════════════════════════════════════════════════╗
║                 RÉSULTATS DE LATENCE                         ║
╚══════════════════════════════════════════════════════════════╝

Statistiques :
  • Latence minimale  :     8.00 µs
  • Latence maximale  :    47.00 µs  ← Excellent !
  • Latence moyenne   :    14.50 µs
  • Écart-type        :     6.20 µs

Histogramme des latences:
     8-   12 µs: ████████████████████████████████ 320
    12-   16 µs: ████████████████████ 200
    ...

💡 Recommandations :

  ✓ Configuration temps réel excellente !
    Votre système est prêt pour des applications temps réel
    avec des contraintes de latence strictes.

🔧 Pour des tests de stress approfondis :
   sudo cyclictest -t1 -p 80 -a 2 -m -i 1000 -l 3600000
```

### Interprétation des résultats

| Latence Max | Qualité | Action |
|-------------|---------|--------|
| < 50 µs | Excellent | Système parfaitement configuré |
| 50-100 µs | Très bon | Configuration RT fonctionnelle |
| 100-200 µs | Acceptable | Vérifier la configuration |
| > 200 µs | Problème | Voir section Dépannage |

## Dépannage

### Le kernel RT n'est pas actif après redémarrage

```bash
# Vérifier les kernels disponibles
dpkg -l | grep linux-image

# Vérifier le kernel au boot
cat /proc/cmdline

# Réinstaller si nécessaire
sudo apt install --reinstall linux-raspi-realtime
```

### Latences élevées même en mode RT

1. **Vérifier l'isolation CPU** :
   ```bash
   cat /sys/devices/system/cpu/isolated
   # Doit afficher "2-3"
   ```

2. **Vérifier que le programme utilise bien le CPU isolé** :
   ```bash
   # Pendant l'exécution du tutoriel
   ps -eo pid,comm,psr | grep rt_tuto
   # La colonne PSR doit afficher 2
   ```

3. **Vérifier les interruptions** :
   ```bash
   cat /proc/interrupts | head -20
   # Les CPUs 2-3 doivent avoir très peu d'interruptions
   ```

### Erreurs de permission

```bash
# Vérifier les limites RT
ulimit -r  # Doit être 99

# Si ce n'est pas le cas:
# 1. Vérifier /etc/security/limits.conf contient bien les limites RT
# 2. Se reconnecter complètement (exit puis ssh)
# 3. NE PAS utiliser root - les limites PAM s'appliquent aux utilisateurs normaux
```

### ulimit -r retourne 0

Si `ulimit -r` retourne 0 :

- Vérifiez que vous êtes connecté avec un **utilisateur normal** (ex: `ubuntu`), pas `root`
- Les limites définies dans `/etc/security/limits.conf` sont appliquées par PAM aux utilisateurs, pas à root
- Déconnectez-vous complètement et reconnectez-vous pour que les limites soient appliquées

### Problèmes de cross-compilation

Voir la section dépannage dans [CROSS_COMPILE.md](CROSS_COMPILE.md).

## Structure du projet

```
rpi-rt-test/
├── README.md                      # Ce fichier
├── CROSS_COMPILE.md              # Guide de cross-compilation
├── LICENSE                        # Licence MIT
├── CMakeLists.txt                # Configuration CMake
├── toolchain-rpi4-aarch64.cmake  # Toolchain cross-compilation
├── scripts/
│   ├── setup_realtime_rpi.sh     # Script d'installation kernel RT
│   └── deploy.sh                 # Script de compilation et déploiement
├── src/
│   ├── rt_tuto.cpp               # Tutoriel principal (abondamment commenté)
│   └── rt_utils.h                # Fonctions utilitaires
├── build/                        # Répertoire de compilation (généré)
└── bin/                          # Binaires cross-compilés (généré)
```

## Références

- [Linux Foundation - Real-Time Linux](https://wiki.linuxfoundation.org/realtime/start)
- [RT PREEMPT Documentation](https://rt.wiki.kernel.org/)
- [Raspberry Pi Ubuntu Documentation](https://ubuntu.com/download/raspberry-pi)
- [POSIX Real-Time Extensions](https://pubs.opengroup.org/onlinepubs/9699919799/)
- [sched_setscheduler(2) - Linux man page](https://man7.org/linux/man-pages/man2/sched_setscheduler.2.html)
- [mlockall(2) - Linux man page](https://man7.org/linux/man-pages/man2/mlockall.2.html)



