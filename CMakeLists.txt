# ==============================================================================
# CMakeLists.txt - Configuration de build pour rt_tuto
# ==============================================================================
#
# Ce fichier configure la compilation du tutoriel temps réel
# pour Raspberry Pi 4 (ARM64) via cross-compilation depuis WSL2.
#
# UTILISATION :
#
#   Cross-compilation pour Raspberry Pi 4 :
#     cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-rpi4-aarch64.cmake
#     cmake --build build -j$(nproc)
#
#   Compilation native (pour tests sur x86_64) :
#     cmake -B build
#     cmake --build build
#
# ==============================================================================

cmake_minimum_required(VERSION 3.16)

# Nom et version du projet
project(RPiRealtimeTuto
    VERSION 1.0.0
    DESCRIPTION "Tutoriel pratique des APIs temps réel Linux sur Raspberry Pi"
    LANGUAGES CXX
)

# ==============================================================================
# Configuration du standard C++
# ==============================================================================

# C++17 pour les fonctionnalités modernes (std::optional, structured bindings, etc.)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Désactiver les extensions GNU pour une meilleure portabilité
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# Options de compilation
# ==============================================================================

# Activer tous les avertissements utiles
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Avertissements supplémentaires pour une meilleure qualité de code
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wconversion -Wsign-conversion")

# ==============================================================================
# Configuration spécifique à la cross-compilation
# ==============================================================================

if(CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    message(STATUS "")
    message(STATUS "=== Cross-compilation pour Raspberry Pi 4 (ARM64) ===")
    message(STATUS "")
    
    # Optimisations spécifiques au Cortex-A72 du Raspberry Pi 4
    #
    # -O3 : Niveau d'optimisation maximal
    # -march=armv8-a+crc : Architecture ARMv8-A avec extension CRC32
    # -mtune=cortex-a72 : Optimisations spécifiques au Cortex-A72
    # -ffast-math : Optimisations mathématiques agressives (OK pour nos besoins)
    #
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=armv8-a+crc -mtune=cortex-a72")
    
    # Version Debug avec symboles mais toujours optimisée pour le Pi
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O2 -march=armv8-a+crc")
    
    # Définir une macro pour identifier la compilation croisée dans le code
    add_compile_definitions(CROSS_COMPILED)
    add_compile_definitions(TARGET_RPI4)
    
else()
    message(STATUS "")
    message(STATUS "=== Compilation native (${CMAKE_SYSTEM_PROCESSOR}) ===")
    message(STATUS "")
    
    # Optimisations pour le système hôte
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
endif()

# ==============================================================================
# Définition de l'exécutable
# ==============================================================================

add_executable(rt_tuto
    src/rt_tuto.cpp
)

# Répertoires d'inclusion
target_include_directories(rt_tuto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ==============================================================================
# Bibliothèques système
# ==============================================================================

# pthread est requis pour :
# - std::thread
# - pthread_setaffinity_np()
# - Autres fonctions POSIX threads
find_package(Threads REQUIRED)
target_link_libraries(rt_tuto PRIVATE Threads::Threads)

# ==============================================================================
# Installation
# ==============================================================================

# Installation de l'exécutable dans le répertoire bin/
install(TARGETS rt_tuto
    RUNTIME DESTINATION bin
)

# ==============================================================================
# Informations de build
# ==============================================================================

message(STATUS "")
message(STATUS "╔══════════════════════════════════════════════════════════════╗")
message(STATUS "║             Configuration de build rt_tuto                   ║")
message(STATUS "╚══════════════════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "  Projet:            ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "  Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Compiler:      ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ Standard:      C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Cross-compiling:   ${CMAKE_CROSSCOMPILING}")
message(STATUS "  Target system:     ${CMAKE_SYSTEM_NAME}")
message(STATUS "  Target processor:  ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "  CXX Flags:         ${CMAKE_CXX_FLAGS}")
message(STATUS "  CXX Flags Release: ${CMAKE_CXX_FLAGS_RELEASE}")
message(STATUS "  CXX Flags Debug:   ${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "")
message(STATUS "╔══════════════════════════════════════════════════════════════╗")
message(STATUS "║                    Commandes utiles                          ║")
message(STATUS "╚══════════════════════════════════════════════════════════════╝")
message(STATUS "")
message(STATUS "  Compiler:          cmake --build build -j\$(nproc)")
message(STATUS "  Vérifier binaire:  file build/rt_tuto")
message(STATUS "  Déployer:          scp build/rt_tuto ubuntu@raspberrypi:~/")
message(STATUS "")

