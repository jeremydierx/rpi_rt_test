/**
 * @file rt_tuto.cpp
 * @brief Tutoriel pratique d'utilisation des APIs temps réel Linux
 * 
 * @author Jeremy Dierx | Code Alchimie
 * @date 2025
 * 
 * ============================================================================
 * OBJECTIF PÉDAGOGIQUE
 * ============================================================================
 * 
 * Ce programme est un EXEMPLE ÉDUCATIF qui démontre comment utiliser les APIs
 * temps réel POSIX sous Linux. Il n'est PAS conçu comme un outil de stress test
 * (utilisez cyclictest pour cela), mais comme une référence pédagogique claire
 * et bien commentée.
 * 
 * CONCEPTS TEMPS RÉEL DÉMONTRÉS :
 * 
 * 1. ORDONNANCEMENT TEMPS RÉEL (SCHED_FIFO)
 *    - Configuration de la politique d'ordonnancement
 *    - Priorités temps réel (1-99)
 *    - Différence avec SCHED_OTHER
 * 
 * 2. VERROUILLAGE MÉMOIRE (mlockall)
 *    - Pourquoi éviter les page faults
 *    - MCL_CURRENT vs MCL_FUTURE
 * 
 * 3. AFFINAGE CPU (CPU Pinning)
 *    - Utilisation des CPUs isolés
 *    - pthread_setaffinity_np()
 * 
 * 4. HORLOGES ET TEMPORISATION
 *    - CLOCK_MONOTONIC
 *    - clock_nanosleep() avec TIMER_ABSTIME
 * 
 * POUR LES STRESS TESTS RÉELS :
 * Utilisez cyclictest : sudo cyclictest -t1 -p 80 -a 2 -m -i 1000 -l 10000
 * 
 * ============================================================================
 * UTILISATION
 * ============================================================================
 * 
 *   sudo ./rt_tuto            # Démonstration complète des APIs RT
 *   ./rt_tuto --help          # Afficher l'aide
 * 
 * ============================================================================
 */

// ============================================================================
// INCLUDES SYSTÈME
// ============================================================================

// Headers POSIX pour les fonctionnalités temps réel
#include <pthread.h>      // Threads POSIX et affinage CPU
#include <sched.h>        // Ordonnancement : sched_setscheduler(), SCHED_FIFO
#include <sys/mman.h>     // Verrouillage mémoire : mlockall()
#include <time.h>         // Horloges : clock_gettime(), clock_nanosleep()
#include <errno.h>        // Codes d'erreur
#include <string.h>       // strerror()
#include <unistd.h>       // getopt(), sysconf()

// Headers C++ standard
#include <iostream>       // Sortie console
#include <iomanip>        // Formatage des nombres
#include <vector>         // Stockage des latences
#include <cmath>          // sqrt() pour l'écart-type
#include <algorithm>      // min_element(), max_element()

// Header utilitaire local
#include "rt_utils.h"

// ============================================================================
// CONSTANTES DE CONFIGURATION
// ============================================================================

/**
 * PÉRIODE DE LA TÂCHE PÉRIODIQUE
 * 
 * Pour une application temps réel typique (contrôle, robotique), une période
 * de 1 ms (1000 µs) est courante. Plus la période est courte, plus les
 * contraintes de latence sont strictes.
 */
constexpr int PERIOD_US = 1000;  // 1000 µs = 1 ms

/**
 * NOMBRE D'ITÉRATIONS
 * 
 * Pour une démonstration pédagogique, 1000 itérations (1 seconde) suffisent.
 * Pour des tests de stress réels, utilisez cyclictest avec des millions
 * d'itérations.
 */
constexpr int NUM_ITERATIONS = 1000;

/**
 * PRIORITÉ TEMPS RÉEL
 * 
 * Sous Linux, les priorités SCHED_FIFO vont de 1 (plus basse) à 99 (plus haute).
 * Une priorité de 80 est suffisante pour la plupart des applications tout en
 * laissant de la marge pour d'éventuels threads critiques du système.
 */
