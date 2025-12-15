#!/bin/bash

# Script de configuration temps réel pour Raspberry Pi Ubuntu 24.04
# Usage: sudo ./setup_realtime_rpi.sh

set -e

if [ "$EUID" -ne 0 ]; then
    echo "❌ Ce script doit être exécuté en tant que root"
    echo "💡 Utiliser: sudo $0"
    exit 1
fi

echo "🍓 Configuration Temps Réel - Raspberry Pi Ubuntu 24.04"
echo "======================================================"

# Vérifier qu'on est sur un Raspberry Pi
if ! grep -q "Raspberry Pi" /proc/device-tree/model 2>/dev/null; then
    echo "⚠️  Ce script est conçu pour Raspberry Pi"
    read -p "Continuer quand même ? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

echo "🔍 Système détecté:"
echo "   Modèle: $(cat /proc/device-tree/model 2>/dev/null || echo 'Inconnu')"
echo "   Kernel: $(uname -r)"
echo "   CPUs: $(nproc)"

# 1. Installation du kernel temps réel
echo ""
echo "📦 1. Installation du kernel temps réel..."

if ! dpkg -l | grep -q linux-raspi-realtime; then
    echo "   Installation linux-raspi-realtime..."
    apt update
    apt install -y linux-raspi-realtime
    echo "✅ Kernel temps réel installé"
    REBOOT_NEEDED=true
else
    echo "✅ Kernel temps réel déjà installé"
fi

# 2. Configuration boot
echo ""
echo "⚙️  2. Configuration du boot..."

CMDLINE_FILE="/boot/firmware/cmdline.txt"
if [ -f "$CMDLINE_FILE" ]; then
    # Sauvegarder l'original
    cp "$CMDLINE_FILE" "$CMDLINE_FILE.backup.$(date +%Y%m%d_%H%M%S)"
    
    # Paramètres temps réel à ajouter
    RT_PARAMS="isolcpus=2,3 rcu_nocbs=2,3 nohz_full=2,3 preempt=full"
    
    # Vérifier si déjà configuré
    if ! grep -q "isolcpus" "$CMDLINE_FILE"; then
        echo "   Ajout des paramètres temps réel au boot..."
        sed -i "s/$/ $RT_PARAMS/" "$CMDLINE_FILE"
        echo "✅ Paramètres boot configurés"
        REBOOT_NEEDED=true
    else
        echo "✅ Paramètres boot déjà configurés"
    fi
else
    echo "⚠️  Fichier $CMDLINE_FILE non trouvé"
fi

# 3. Configuration système
echo ""
echo "🔧 3. Configuration système..."

# Limites utilisateur
LIMITS_FILE="/etc/security/limits.conf"
if ! grep -q "rtprio 99" "$LIMITS_FILE"; then
    echo "   Configuration des limites utilisateur..."
    cat >> "$LIMITS_FILE" << EOF

# Configuration temps réel
* soft rtprio 99
* hard rtprio 99
* soft memlock unlimited
* hard memlock unlimited
* soft nice -20
* hard nice -20
EOF
    echo "✅ Limites utilisateur configurées"
else
    echo "✅ Limites utilisateur déjà configurées"
fi

# Script de démarrage
RC_LOCAL="/etc/rc.local"
if [ ! -f "$RC_LOCAL" ] || ! grep -q "Configuration temps réel" "$RC_LOCAL"; then
    echo "   Création du script de démarrage..."
    cat > "$RC_LOCAL" << 'EOF'
#!/bin/bash
# Configuration temps réel au boot

echo "🚀 Application configuration temps réel..."

# CPU Governor performance
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    if [ -f "$cpu" ]; then
        echo performance > "$cpu"
    fi
done

# Désactiver swap
swapoff -a 2>/dev/null || true

