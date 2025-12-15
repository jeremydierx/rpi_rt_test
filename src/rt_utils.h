/**
 * @file rt_utils.h
 * @brief Fonctions utilitaires pour les tests temps réel
 * 
 * @author Jeremy Dierx | Code Alchimie
 * @date 2025
 * 
 * Ce fichier contient des fonctions et structures utilitaires pour :
 * - Manipulation des structures timespec
 * - Calcul de statistiques sur les latences
 * - Affichage d'histogrammes
 * - Codes couleur pour le terminal
 */

#ifndef RT_UTILS_H
#define RT_UTILS_H

#include <stdint.h>
#include <time.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

// ============================================================================
// CODES COULEUR ANSI POUR LE TERMINAL
// ============================================================================

/**
 * Codes d'échappement ANSI pour colorer la sortie terminal.
 * 
 * Ces codes fonctionnent sur la plupart des terminaux modernes (Linux, macOS,
 * Windows Terminal, etc.). Ils permettent d'améliorer la lisibilité des
 * résultats en utilisant des couleurs pour indiquer succès/échec.
 * 
 * Format : \033[XXm où XX est le code couleur
 * - 0  : Reset (retour à la normale)
 * - 31 : Rouge
 * - 32 : Vert
 * - 33 : Jaune
 * - 34 : Bleu
 * - 36 : Cyan
 */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

// ============================================================================
// FONCTIONS DE MANIPULATION DU TEMPS
// ============================================================================

/**
 * @brief Calcule la différence entre deux instants en nanosecondes
 * 
 * Cette fonction calcule (end - start) en nanosecondes.
 * Elle gère correctement les cas où tv_nsec de end est inférieur à celui de start.
 * 
 * @param start Instant de début
 * @param end Instant de fin
 * @return Différence en nanosecondes (0 si end < start)
 * 
 * @note La fonction retourne 0 si end < start pour éviter les valeurs négatives
 *       avec un type uint64_t.
 * 
 * EXEMPLE D'UTILISATION :
 * @code
 * struct timespec start, end;
 * clock_gettime(CLOCK_MONOTONIC, &start);
 * // ... code à mesurer ...
 * clock_gettime(CLOCK_MONOTONIC, &end);
 * uint64_t elapsed_ns = timespec_diff_ns(start, end);
 * @endcode
 */
inline uint64_t timespec_diff_ns(const struct timespec& start, const struct timespec& end)
{
    // Conversion en nanosecondes totales
    // Attention au dépassement : tv_sec peut être grand, on utilise uint64_t
    uint64_t start_ns = static_cast<uint64_t>(start.tv_sec) * 1000000000ULL 
                      + static_cast<uint64_t>(start.tv_nsec);
    uint64_t end_ns = static_cast<uint64_t>(end.tv_sec) * 1000000000ULL 
                    + static_cast<uint64_t>(end.tv_nsec);
    
    // Protection contre les valeurs négatives (ne devrait pas arriver avec CLOCK_MONOTONIC)
    return (end_ns > start_ns) ? (end_ns - start_ns) : 0;
}

/**
 * @brief Ajoute des microsecondes à un timespec
 * 
 * @param ts Structure timespec à modifier
 * @param us Microsecondes à ajouter
 * 
 * @note Gère automatiquement le débordement de tv_nsec
 */
inline void timespec_add_us(struct timespec& ts, uint64_t us)
{
    ts.tv_nsec += static_cast<long>(us * 1000);
    
    // Normalisation si tv_nsec >= 1 seconde
    while (ts.tv_nsec >= 1000000000) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000;
    }
}

// ============================================================================
// STRUCTURES DE STATISTIQUES
// ============================================================================

/**
 * @brief Structure contenant les statistiques de latence
 * 
 * Cette structure regroupe les métriques essentielles pour évaluer
 * la qualité d'un système temps réel :
 * 
 * - min_ns : Meilleur cas (latence minimale)
 * - max_ns : Pire cas (latence maximale) - LE PLUS IMPORTANT en temps réel
 * - avg_ns : Cas moyen (latence moyenne)
 * - stddev_ns : Variabilité (écart-type)
 * 
 * En temps réel, on s'intéresse surtout au PIRE CAS (max_ns) car c'est
 * lui qui détermine si les deadlines peuvent être respectées.
 */
struct LatencyStats {
    uint64_t min_ns;    ///< Latence minimale en nanosecondes
    uint64_t max_ns;    ///< Latence maximale en nanosecondes
    double avg_ns;      ///< Latence moyenne en nanosecondes
    double stddev_ns;   ///< Écart-type en nanosecondes
};

// ============================================================================
// FONCTIONS DE CALCUL STATISTIQUE
// ============================================================================

