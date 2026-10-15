#include "types.hpp"
#include "parser.hpp"
#include "solver.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

static int g_tests_run = 0;
static int g_tests_passed = 0;

#define ASSERT_EQ(actual, expected) do { \
    if ((actual) != (expected)) { \
        std::cerr << "  FAIL: " << #actual << " == " << #expected \
                  << " (got " << (actual) << ", expected " << (expected) << ") at " \
                  << __FILE__ << ":" << __LINE__ << std::endl; \
        return false; \
    } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        std::cerr << "  FAIL: " << #cond << " is false at " \
                  << __FILE__ << ":" << __LINE__ << std::endl; \
        return false; \
    } \
} while(0)

#define RUN_TEST(fn) do { \
    g_tests_run++; \
    std::cout << "[TEST " << g_tests_run << "] " << #fn << "... "; \
    if (fn()) { \
        std::cout << "PASSED" << std::endl; \
        g_tests_passed++; \
    } else { \
        std::cout << "FAILED" << std::endl; \
    } \
} while(0)

// Test 1: Penalty table for empty stations
bool test_penalty_table() {
    for (int i = 0; i <= 4; ++i) {
        ASSERT_EQ(get_empty_stations_penalty(i), 0);
    }
    ASSERT_EQ(get_empty_stations_penalty(5), -1);
    ASSERT_EQ(get_empty_stations_penalty(6), -2);
    ASSERT_EQ(get_empty_stations_penalty(7), -3);
    ASSERT_EQ(get_empty_stations_penalty(8), -4);
    ASSERT_EQ(get_empty_stations_penalty(9), -4);
    ASSERT_EQ(get_empty_stations_penalty(10), -5);
    ASSERT_EQ(get_empty_stations_penalty(11), -5);
    ASSERT_EQ(get_empty_stations_penalty(12), -6);
    ASSERT_EQ(get_empty_stations_penalty(13), -6);
    ASSERT_EQ(get_empty_stations_penalty(14), -7);
    ASSERT_EQ(get_empty_stations_penalty(15), -7);
    ASSERT_EQ(get_empty_stations_penalty(16), -8);
    ASSERT_EQ(get_empty_stations_penalty(17), -8);
    ASSERT_EQ(get_empty_stations_penalty(18), -9);
    ASSERT_EQ(get_empty_stations_penalty(19), -9);
    ASSERT_EQ(get_empty_stations_penalty(20), -10);
    ASSERT_EQ(get_empty_stations_penalty(99), -10);
    return true;
}

// Test 2: Card parsing
bool test_card_parsing() {
    Card c1 = parse_card_token("3");
    ASSERT_EQ((int)c1.type, (int)CardType::NUMBER);
    ASSERT_EQ(c1.value, 3);

    Card c2 = parse_card_token("S2");
    ASSERT_EQ((int)c2.type, (int)CardType::SKIP);
    ASSERT_EQ(c2.value, 2);

    Card c3 = parse_card_token("SKIP3");
    ASSERT_EQ((int)c3.type, (int)CardType::SKIP);
    ASSERT_EQ(c3.value, 3);

    Card c4 = parse_card_token("TRANSFER");
    ASSERT_EQ((int)c4.type, (int)CardType::TRANSFER);

    Card c5 = parse_card_token("T");
    ASSERT_EQ((int)c5.type, (int)CardType::TRANSFER);

    Card c6 = parse_card_token("FREE");
    ASSERT_EQ((int)c6.type, (int)CardType::FREE);

    Card c7 = parse_card_token("F");
    ASSERT_EQ((int)c7.type, (int)CardType::FREE);

    return true;
}

// Test 3: Scenario parser with MAP, SETTINGS, OPPONENTS, TURNS
bool test_scenario_parser() {
    std::string text = 
        "[MAP]\n"
        "LineA | 2 | 6 | 3 | S1 S2 S3\n"
        "LineB | 1 | 4 | 2 | S2 S4\n"
        "\n"
        "[SETTINGS]\n"
        "allow_short_use: false\n"
        "\n"
        "[OPPONENTS]\n"
        "LineA : 3\n"
        "\n"
        "[TURNS]\n"
        "3 S2 T FREE\n";

    Scenario sc = parse_scenario_string(text);
    ASSERT_EQ((int)sc.map.routes.size(), 2);
    ASSERT_EQ((int)sc.map.total_stations, 4); // S1, S2, S3, S4
    ASSERT_EQ(sc.settings.allow_short_use, false);
    ASSERT_EQ((int)sc.turns.size(), 4);
    ASSERT_EQ(sc.opponent_finish_turns[0], 3); // LineA finished on turn 3
    ASSERT_EQ(sc.opponent_finish_turns[1], 999999); // LineB unlisted

    // S2 is shared by LineA and LineB -> degree 2, transfer points 4
    int s2_id = -1;
    for (size_t i = 0; i < sc.map.station_names.size(); ++i) {
        if (sc.map.station_names[i] == "S2") s2_id = (int)i;
    }
    ASSERT_TRUE(s2_id >= 0);
    ASSERT_EQ(sc.map.station_degrees[s2_id], 2);
    ASSERT_EQ(sc.map.station_transfer_pts[s2_id], 4);

    return true;
}

