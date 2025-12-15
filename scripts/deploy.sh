#!/bin/bash
# ==============================================================================
# deploy.sh - Script de cross-compilation et déploiement pour Raspberry Pi 4
# ==============================================================================
#
# Ce script automatise :
# 1. La cross-compilation du tutoriel pour ARM64
# 2. La vérification du binaire généré
# 3. Le déploiement optionnel sur le Raspberry Pi
#
# UTILISATION :
#   ./scripts/deploy.sh              # Compile uniquement
#   ./scripts/deploy.sh --deploy     # Compile et déploie sur le Pi
#   ./scripts/deploy.sh --clean      # Nettoie et recompile
#
# CONFIGURATION :
#   RPI_HOST : Adresse du Raspberry Pi (défaut: ubuntu@raspberrypi.local)
#
# EXEMPLES :
#   ./scripts/deploy.sh
#   RPI_HOST=ubuntu@192.168.1.100 ./scripts/deploy.sh --deploy
#
# ==============================================================================

set -e  # Arrêter en cas d'erreur

# ==============================================================================
# Configuration
# ==============================================================================

# Adresse du Raspberry Pi (peut être surchargée par variable d'environnement)
RPI_HOST="${RPI_HOST:-ubuntu@raspberrypi.local}"

# Répertoires
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
BIN_DIR="$PROJECT_ROOT/bin"
TOOLCHAIN_FILE="$PROJECT_ROOT/toolchain-rpi4-aarch64.cmake"

# Nom du binaire
BINARY_NAME="rt_tuto"

# Couleurs pour l'affichage
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ==============================================================================
# Fonctions utilitaires
# ==============================================================================

