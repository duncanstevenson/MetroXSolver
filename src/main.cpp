#include "types.hpp"
#include "parser.hpp"
#include "solver.hpp"
#include <iostream>
#include <iomanip>

void print_banner() {
    std::cout << "=========================================================\n";
    std::cout << "          METRO X THEORETICAL MAX SCORE SOLVER           \n";
    std::cout << "=========================================================\n\n";
}

void print_map_summary(const Scenario& sc) {
    std::cout << "[Map Summary]\n";
    std::cout << "  Total Stations: " << sc.map.total_stations << "\n";
    std::cout << "  Total Routes:   " << sc.map.routes.size() << "\n\n";

    std::cout << "  Routes:\n";
    for (const auto& r : sc.map.routes) {
        int opp_turn = sc.opponent_finish_turns[r.id];
        std::string opp_str = (opp_turn < 99999) ? ("Turn " + std::to_string(opp_turn)) : "None";
        std::cout << "    * " << std::left << std::setw(12) << r.name 
                  << " | Windows: " << r.max_windows
                  << " | Gold: " << std::setw(2) << r.gold_bonus
                  << " | Silver: " << std::setw(2) << r.silver_bonus
                  << " | Stations: " << std::setw(2) << r.stations.size()
                  << " | Opponent Finished: " << opp_str << "\n";
    }

    std::cout << "\n  High-Degree Transfer Hubs:\n";
    for (int s = 0; s < sc.map.total_stations; ++s) {
        if (sc.map.station_degrees[s] >= 2) {
            std::cout << "    * Station " << std::left << std::setw(8) << sc.map.station_names[s]
                      << " (Degree: " << sc.map.station_degrees[s]
                      << ", Transfer Value: " << sc.map.station_transfer_pts[s] << " pts)\n";
        }
    }

    std::cout << "\n[Turn Sequence] (" << sc.turns.size() << " turns):\n  ";
    for (size_t i = 0; i < sc.turns.size(); ++i) {
        std::cout << sc.turns[i].to_string() << " ";
        if ((i + 1) % 10 == 0 && i + 1 < sc.turns.size()) std::cout << "\n  ";
    }
    std::cout << "\n\n";

    std::cout << "[Settings]\n";
    std::cout << "  Short uses of cards (underfill): " 
              << (sc.settings.allow_short_use ? "ALLOWED (can stop early to save transfers)" 
                                              : "DISABLED (forced full fill unless blocked)") 
              << "\n\n";
}

void print_playbook(const Scenario& sc, const Solution& sol) {
    std::cout << "=========================================================\n";
    std::cout << "                    OPTIMAL PLAYBOOK                     \n";
    std::cout << "=========================================================\n";

    for (size_t t = 0; t < sol.playbook.size(); ++t) {
        const auto& move = sol.playbook[t];
        const auto& card = sc.turns[t];
        int turn_num = (int)t + 1;

        std::cout << "Turn " << std::right << std::setw(2) << turn_num << " | Card: " 
                  << std::left << std::setw(8) << card.to_string() << " | ";

        if (card.type == CardType::FREE) {
            int sid = move.marked_stations.empty() ? -1 : move.marked_stations[0];
            std::cout << "FREE SPACE -> Marked Station: " 
                      << (sid != -1 ? sc.map.station_names[sid] : "None");
        } else {
            std::string rname = (move.route_idx >= 0) ? sc.map.routes[move.route_idx].name : "Unknown";
            std::cout << "Route: " << std::left << std::setw(8) << rname << " | ";

            if (move.is_transfer) {
                std::cout << "TRANSFER on " << sc.map.station_names[move.transfer_station]
                          << " (+" << move.transfer_points << " pts)";
            } else if (move.marked_stations.empty()) {
                std::cout << "Burned window (0 stations marked)";
            } else {
                std::cout << "Marked: [";
                for (size_t i = 0; i < move.marked_stations.size(); ++i) {
                    std::cout << sc.map.station_names[move.marked_stations[i]];
                    if (i + 1 < move.marked_stations.size()) std::cout << ", ";
                }
                std::cout << "]";
            }
        }

        // Completion announcements
        if (!move.completed_routes.empty()) {
            for (size_t i = 0; i < move.completed_routes.size(); ++i) {
                int rid = move.completed_routes[i];
                int bonus = move.completion_bonuses[i];
                bool is_gold = move.is_gold_bonus[i];
                std::cout << "\n        >>> COMPLETED " << sc.map.routes[rid].name 
                          << "! Awarded " << (is_gold ? "GOLD DIAMOND" : "SILVER SQUARE")
                          << " (+" << bonus << " pts)";
            }
        }
        std::cout << "\n";
    }

    std::cout << "\n=========================================================\n";
    std::cout << "                     SCORE SUMMARY                       \n";
    std::cout << "=========================================================\n";
    std::cout << "  Route Completion Bonuses : +" << sol.route_score << " pts\n";
    std::cout << "  Transfer Stations Points : +" << sol.transfer_score << " pts\n";
    std::cout << "  Empty Stations (" << sol.empty_stations << " left)  : " 
              << sol.penalty << " pts (penalty)\n";
    std::cout << "---------------------------------------------------------\n";
    std::cout << "  TOTAL THEORETICAL MAX    : " << sol.total_score << " POINTS\n";
    std::cout << "=========================================================\n";
    std::cout << "  States Evaluated         : " << sol.states_evaluated << "\n";
    std::cout << "  Compute Time             : " << sol.search_time_ms << " ms\n\n";
}

int main(int argc, char* argv[]) {
    print_banner();

    std::string scenario_path = "scenarios/sample_game.txt";
    bool cli_override_settings = false;
    bool cli_allow_short_use = true;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--strict" || arg == "--disable-short-use" || arg == "--no-underfill") {
            cli_override_settings = true;
            cli_allow_short_use = false;
        } else if (arg == "--allow-short-use" || arg == "--underfill") {
            cli_override_settings = true;
            cli_allow_short_use = true;
        } else if (arg.rfind("--", 0) != 0) {
            scenario_path = arg;
        }
    }

    try {
        std::cout << "Loading scenario from: " << scenario_path << "\n\n";
        Scenario scenario = load_scenario(scenario_path);

        if (cli_override_settings) {
            scenario.settings.allow_short_use = cli_allow_short_use;
        }

        print_map_summary(scenario);

        std::cout << "Running optimizer (DFBnB + Beam Search)...\n";
        MetroXSolver solver(scenario);
        Solution sol = solver.solve();

        print_playbook(scenario, sol);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