constexpr int RT_PRIORITY = 80;

/**
 * CPU ISOLÉ POUR L'EXÉCUTION
 * 
 * Le script setup_realtime_rpi.sh configure isolcpus=2,3. Ces CPUs sont
 * réservés aux tâches temps réel et ne reçoivent pas de tâches du scheduler
 * général.
 */
constexpr int RT_CPU = 2;

// ============================================================================
// FONCTIONS DE CONFIGURATION TEMPS RÉEL
// ============================================================================

/**
 * @brief Configure le thread courant pour l'exécution temps réel
 * 
 * Cette fonction applique les trois configurations essentielles pour une tâche
 * temps réel sous Linux :
 * 1. Verrouillage mémoire (mlockall)
 * 2. Ordonnancement SCHED_FIFO
 * 3. Affinage CPU sur un cœur isolé
 * 
 * @return true si la configuration réussit, false sinon
 */
bool configure_realtime()
{
    std::cout << COLOR_CYAN << "\n╔══════════════════════════════════════════════════════════════╗\n"
              << "║           CONFIGURATION TEMPS RÉEL                           ║\n"
              << "╚══════════════════════════════════════════════════════════════╝"
              << COLOR_RESET << "\n" << std::endl;
    
    // ========================================================================
    // ÉTAPE 1 : VERROUILLAGE MÉMOIRE
    // ========================================================================
    
    std::cout << "1️⃣  Verrouillage mémoire avec mlockall()" << std::endl;
    std::cout << "   ────────────────────────────────────────" << std::endl;
    std::cout << "   Objectif : Éviter les page faults qui causent des latences" << std::endl;
    std::cout << "              imprévisibles (plusieurs millisecondes)." << std::endl;
    std::cout << std::endl;
    
    /*
     * mlockall() verrouille toutes les pages mémoire en RAM.
     * 
     * POURQUOI C'EST NÉCESSAIRE ?
     * Par défaut, Linux utilise la mémoire virtuelle. Quand un programme accède
     * à une page non chargée en RAM, un "page fault" se produit : le kernel doit
     * charger la page depuis le disque ou la swap. Ce processus peut prendre
     * plusieurs MILLISECONDES - catastrophique pour le temps réel.
     * 
     * MCL_CURRENT : Verrouille les pages actuellement mappées
     * MCL_FUTURE : Verrouille aussi les futures allocations
     */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0) {
        std::cerr << COLOR_RED 
                  << "   ✗ Erreur mlockall: " << strerror(errno) << "\n\n"
                  << "   Solution : Exécuter avec sudo\n"
                  << "              sudo ./rt_tuto\n"
                  << COLOR_RESET << std::endl;
        return false;
    }
    std::cout << "   " << COLOR_GREEN << "✓ Mémoire verrouillée (MCL_CURRENT | MCL_FUTURE)" 
              << COLOR_RESET << "\n" << std::endl;
    
    // ========================================================================
    // ÉTAPE 2 : ORDONNANCEMENT SCHED_FIFO
    // ========================================================================
    
    std::cout << "2️⃣  Configuration ordonnancement SCHED_FIFO" << std::endl;
    std::cout << "   ─────────────────────────────────────────" << std::endl;
    std::cout << "   Objectif : Garantir l'exécution prioritaire du thread." << std::endl;
    std::cout << std::endl;
    
    /*
     * POLITIQUES D'ORDONNANCEMENT LINUX :
     * 
     * SCHED_OTHER (défaut) :
     *   - Ordonnancement équitable (CFS)
     *   - Pas de garanties de latence
     *   - Préemptable à tout moment
     * 
     * SCHED_FIFO (temps réel) :
     *   - First-In-First-Out
     *   - Priorités 1-99 (99 = plus haute)
     *   - Préempté UNIQUEMENT par :
     *     * Thread de priorité supérieure
     *     * Interruption matérielle
     *   - Garde le CPU jusqu'à ce qu'il :
     *     * Se bloque (sleep, I/O, mutex)
     *     * Yield explicitement
     */
    struct sched_param param;
    param.sched_priority = RT_PRIORITY;
    
    std::cout << "   Configuration :" << std::endl;
    std::cout << "   • Politique : SCHED_FIFO (temps réel)" << std::endl;
    std::cout << "   • Priorité  : " << RT_PRIORITY << " (1-99, 99 = max)" << std::endl;
    std::cout << std::endl;
    
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0) {
        std::cerr << COLOR_RED 
                  << "   ✗ Erreur sched_setscheduler: " << strerror(errno) << "\n\n"
                  << "   Solutions :\n"
                  << "   1. Exécuter avec sudo: sudo ./rt_tuto\n"
                  << "   2. Vérifier : ulimit -r (doit être 99)\n"
                  << COLOR_RESET << std::endl;
        munlockall();
        return false;
    }
    std::cout << "   " << COLOR_GREEN << "✓ SCHED_FIFO activé avec priorité " << RT_PRIORITY 
              << COLOR_RESET << "\n" << std::endl;
    
    // ========================================================================
    // ÉTAPE 3 : AFFINAGE CPU (CPU PINNING)
    // ========================================================================
    
    std::cout << "3️⃣  Affinage sur CPU isolé" << std::endl;
    std::cout << "   ────────────────────────" << std::endl;
    std::cout << "   Objectif : Exécuter sur un CPU dédié sans interférences." << std::endl;
    std::cout << std::endl;
    
    /*
     * CPU PINNING : POURQUOI ?
     * 
     * Même avec SCHED_FIFO, des interférences peuvent survenir :
     * - Interruptions matérielles
     * - Threads kernel (softirq, workqueues)
     * - Cache invalidation par d'autres processus
     * 
     * SOLUTION : Combiner l'affinage avec l'isolation CPU du kernel.
     * 
     * PARAMÈTRES KERNEL (configurés par setup_realtime_rpi.sh) :
     *   isolcpus=2,3  : CPUs exclus du scheduler général
     *   rcu_nocbs=2,3 : RCU callbacks déplacés ailleurs
     *   nohz_full=2,3 : Ticks timer désactivés (tickless)
     * 
     * L'affinage force le thread sur un CPU isolé, garantissant une
     * exécution sans perturbation.
     */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(RT_CPU, &cpuset);
    
    std::cout << "   Configuration :" << std::endl;
    std::cout << "   • CPU cible : " << RT_CPU << " (isolé par isolcpus=2,3)" << std::endl;
    std::cout << "   • Méthode   : pthread_setaffinity_np()" << std::endl;
    std::cout << std::endl;
    
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        std::cerr << COLOR_YELLOW 
                  << "   ⚠ Avertissement : Affinage CPU échoué - " << strerror(errno) << "\n"
                  << "     Le test continue mais les résultats peuvent être moins bons.\n"
                  << COLOR_RESET << std::endl;
    } else {
        std::cout << "   " << COLOR_GREEN << "✓ Thread affiné sur CPU " << RT_CPU << " (isolé)" 
                  << COLOR_RESET << std::endl;
    }
    
    return true;
}