/**
 * @brief Calcule les statistiques (min, max, moyenne, écart-type) d'un ensemble de latences
 * 
 * @param latencies Vecteur de latences en nanosecondes
 * @return Structure LatencyStats contenant les statistiques
 * 
 * ALGORITHME :
 * 1. Trouve le minimum et maximum avec std::min_element/max_element
 * 2. Calcule la moyenne avec une somme puis division
 * 3. Calcule l'écart-type avec la formule : sqrt(E[(X-µ)²])
 * 
 * COMPLEXITÉ : O(n) où n est le nombre de latences
 * 
 * @note Retourne des valeurs nulles si le vecteur est vide
 */
inline LatencyStats calculate_stats(const std::vector<uint64_t>& latencies)
{
    LatencyStats stats = {0, 0, 0.0, 0.0};
    
    if (latencies.empty()) {
        return stats;
    }
    
    // Calcul du minimum et maximum
    // std::min_element et std::max_element retournent des itérateurs
    auto min_it = std::min_element(latencies.begin(), latencies.end());
    auto max_it = std::max_element(latencies.begin(), latencies.end());
    stats.min_ns = *min_it;
    stats.max_ns = *max_it;
    
    // Calcul de la moyenne
    // Utilisation de uint64_t pour la somme pour éviter les dépassements
    uint64_t sum = 0;
    for (const auto& lat : latencies) {
        sum += lat;
    }
    stats.avg_ns = static_cast<double>(sum) / static_cast<double>(latencies.size());
    
    // Calcul de l'écart-type
    // Formule : σ = sqrt(Σ(xi - µ)² / N)
    double variance = 0.0;
    for (const auto& lat : latencies) {
        double diff = static_cast<double>(lat) - stats.avg_ns;
        variance += diff * diff;
    }
    variance /= static_cast<double>(latencies.size());
    stats.stddev_ns = std::sqrt(variance);
    
    return stats;
}

/**
 * @brief Calcule le percentile d'un ensemble de latences
 * 
 * @param latencies Vecteur de latences (sera modifié - trié)
 * @param percentile Percentile à calculer (0-100)
 * @return Valeur du percentile en nanosecondes
 * 
 * EXEMPLE :
 * - percentile = 99 → valeur en dessous de laquelle 99% des latences se situent
 * - percentile = 50 → médiane
 * 
 * @note Le vecteur passé est trié en place pour des raisons de performance
 */
inline uint64_t calculate_percentile(std::vector<uint64_t>& latencies, double percentile)
{
    if (latencies.empty()) {
        return 0;
    }
    
    // Tri du vecteur
    std::sort(latencies.begin(), latencies.end());
    
    // Calcul de l'index
    size_t index = static_cast<size_t>(
        (percentile / 100.0) * static_cast<double>(latencies.size() - 1)
    );
    
    return latencies[index];
}

// ============================================================================
// FONCTIONS D'AFFICHAGE
// ============================================================================

/**
 * @brief Affiche un histogramme ASCII des latences
 * 
 * Cette fonction crée une visualisation textuelle de la distribution des latences.
 * Elle est utile pour identifier rapidement :
 * - La concentration des valeurs (pic principal)
 * - Les valeurs aberrantes (outliers)
 * - La forme de la distribution (gaussienne, exponentielle, etc.)
 * 
 * @param latencies Vecteur de latences en nanosecondes
 * @param num_bins Nombre de barres dans l'histogramme (défaut: 15)
 * 
 * EXEMPLE DE SORTIE :
 * @code
 * Histogramme des latences:
 *      0-   100 µs: ████████████████████████████████ (7823)
 *    100-   200 µs: ████████ (1456)
 *    200-   300 µs: ██ (398)
 *    ...
 * @endcode
 */