// Test 4: Move generation for Number card and station blocking
bool test_number_card_move_generation() {
    std::string text = 
        "[MAP]\n"
        "LineA | 2 | 5 | 2 | S1 S2 S3 S4\n"
        "\n"
        "[TURNS]\n"
        "3\n";

    Scenario sc = parse_scenario_string(text);
    MetroXSolver solver(sc);

    // Initial state: empty board
    BoardState state;
    auto moves = solver.generate_moves(state, 0);
    ASSERT_TRUE(!moves.empty());

    // First move should mark up to 3 stations
    ASSERT_EQ(moves[0].route_idx, 0);
    ASSERT_EQ((int)moves[0].marked_stations.size(), 3);

    // If S2 is already marked, a Number card 3 should STOP before S2 (marking only S1)
    state.stations_marked.set(1); // mark S2
    auto blocked_moves = solver.generate_moves(state, 0);
    ASSERT_TRUE(!blocked_moves.empty());
    // In strict mode or default mode, since S2 is marked, train stops at S1
    ASSERT_EQ((int)blocked_moves[0].marked_stations.size(), 1);
    ASSERT_EQ(blocked_moves[0].marked_stations[0], 0); // S1

    return true;
}

// Test 5: Move generation for Skip card
bool test_skip_card_move_generation() {
    std::string text = 
        "[MAP]\n"
        "LineA | 2 | 5 | 2 | S1 S2 S3 S4\n"
        "\n"
        "[TURNS]\n"
        "S2\n";

    Scenario sc = parse_scenario_string(text);
    MetroXSolver solver(sc);

    // If S2 is marked, Skip 2 should jump over S2 and mark S1 and S3!
    BoardState state;
    state.stations_marked.set(1); // S2 is marked

    auto moves = solver.generate_moves(state, 0);
    ASSERT_TRUE(!moves.empty());
    ASSERT_EQ((int)moves[0].marked_stations.size(), 2);
    ASSERT_EQ(moves[0].marked_stations[0], 0); // S1
    ASSERT_EQ(moves[0].marked_stations[1], 2); // S3 (jumped over S2!)

    return true;
}

// Test 6: Transfer card mechanics
bool test_transfer_card_mechanics() {
    std::string text = 
        "[MAP]\n"
        "LineA | 2 | 5 | 2 | S1 S2 S3\n"
        "LineB | 1 | 5 | 2 | S2 S4\n"
        "\n"
        "[TURNS]\n"
        "TRANSFER\n";

    Scenario sc = parse_scenario_string(text);
    MetroXSolver solver(sc);

    BoardState state;
    state.stations_marked.set(0); // S1 already marked

    // LineA first empty station is now S2 (Transfer hub, deg 2 = 4 pts)
    auto moves = solver.generate_moves(state, 0);
    bool found_s2_transfer = false;
    for (const auto& m : moves) {
        if (m.route_idx == 0 && m.is_transfer && m.transfer_points == 4) {
            found_s2_transfer = true;
            break;
        }
    }
    ASSERT_TRUE(found_s2_transfer);
    return true;
}

// Test 7: Window exhaustion prevents route usage
bool test_window_exhaustion() {
    std::string text = 
        "[MAP]\n"
        "LineA | 1 | 5 | 2 | S1 S2\n"
        "LineB | 2 | 5 | 2 | S3 S4\n"
        "\n"
        "[TURNS]\n"
        "2\n";

    Scenario sc = parse_scenario_string(text);
    MetroXSolver solver(sc);

    BoardState state;
    state.windows_used[0] = 1; // LineA used its 1 allowed window

    auto moves = solver.generate_moves(state, 0);
    for (const auto& m : moves) {
        ASSERT_TRUE(m.route_idx != 0); // LineA should NOT be available
    }
    return true;
}

// Test 8: Free Space card marks any station without window consumption
bool test_free_space_card() {
    std::string text = 
        "[MAP]\n"
        "LineA | 1 | 5 | 2 | S1 S2 S3\n"
        "\n"
        "[TURNS]\n"
        "FREE\n";

    Scenario sc = parse_scenario_string(text);
    MetroXSolver solver(sc);

    BoardState state;
    state.windows_used[0] = 1; // LineA windows already exhausted!

    auto moves = solver.generate_moves(state, 0);
    // FREE card can still be played!
    ASSERT_TRUE(!moves.empty());
    ASSERT_EQ(moves[0].route_idx, -1); // Route index is -1 for Free card
    return true;
}