// ============================================================================
// FONCTION DE DÉMONSTRATION DE TÂCHE PÉRIODIQUE
// ============================================================================

/**
 * @brief Exécute une tâche périodique temps réel et mesure les latences
 * 
 * Cette fonction démontre comment implémenter une boucle périodique temps réel
 * avec mesure de latence. C'est le modèle de base pour toute application RT.
 * 
 * @return Vecteur des latences mesurées en nanosecondes
 */
std::vector<uint64_t> run_periodic_task()
{
    std::cout << "\n" << COLOR_BLUE 
              << "╔══════════════════════════════════════════════════════════════╗\n"
              << "║              EXÉCUTION TÂCHE PÉRIODIQUE                      ║\n"
              << "╚══════════════════════════════════════════════════════════════╝"
              << COLOR_RESET << "\n" << std::endl;
    
    std::cout << "Paramètres :" << std::endl;
    std::cout << "  • Période     : " << PERIOD_US << " µs (1 ms)" << std::endl;
    std::cout << "  • Itérations  : " << NUM_ITERATIONS << std::endl;
    std::cout << "  • Durée totale: ~" << (PERIOD_US * NUM_ITERATIONS / 1000000) << " seconde(s)" << std::endl;
    std::cout << std::endl;
    
    // Pré-allocation du vecteur pour éviter les allocations pendant la boucle
    std::vector<uint64_t> latencies;
    latencies.reserve(NUM_ITERATIONS);
    
    // ========================================================================
    // INITIALISATION DE L'HORLOGE
    // ========================================================================
    
    /*
     * struct timespec : Structure POSIX pour représenter un instant
     *   tv_sec  : secondes
     *   tv_nsec : nanosecondes (0 à 999,999,999)
     * 
     * CLOCK_MONOTONIC :
     *   - Horloge monotone croissante
     *   - NON affectée par NTP ou changements d'heure
     *   - Idéale pour mesurer des intervalles
     * 
     * Alternative CLOCK_REALTIME :
     *   - Temps réel (wall clock)
     *   - PEUT sauter (NTP, ajustements)
     *   - À ÉVITER pour les mesures de latence
     */
    struct timespec next_period;
    clock_gettime(CLOCK_MONOTONIC, &next_period);
    
    std::cout << "Démarrage de la boucle périodique..." << std::endl;
    std::cout << "(Affichage tous les 100 cycles)\n" << std::endl;
    
    // ========================================================================
    // BOUCLE PÉRIODIQUE TEMPS RÉEL
    // ========================================================================
    
    for (int i = 0; i < NUM_ITERATIONS; ++i) {
        // --------------------------------------------------------------------
        // ATTENTE DE LA PROCHAINE PÉRIODE
        // --------------------------------------------------------------------
        
        /*
         * clock_nanosleep() : Suspension précise du thread
         * 
         * Paramètres :
         *   CLOCK_MONOTONIC : horloge de référence
         *   TIMER_ABSTIME   : temps ABSOLU (pas relatif)
         *   &next_period    : instant de réveil
         *   NULL            : pas de temps restant retourné
         * 
         * POURQUOI TIMER_ABSTIME ?
         * Avec un temps relatif, les petites erreurs s'accumulent.
         * Avec un temps absolu, on spécifie l'instant exact de réveil,
         * évitant toute dérive sur le long terme.
         */
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next_period, NULL);
        
        // --------------------------------------------------------------------
        // MESURE DE LA LATENCE
        // --------------------------------------------------------------------
        
        /*
         * Immédiatement après le réveil, on mesure l'instant réel.
         * La latence est la différence entre l'instant prévu (next_period)
         * et l'instant réel (now).
         * 
         * Une latence de 0 est impossible (temps de réveil du scheduler).
         * Une latence < 100 µs est excellente avec un kernel RT.
         * Une latence > 500 µs indique un problème de configuration.
         */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        
        uint64_t latency_ns = timespec_diff_ns(next_period, now);
        latencies.push_back(latency_ns);
        
        // --------------------------------------------------------------------
        // CALCUL DE LA PROCHAINE PÉRIODE
        // --------------------------------------------------------------------
        
        /*
         * On ajoute la période à l'instant prévu.
         * Si tv_nsec dépasse 1 seconde (1,000,000,000 ns), on incrémente
         * tv_sec et on soustrait 1 seconde de tv_nsec.
         */
        next_period.tv_nsec += PERIOD_US * 1000;  // µs → ns
        if (next_period.tv_nsec >= 1000000000) {
            next_period.tv_sec++;
            next_period.tv_nsec -= 1000000000;
        }
        
        // Affichage de la progression
        if ((i + 1) % 100 == 0) {
            std::cout << "  Cycle " << std::setw(4) << (i + 1) << "/" << NUM_ITERATIONS 
                      << " - Latence courante: " << std::setw(5) << (latency_ns / 1000) 
                      << " µs" << std::endl;
        }
    }
    
    std::cout << "\n" << COLOR_GREEN << "✓ Tâche périodique terminée" << COLOR_RESET << std::endl;
    
    return latencies;
}