inline void print_histogram(const std::vector<uint64_t>& latencies, int num_bins = 15)
{
    if (latencies.empty()) {
        std::cout << "  Aucune donnée pour l'histogramme" << std::endl;
        return;
    }
    
    // Trouver les bornes
    uint64_t min_lat = *std::min_element(latencies.begin(), latencies.end());
    uint64_t max_lat = *std::max_element(latencies.begin(), latencies.end());
    
    // Cas particulier : toutes les valeurs sont identiques
    if (min_lat == max_lat) {
        std::cout << "\n  Histogramme: Toutes les latences sont identiques ("
                  << min_lat / 1000 << " µs)" << std::endl;
        return;
    }
    
    // Calcul de la largeur de chaque bin
    uint64_t range = max_lat - min_lat;
    uint64_t bin_width = range / static_cast<uint64_t>(num_bins);
    if (bin_width == 0) bin_width = 1;  // Éviter la division par zéro
    
    // Compter les valeurs dans chaque bin
    std::vector<int> bins(num_bins, 0);
    
    for (const auto& lat : latencies) {
        int bin_index = static_cast<int>((lat - min_lat) / bin_width);
        // S'assurer que l'index est dans les limites
        if (bin_index >= num_bins) bin_index = num_bins - 1;
        if (bin_index < 0) bin_index = 0;
        bins[bin_index]++;
    }
    
    // Trouver le maximum pour normaliser les barres
    int max_count = *std::max_element(bins.begin(), bins.end());
    if (max_count == 0) max_count = 1;  // Éviter la division par zéro
    
    // Largeur maximale des barres
    const int max_bar_width = 40;
    
    // Affichage
    std::cout << "\n  " << COLOR_CYAN << "Histogramme des latences:" << COLOR_RESET << std::endl;
    
    for (int i = 0; i < num_bins; ++i) {
        // Calcul des bornes du bin en microsecondes
        uint64_t range_start_us = (min_lat + static_cast<uint64_t>(i) * bin_width) / 1000;
        uint64_t range_end_us = (min_lat + static_cast<uint64_t>(i + 1) * bin_width) / 1000;
        
        // Calcul de la longueur de la barre
        int bar_length = (bins[i] * max_bar_width) / max_count;
        
        // Choix de la couleur en fonction de la position (vert pour les faibles, rouge pour les hautes)
        const char* color;
        if (i < num_bins / 3) {
            color = COLOR_GREEN;
        } else if (i < 2 * num_bins / 3) {
            color = COLOR_YELLOW;
        } else {
            color = COLOR_RED;
        }
        
        // Affichage de la ligne
        std::cout << "    " << std::setw(6) << range_start_us << "-"
                  << std::setw(6) << range_end_us << " µs: ";
        
        std::cout << color;
        for (int j = 0; j < bar_length; ++j) {
            std::cout << "█";
        }
        std::cout << COLOR_RESET;
        
        // Affichage du compte si la barre est trop courte
        if (bar_length < 3 && bins[i] > 0) {
            std::cout << " (" << bins[i] << ")";
        } else if (bins[i] > 0) {
            std::cout << " " << bins[i];
        }
        
        std::cout << std::endl;
    }
}

/**
 * @brief Affiche un tableau comparatif des résultats
 * 
 * @param no_rt_stats Statistiques du test sans RT (peut être NULL)
 * @param rt_stats Statistiques du test avec RT (peut être NULL)
 */
inline void print_comparison_table(const LatencyStats* no_rt_stats, 
                                    const LatencyStats* rt_stats)
{
    std::cout << "\n" << COLOR_CYAN 
              << "╔════════════════════════════════════════════════════════════════╗\n"
              << "║              TABLEAU COMPARATIF DES RÉSULTATS                  ║\n"
              << "╠════════════════════════════════════════════════════════════════╣\n"
              << "║  Métrique       │  Sans RT      │  Avec RT      │  Amélioration║\n"
              << "╠════════════════════════════════════════════════════════════════╣"
              << COLOR_RESET << std::endl;
    
    if (no_rt_stats && rt_stats) {
        // Latence max
        double improvement_max = (static_cast<double>(no_rt_stats->max_ns) - 
                                  static_cast<double>(rt_stats->max_ns)) / 
                                 static_cast<double>(no_rt_stats->max_ns) * 100.0;
        
        std::cout << std::fixed << std::setprecision(1);
        std::cout << "║  Latence max    │  " 
                  << std::setw(8) << no_rt_stats->max_ns / 1000 << " µs │  "
                  << std::setw(8) << rt_stats->max_ns / 1000 << " µs │  "
                  << COLOR_GREEN << std::setw(6) << improvement_max << "%" 
                  << COLOR_RESET << "    ║" << std::endl;
        
        // Latence moyenne
        double improvement_avg = (no_rt_stats->avg_ns - rt_stats->avg_ns) / 
                                 no_rt_stats->avg_ns * 100.0;
        
        std::cout << "║  Latence moy    │  " 
                  << std::setw(8) << no_rt_stats->avg_ns / 1000.0 << " µs │  "
                  << std::setw(8) << rt_stats->avg_ns / 1000.0 << " µs │  "
                  << COLOR_GREEN << std::setw(6) << improvement_avg << "%" 
                  << COLOR_RESET << "    ║" << std::endl;
        
        // Écart-type
        double improvement_std = (no_rt_stats->stddev_ns - rt_stats->stddev_ns) / 
                                 no_rt_stats->stddev_ns * 100.0;
        
        std::cout << "║  Écart-type     │  " 
                  << std::setw(8) << no_rt_stats->stddev_ns / 1000.0 << " µs │  "
                  << std::setw(8) << rt_stats->stddev_ns / 1000.0 << " µs │  "
                  << COLOR_GREEN << std::setw(6) << improvement_std << "%" 
                  << COLOR_RESET << "    ║" << std::endl;
    }
    
    std::cout << COLOR_CYAN 
              << "╚════════════════════════════════════════════════════════════════╝"
              << COLOR_RESET << std::endl;
}

#endif // RT_UTILS_H