print_header() {
    echo -e "${BLUE}"
    echo "╔══════════════════════════════════════════════════════════════╗"
    echo "║     Cross-Compilation Raspberry Pi 4 - rt_tuto               ║"
    echo "╚══════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_step() {
    echo -e "${YELLOW}▶ $1${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# ==============================================================================
# Vérification des prérequis
# ==============================================================================

check_prerequisites() {
    print_step "Vérification des prérequis..."
    
    # Vérifier que le compilateur cross est installé
    if ! command -v aarch64-linux-gnu-g++ &> /dev/null; then
        print_error "aarch64-linux-gnu-g++ non trouvé !"
        echo "Installation : sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
        exit 1
    fi
    
    # Vérifier que CMake est installé
    if ! command -v cmake &> /dev/null; then
        print_error "CMake non trouvé !"
        echo "Installation : sudo apt install cmake"
        exit 1
    fi
    
    # Vérifier que le fichier toolchain existe
    if [ ! -f "$TOOLCHAIN_FILE" ]; then
        print_error "Fichier toolchain non trouvé : $TOOLCHAIN_FILE"
        exit 1
    fi
    
    print_success "Tous les prérequis sont satisfaits"
}

# ==============================================================================
# Compilation
# ==============================================================================

compile() {
    print_step "Compilation pour Raspberry Pi 4 (ARM64)..."
    
    # Créer les répertoires si nécessaires
    mkdir -p "$BUILD_DIR"
    mkdir -p "$BIN_DIR"
    
    # Configuration CMake
    print_info "Configuration CMake avec toolchain cross-compilation..."
    cd "$BUILD_DIR"
    cmake "$PROJECT_ROOT" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE" \
        -DCMAKE_BUILD_TYPE=Release
    
    # Compilation
    print_info "Compilation en cours..."
    cmake --build . -j$(nproc)
    
    # Copier le binaire dans bin/
    if [ -f "$BUILD_DIR/$BINARY_NAME" ]; then
        cp "$BUILD_DIR/$BINARY_NAME" "$BIN_DIR/"
        print_success "Binaire généré : $BIN_DIR/$BINARY_NAME"
    else
        print_error "Binaire non trouvé après compilation !"
        exit 1
    fi
}

# ==============================================================================
# Vérification du binaire
# ==============================================================================

verify_binary() {
    print_step "Vérification du binaire..."
    
    local binary="$BIN_DIR/$BINARY_NAME"
    
    if [ ! -f "$binary" ]; then
        print_error "Binaire non trouvé : $binary"
        exit 1
    fi
    
    # Vérifier le type de fichier
    local file_type=$(file "$binary")
    echo "  Type: $file_type"
    
    # Vérifier que c'est bien un binaire ARM64
    if echo "$file_type" | grep -q "ARM aarch64"; then
        print_success "Architecture correcte : ARM aarch64"
    else
        print_error "Architecture incorrecte ! Attendu: ARM aarch64"
        exit 1
    fi
    
    # Afficher les informations ELF
    print_info "Informations ELF :"
    aarch64-linux-gnu-readelf -h "$binary" | grep -E "(Class|Machine|Entry)"
    
    # Afficher les dépendances
    print_info "Bibliothèques requises :"
    aarch64-linux-gnu-readelf -d "$binary" | grep NEEDED | sed 's/.*\[\(.*\)\]/  - \1/'
    
    # Afficher la taille
    local size=$(ls -lh "$binary" | awk '{print $5}')
    print_info "Taille du binaire : $size"
}

# ==============================================================================
# Déploiement
# ==============================================================================

deploy() {
    print_step "Déploiement sur $RPI_HOST..."
    
    local binary="$BIN_DIR/$BINARY_NAME"
    
    # Vérifier la connectivité SSH
    print_info "Test de connexion SSH..."
    if ! ssh -o ConnectTimeout=5 -o BatchMode=yes "$RPI_HOST" "echo OK" &> /dev/null; then
        print_error "Impossible de se connecter à $RPI_HOST"
        echo ""
        echo "Solutions possibles :"
        echo "  1. Vérifier que le Pi est allumé et sur le réseau"
        echo "  2. Vérifier l'adresse : RPI_HOST=ubuntu@192.168.x.x ./scripts/deploy.sh --deploy"
        echo "  3. Configurer l'authentification SSH par clé"
        exit 1
    fi
    
    # Copier le binaire
    print_info "Copie du binaire..."
    scp "$binary" "$RPI_HOST:~/"
    
    # Rendre exécutable
    ssh "$RPI_HOST" "chmod +x ~/$BINARY_NAME"
    
    print_success "Déploiement terminé !"
    echo ""
    print_info "Pour exécuter sur le Raspberry Pi :"
    echo "  ssh $RPI_HOST"
    echo "  sudo ./$BINARY_NAME              # Démonstration complète"
    echo "  ./$BINARY_NAME --help            # Afficher l'aide"
}

# ==============================================================================
# Nettoyage
# ==============================================================================

clean() {
    print_step "Nettoyage des fichiers de build..."
    
    rm -rf "$BUILD_DIR"
    rm -rf "$BIN_DIR"
    
    print_success "Nettoyage terminé"
}

# ==============================================================================
# Affichage de l'aide
# ==============================================================================

show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Script de cross-compilation et déploiement pour Raspberry Pi 4."
    echo ""
    echo "OPTIONS:"
    echo "  --deploy    Compile et déploie le binaire sur le Raspberry Pi"
    echo "  --clean     Nettoie les fichiers de build avant de compiler"
    echo "  --help, -h  Affiche cette aide"
    echo ""
    echo "VARIABLES D'ENVIRONNEMENT:"
    echo "  RPI_HOST    Adresse du Raspberry Pi (défaut: ubuntu@raspberrypi.local)"
    echo ""
    echo "EXEMPLES:"
    echo "  $0                                           # Compile uniquement"
    echo "  $0 --deploy                                  # Compile et déploie"
    echo "  $0 --clean                                   # Nettoie et compile"
    echo "  RPI_HOST=ubuntu@192.168.1.100 $0 --deploy   # Déploie sur IP spécifique"
}

# ==============================================================================
# Point d'entrée
# ==============================================================================

main() {
    local do_deploy=false
    local do_clean=false
    
    # Parser les arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --deploy)
                do_deploy=true
                shift
                ;;
            --clean)
                do_clean=true
                shift
                ;;
            --help|-h)
                show_help
                exit 0
                ;;
            *)
                print_error "Option inconnue : $1"
                show_help
                exit 1
                ;;
        esac
    done
    
    # Exécution
    print_header
    
    echo "Répertoire projet : $PROJECT_ROOT"
    echo "Cible déploiement : $RPI_HOST"
    echo ""
    
    check_prerequisites
    
    if $do_clean; then
        clean
    fi
    
    compile
    verify_binary
    
    if $do_deploy; then
        deploy
    fi
    
    echo ""
    print_success "Terminé avec succès !"
    
    if ! $do_deploy; then
        echo ""
        print_info "Pour déployer : $0 --deploy"
    fi
}

# Exécuter le script
main "$@"

