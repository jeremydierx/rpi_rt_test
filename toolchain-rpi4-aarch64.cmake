# ==============================================================================
# Toolchain CMake pour cross-compilation Raspberry Pi 4 (ARM64)
# ==============================================================================
#
# Ce fichier configure CMake pour générer des binaires ARM64 depuis un système
# x86_64 (typiquement WSL2 Ubuntu).
#
# UTILISATION :
#   cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-rpi4-aarch64.cmake
#   cmake --build build
#
# PRÉREQUIS :
#   sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#
# ==============================================================================

# ==============================================================================
# Configuration du système cible
# ==============================================================================

# Système d'exploitation cible
set(CMAKE_SYSTEM_NAME Linux)

# Architecture du processeur cible
# aarch64 = ARM 64-bit (ARMv8-A)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# ==============================================================================
# Compilateurs cross-compilation
# ==============================================================================

# Compilateur C pour ARM64
# Le préfixe "aarch64-linux-gnu-" indique :
#   - aarch64 : architecture ARM 64-bit
#   - linux : système d'exploitation cible
#   - gnu : ABI GNU/glibc
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)

# Compilateur C++ pour ARM64
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# ==============================================================================
# Outils binaires
# ==============================================================================

# Archiveur pour créer des bibliothèques statiques
set(CMAKE_AR aarch64-linux-gnu-ar)

# Génération d'index pour les bibliothèques
set(CMAKE_RANLIB aarch64-linux-gnu-ranlib)

# Suppression des symboles de débogage
set(CMAKE_STRIP aarch64-linux-gnu-strip)

# Linker (optionnel, utilise gcc par défaut)
# set(CMAKE_LINKER aarch64-linux-gnu-ld)

# ==============================================================================
# Configuration des chemins de recherche
# ==============================================================================

# Ces paramètres contrôlent comment CMake recherche les dépendances
# lors de la cross-compilation.

# NEVER : Ne jamais chercher les programmes sur le système cible
# (les outils comme cmake, pkg-config, etc. sont sur l'hôte)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# ONLY : Chercher les bibliothèques UNIQUEMENT dans le sysroot cible
# (évite de linker avec les libs x86_64 de l'hôte)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)

# ONLY : Chercher les headers UNIQUEMENT dans le sysroot cible
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# ONLY : Chercher les packages CMake UNIQUEMENT dans le sysroot cible
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# ==============================================================================
# Flags de compilation par défaut
# ==============================================================================

# Position Independent Code - nécessaire pour les bibliothèques partagées
# et recommandé pour la sécurité (ASLR)
set(CMAKE_C_FLAGS_INIT "-fPIC")
set(CMAKE_CXX_FLAGS_INIT "-fPIC")

# ==============================================================================
# Configuration optionnelle du sysroot
# ==============================================================================

# Si vous avez synchronisé le sysroot du Raspberry Pi, décommentez et adaptez :
# set(CMAKE_SYSROOT /chemin/vers/rpi-sysroot)
# set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

# ==============================================================================
# Variables de cache pour éviter les re-détections
# ==============================================================================

# Désactiver les tests de compilation qui échoueraient en cross-compilation
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