# Configuration réseau temps réel
for iface in eth0 enp1s0; do
    if ip link show "$iface" >/dev/null 2>&1; then
        ethtool -K "$iface" gro off gso off tso off lro off 2>/dev/null || true
        ethtool -G "$iface" rx 4096 tx 4096 2>/dev/null || true
        ethtool -C "$iface" rx-usecs 0 tx-usecs 0 2>/dev/null || true
    fi
done

# Configuration mémoire
echo 1 > /proc/sys/vm/swappiness
echo 10 > /proc/sys/vm/dirty_ratio

# Configuration réseau
echo 1 > /proc/sys/net/core/busy_poll 2>/dev/null || true
echo 1 > /proc/sys/net/core/busy_read 2>/dev/null || true

echo "✅ Configuration temps réel appliquée"

exit 0
EOF
    chmod +x "$RC_LOCAL"
    echo "✅ Script de démarrage créé"
else
    echo "✅ Script de démarrage déjà configuré"
fi

# 4. Configuration services
echo ""
echo "🛠️  4. Configuration des services..."

# Désactiver services non critiques
SERVICES_TO_DISABLE=(
    "snapd"
    "bluetooth"
    "cups"
    "avahi-daemon"
    "ModemManager"
)

for service in "${SERVICES_TO_DISABLE[@]}"; do
    if systemctl is-enabled "$service" >/dev/null 2>&1; then
        systemctl disable "$service"
        echo "   Désactivé: $service"
    fi
done

# 5. Test de configuration
echo ""
echo "🧪 5. Tests de configuration..."

# Vérifier les CPUs
echo "   CPUs disponibles: $(nproc)"
echo "   CPUs isolés: $(cat /sys/devices/system/cpu/isolated 2>/dev/null || echo 'Aucun (redémarrage requis)')"

# Vérifier les limites
echo "   Limite rtprio: $(ulimit -r 2>/dev/null || echo 'Non configuré')"

# 6. Script de validation post-reboot
VALIDATION_SCRIPT="/usr/local/bin/validate_realtime.sh"
cat > "$VALIDATION_SCRIPT" << 'EOF'
#!/bin/bash
echo "🔍 Validation Configuration Temps Réel"
echo "====================================="

echo "Kernel: $(uname -r)"
if uname -r | grep -qE "(rt|realtime)"; then
    echo "✅ Kernel temps réel actif"
else
    echo "⚠️  Kernel temps réel non actif"
fi

echo "CPUs isolés: $(cat /sys/devices/system/cpu/isolated 2>/dev/null || echo 'Aucun')"
echo "Governor CPU: $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null || echo 'N/A')"
echo "Swap: $(swapon --show | wc -l) partition(s) active(s)"

echo ""
echo "Limites utilisateur:"
echo "  rtprio: $(ulimit -r)"
echo "  memlock: $(ulimit -l)"

echo ""
echo "Test latence (cyclictest):"
if command -v cyclictest >/dev/null; then
    cyclictest -t1 -p 80 -i 10000 -l 1000
else
    echo "  cyclictest non installé (sudo apt install rt-tests)"
fi
EOF
chmod +x "$VALIDATION_SCRIPT"

# Résumé
echo ""
echo "🎉 Configuration Temps Réel Terminée !"
echo "====================================="

if [ "${REBOOT_NEEDED:-false}" = "true" ]; then
    echo "⚠️  REDÉMARRAGE REQUIS pour activer le kernel temps réel"
    echo ""
    echo "Après redémarrage:"
    echo "  1. Vérifier: uname -r (doit contenir 'rt' ou 'realtime')"
    echo "  2. Valider: sudo $VALIDATION_SCRIPT"
    echo ""
    read -p "Redémarrer maintenant ? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "🔄 Redémarrage..."
        reboot
    else
        echo "💡 N'oubliez pas de redémarrer pour activer les changements"
    fi
else
    echo "✅ Configuration appliquée - Pas de redémarrage nécessaire"
    echo "🧪 Lancer la validation: sudo $VALIDATION_SCRIPT"
fi

echo ""
echo "📖 Pour plus d'informations, consultez le README.md" 