// ============================================================================
// FONCTION D'ANALYSE ET D'AFFICHAGE DES RÉSULTATS
// ============================================================================

/**
 * @brief Analyse et affiche les statistiques de latence
 * 
 * @param latencies Vecteur des latences en nanosecondes
 */
void display_results(const std::vector<uint64_t>& latencies)
{
    if (latencies.empty()) {
        std::cerr << COLOR_RED << "Erreur : Aucune donnée de latence" << COLOR_RESET << std::endl;
        return;
    }
    
    // Calcul des statistiques
    LatencyStats stats = calculate_stats(latencies);
    
    // Conversion en microsecondes pour l'affichage
    double min_us = stats.min_ns / 1000.0;
    double max_us = stats.max_ns / 1000.0;
    double avg_us = stats.avg_ns / 1000.0;
    double stddev_us = stats.stddev_ns / 1000.0;
    
    // Affichage des résultats
    std::cout << "\n" << COLOR_CYAN 
              << "╔══════════════════════════════════════════════════════════════╗\n"
              << "║                 RÉSULTATS DE LATENCE                         ║\n"
              << "╚══════════════════════════════════════════════════════════════╝" 
              << COLOR_RESET << "\n" << std::endl;
    
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Statistiques :" << std::endl;
    std::cout << "  • Latence minimale  : " << std::setw(8) << min_us << " µs" << std::endl;
    std::cout << "  • Latence maximale  : " << std::setw(8) << max_us << " µs";
    
    // Évaluation de la qualité
    if (max_us < 50) {
        std::cout << "  " << COLOR_GREEN << "← Excellent !" << COLOR_RESET;
    } else if (max_us < 100) {
        std::cout << "  " << COLOR_GREEN << "← Très bon" << COLOR_RESET;
    } else if (max_us < 200) {
        std::cout << "  " << COLOR_YELLOW << "← Acceptable" << COLOR_RESET;
    } else {
        std::cout << "  " << COLOR_RED << "← Vérifier la configuration" << COLOR_RESET;
    }
    std::cout << std::endl;
    
    std::cout << "  • Latence moyenne   : " << std::setw(8) << avg_us << " µs" << std::endl;
    std::cout << "  • Écart-type        : " << std::setw(8) << stddev_us << " µs" << std::endl;
    
    // Affichage de l'histogramme
    print_histogram(latencies);
    
    // Recommandations
    std::cout << "\n" << COLOR_CYAN << "💡 Recommandations :" << COLOR_RESET << std::endl;
    std::cout << std::endl;
    
    if (max_us < 100) {
        std::cout << COLOR_GREEN 
                  << "  ✓ Configuration temps réel excellente !" << COLOR_RESET << std::endl;
        std::cout << "    Votre système est prêt pour des applications temps réel" << std::endl;
        std::cout << "    avec des contraintes de latence strictes." << std::endl;
    } else {
        std::cout << COLOR_YELLOW 
                  << "  ⚠ Latence maximale élevée." << COLOR_RESET << std::endl;
        std::cout << "    Vérifiez :" << std::endl;
        std::cout << "    • Kernel RT actif : uname -r (doit contenir 'rt' ou 'realtime')" << std::endl;
        std::cout << "    • CPUs isolés : cat /sys/devices/system/cpu/isolated (doit être '2-3')" << std::endl;
        std::cout << "    • Limites RT : ulimit -r (doit être 99)" << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << COLOR_CYAN << "🔧 Pour des tests de stress approfondis :" << COLOR_RESET << std::endl;
    std::cout << "   sudo cyclictest -t1 -p 80 -a 2 -m -i 1000 -l 3600000" << std::endl;
}

// ============================================================================
// FONCTION D'AIDE
// ============================================================================

/**
 * @brief Affiche l'aide d'utilisation
 */
void print_usage(const char* program_name)
{
    std::cout << "Usage: sudo " << program_name << " [OPTIONS]\n"
              << "\n"
              << "Programme pédagogique démontrant l'utilisation des APIs temps réel Linux.\n"
              << "\n"
              << "Ce programme illustre :\n"
              << "  • Configuration SCHED_FIFO (ordonnancement temps réel)\n"
              << "  • Verrouillage mémoire avec mlockall()\n"
              << "  • Affinage CPU sur un cœur isolé\n"
              << "  • Boucle périodique avec mesure de latence\n"
              << "\n"
              << "OPTIONS:\n"
              << "  --help, -h  Affiche cette aide\n"
              << "\n"
              << "PRÉREQUIS:\n"
              << "  • Kernel RT installé (uname -r doit contenir 'rt' ou 'realtime')\n"
              << "  • CPUs isolés (isolcpus=2,3 dans cmdline)\n"
              << "  • Limites configurées (ulimit -r doit être 99)\n"
              << "  • Exécution avec sudo\n"
              << "\n"
              << "POUR DES TESTS DE STRESS RÉELS:\n"
              << "  Utilisez cyclictest (outil de référence) :\n"
              << "    sudo apt install rt-tests\n"
              << "    sudo cyclictest -t1 -p 80 -a 2 -m -i 1000 -l 10000\n"
              << std::endl;
}

// ============================================================================
// FONCTION PRINCIPALE
// ============================================================================

/**
 * @brief Point d'entrée du programme
 * 
 * Déroule la démonstration complète de configuration et d'exécution
 * d'une tâche temps réel.
 */
int main(int argc, char* argv[])
{
    // Parse des arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Option inconnue: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // En-tête
    std::cout << COLOR_CYAN
              << "\n╔══════════════════════════════════════════════════════════════╗\n"
              << "║    DÉMONSTRATION APIs TEMPS RÉEL LINUX - RASPBERRY PI 4     ║\n"
              << "╚══════════════════════════════════════════════════════════════╝"
              << COLOR_RESET << std::endl;
    
    // Informations système
    std::cout << "\nInformations système :" << std::endl;
    std::cout << "  • CPUs disponibles : " << sysconf(_SC_NPROCESSORS_ONLN) << std::endl;
    std::cout << "  • Période de test  : " << PERIOD_US << " µs" << std::endl;
    std::cout << "  • Itérations       : " << NUM_ITERATIONS << std::endl;
    
    // Configuration temps réel
    if (!configure_realtime()) {
        std::cerr << "\n" << COLOR_RED 
                  << "✗ Échec de la configuration temps réel" 
                  << COLOR_RESET << std::endl;
        return 1;
    }
    
    // Exécution de la tâche périodique
    std::vector<uint64_t> latencies = run_periodic_task();
    
    // Affichage des résultats
    display_results(latencies);
    
    // Nettoyage
    struct sched_param param;
    param.sched_priority = 0;
    sched_setscheduler(0, SCHED_OTHER, &param);
    munlockall();
    
    // Conclusion
    std::cout << "\n" << COLOR_CYAN
              << "╔══════════════════════════════════════════════════════════════╗\n"
              << "║                    FIN DE LA DÉMONSTRATION                   ║\n"
              << "╚══════════════════════════════════════════════════════════════╝"
              << COLOR_RESET << std::endl;
    
    std::cout << "\nCe programme est un EXEMPLE PÉDAGOGIQUE." << std::endl;
    std::cout << "Pour des tests de performance réels et stress tests, utilisez :" << std::endl;
    std::cout << "  sudo cyclictest -t1 -p 80 -a 2 -m -i 1000 -l 3600000\n" << std::endl;
    
    return 0;
}