// Test 9: Route completion bonuses (Gold vs Silver timing)
bool test_route_completion_timing() {
    std::string text = 
        "[MAP]\n"
        "LineA | 2 | 6 | 2 | S1 S2\n"
        "\n"
        "[OPPONENTS]\n"
        "LineA : 1\n" // Opponent finishes on turn 1
        "\n"
        "[TURNS]\n"
        "2\n";

    Scenario sc = parse_scenario_string(text);
    MetroXSolver solver(sc);

    // Completed on turn 1 -> On or before turn 1 opponent finish -> Gold (+6)
    Solution sol = solver.solve(false);
    ASSERT_EQ(sol.route_score, 6);

    // Now scenario where opponent finished before turn 1 (impossible in game, but say turn 0)
    std::string text2 = 
        "[MAP]\n"
        "LineA | 2 | 6 | 2 | S1 S2\n"
        "\n"
        "[OPPONENTS]\n"
        "LineA : 0\n"
        "\n"
        "[TURNS]\n"
        "2\n";

    Scenario sc2 = parse_scenario_string(text2);
    MetroXSolver solver2(sc2);
    Solution sol2 = solver2.solve(false);
    ASSERT_EQ(sol2.route_score, 2); // Silver bonus awarded (+2)

    return true;
}

// Test 10: Dead-route upper-bound pruning accounts for Free cards
bool test_upper_bound_pruning_with_free_cards() {
    // 2-turn scenario: Turn 0 is '2', Turn 1 is 'FREE'
    std::string text_with_free = 
        "[MAP]\n"
        "LineA | 1 | 6 | 2 | S1 S2 S3\n"
        "\n"
        "[TURNS]\n"
        "2 FREE\n";

    Scenario sc = parse_scenario_string(text_with_free);
    MetroXSolver solver(sc);

    // State at turn 1: LineA window exhausted, S1 & S2 marked, S3 unmarked
    BoardState state;
    state.windows_used[0] = 1; // 0 windows left
    state.stations_marked.set(0); // S1
    state.stations_marked.set(1); // S2
    // S3 (idx 2) is empty and LineA is dead on normal turns

    // With 1 FREE card remaining at turn 1, compute_upper_bound must NOT prune LineA's completion bonus!
    int ub = solver.compute_upper_bound(state, 1);
    ASSERT_TRUE(ub >= 6); // LineA's gold bonus (+6) should be preserved in UB!

    return true;
}

// Test 11: End-to-end full solve on sample scenario
bool test_end_to_end_solver() {
    std::string text = 
        "[MAP]\n"
        "LineA | 2 | 6 | 3 | S1 S2 S3 S4\n"
        "LineB | 2 | 4 | 2 | S4 S5 S6\n"
        "\n"
        "[TURNS]\n"
        "3 2 3\n";

    Scenario sc = parse_scenario_string(text);
    MetroXSolver solver(sc);
    Solution sol = solver.solve(false);

    // Total stations = 6.
    // Turn 1: LineA (3) -> marks S1, S2, S3
    // Turn 2: LineA (2) -> marks S4 (LineA complete: +6 pts)
    // Turn 3: LineB (3) -> marks S5, S6 (LineB complete: +4 pts)
    // All 6 stations marked -> 0 empty stations penalty = 0.
    // S4 is shared (deg 2) -> transfer on S4 if transfer was played, here only completion bonuses: 6 + 4 = 10 pts.
    ASSERT_EQ(sol.total_score, 10);
    ASSERT_EQ((int)sol.playbook.size(), 3);
    return true;
}

int main() {
    std::cout << "========================================\n";
    std::cout << "       METRO X C++ TEST SUITE           \n";
    std::cout << "========================================\n";

    RUN_TEST(test_penalty_table);
    RUN_TEST(test_card_parsing);
    RUN_TEST(test_scenario_parser);
    RUN_TEST(test_number_card_move_generation);
    RUN_TEST(test_skip_card_move_generation);
    RUN_TEST(test_transfer_card_mechanics);
    RUN_TEST(test_window_exhaustion);
    RUN_TEST(test_free_space_card);
    RUN_TEST(test_route_completion_timing);
    RUN_TEST(test_upper_bound_pruning_with_free_cards);
    RUN_TEST(test_end_to_end_solver);

    std::cout << "========================================\n";
    std::cout << "Tests Passed: " << g_tests_passed << " / " << g_tests_run << "\n";
    std::cout << "========================================\n";

    return (g_tests_passed == g_tests_run) ? 0 : 1;
}
