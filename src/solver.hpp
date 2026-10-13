#pragma once
#include "types.hpp"
#include "parser.hpp"
#include <vector>
#include <iostream>
#include <chrono>
#include <numeric>

struct Solution {
    int total_score = -9999;
    int route_score = 0;
    int transfer_score = 0;
    int empty_stations = 0;
    int penalty = 0;
    std::vector<Move> playbook;
    uint64_t states_evaluated = 0;
    double search_time_ms = 0.0;
};

// Custom hash for BoardState transposition table
struct StateKey {
    StationMask stations_marked;
    uint64_t packed_windows = 0;
    int turn = 0;

    bool operator==(const StateKey& other) const {
        return turn == other.turn &&
               packed_windows == other.packed_windows &&
               stations_marked == other.stations_marked;
    }
};

struct StateKeyHash {
    std::size_t operator()(const StateKey& k) const {
        // Hash combination of bitset and packed windows
        std::hash<std::string> str_hash;
        size_t h1 = std::hash<uint64_t>{}(k.packed_windows);
        size_t h2 = std::hash<int>{}(k.turn);
        // Bitset hashing: hash its string or bytes
        size_t h3 = str_hash(k.stations_marked.to_string());
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class MetroXSolver {
public:
    MetroXSolver(const Scenario& sc) : scenario(sc), map(sc.map) {
        precalculate();
    }

    Solution solve() {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Phase 1: Fast Beam Search to establish a strong lower bound (LB)
        run_beam_search(3000);
        std::cout << "[Solver] Warm-start Beam Search finished with score: " 
                  << best_score << " (LB established)\n";

        // Phase 2: Depth-First Branch and Bound (DFBnB)
        BoardState initial_state;
        std::vector<Move> current_path;
        transposition_table.clear();
        states_evaluated = 0;

        dfbnb(initial_state, 0, current_path);

        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end_time - start_time;

        Solution sol;
        sol.total_score = best_score;
        sol.route_score = best_solution_breakdown.route_score;
        sol.transfer_score = best_solution_breakdown.transfer_score;
        sol.empty_stations = best_solution_breakdown.empty_stations;
        sol.penalty = best_solution_breakdown.penalty;
        sol.playbook = best_playbook;
        sol.states_evaluated = states_evaluated;
        sol.search_time_ms = elapsed.count();

        return sol;
    }

private:
    Scenario scenario;
    GameMap map;
    int best_score = -9999;
    std::vector<Move> best_playbook;
    struct ScoreBreakdown {
        int route_score = 0;
        int transfer_score = 0;
        int empty_stations = 0;
        int penalty = 0;
    } best_solution_breakdown;

    uint64_t states_evaluated = 0;
    std::unordered_map<StateKey, int, StateKeyHash> transposition_table;

    // Precalculated remaining transfer counts per turn
    std::vector<int> remaining_transfers;

    void precalculate() {
        int T = (int)scenario.turns.size();
        remaining_transfers.assign(T + 1, 0);
        for (int i = T - 1; i >= 0; --i) {
            remaining_transfers[i] = remaining_transfers[i + 1] + 
                (scenario.turns[i].type == CardType::TRANSFER ? 1 : 0);
        }
    }

    StateKey make_key(const BoardState& state, int turn) const {
        StateKey k;
        k.stations_marked = state.stations_marked;
        k.turn = turn;
        // Pack first 16 routes windows used (4 bits each = 64 bits)
        uint64_t pw = 0;
        for (int i = 0; i < std::min((int)map.routes.size(), 16); ++i) {
            pw |= ((uint64_t)(state.windows_used[i] & 0xF) << (i * 4));
        }
        k.packed_windows = pw;
        return k;
    }

    int compute_upper_bound(const BoardState& state, int turn) {
        int ub = state.route_score + state.transfer_score;
        int turn_1based = turn + 1;

        // 1. Max remaining route bonuses
        for (const auto& r : map.routes) {
            if (!(state.completed_routes_mask & (1U << r.id))) {
                if (turn_1based <= scenario.opponent_finish_turns[r.id]) {
                    ub += r.gold_bonus;
                } else {
                    ub += r.silver_bonus;
                }
            }
        }

        // 2. Max remaining transfers
        int num_transfers_left = remaining_transfers[turn];
        if (num_transfers_left > 0) {
            // Find top transfer values among currently empty stations
            std::vector<int> empty_transfer_vals;
            for (int s = 0; s < map.total_stations; ++s) {
                if (!state.stations_marked.test(s)) {
                    empty_transfer_vals.push_back(map.station_transfer_pts[s]);
                }
            }
            std::sort(empty_transfer_vals.rbegin(), empty_transfer_vals.rend());
            int count = std::min((int)empty_transfer_vals.size(), num_transfers_left);
            for (int i = 0; i < count; ++i) {
                ub += empty_transfer_vals[i];
            }
        }

        // 3. Best possible penalty is 0 (assuming <= 4 empty stations)
        return ub;
    }

    std::vector<Move> generate_moves(const BoardState& state, int turn) {
        std::vector<Move> moves;
        const Card& card = scenario.turns[turn];
        bool future_transfers_exist = (remaining_transfers[turn + 1] > 0);

        if (card.type == CardType::FREE) {
            // Mark any empty station. Prioritize stations on incomplete routes.
            bool found_incomplete = false;
            for (int s = 0; s < map.total_stations; ++s) {
                if (!state.stations_marked.test(s)) {
                    // Check if it belongs to an incomplete route
                    bool in_incomplete_route = false;
                    for (const auto& r : map.routes) {
                        if (!(state.completed_routes_mask & (1U << r.id)) && r.mask.test(s)) {
                            in_incomplete_route = true;
                            break;
                        }
                    }
                    if (in_incomplete_route) {
                        found_incomplete = true;
                        Move m;
                        m.route_idx = -1;
                        m.marked_stations = {s};
                        moves.push_back(m);
                    }
                }
            }
            // If no empty stations on incomplete routes, allow any empty station
            if (!found_incomplete) {
                for (int s = 0; s < map.total_stations; ++s) {
                    if (!state.stations_marked.test(s)) {
                        Move m;
                        m.route_idx = -1;
                        m.marked_stations = {s};
                        moves.push_back(m);
                    }
                }
            }
            return moves;
        }

        // For NUMBER, SKIP, TRANSFER: must choose a route with available windows
        for (const auto& r : map.routes) {
            if (state.windows_used[r.id] >= r.max_windows) {
                continue;
            }

            if (card.type == CardType::TRANSFER) {
                // Find first empty station from Start Point
                int first_empty = -1;
                for (int sid : r.stations) {
                    if (!state.stations_marked.test(sid)) {
                        first_empty = sid;
                        break;
                    }
                }
                Move m;
                m.route_idx = r.id;
                if (first_empty != -1) {
                    m.is_transfer = true;
                    m.transfer_station = first_empty;
                    m.transfer_points = map.station_transfer_pts[first_empty];
                    m.marked_stations = {first_empty};
                }
                // (If route is already full, burns window with 0 marks/points)
                moves.push_back(m);
            }
            else if (card.type == CardType::NUMBER) {
                // Find first empty station
                size_t first_empty_idx = r.stations.size();
                for (size_t i = 0; i < r.stations.size(); ++i) {
                    if (!state.stations_marked.test(r.stations[i])) {
                        first_empty_idx = i;
                        break;
                    }
                }

                if (first_empty_idx == r.stations.size()) {
                    // Fully marked route, burn window
                    Move m;
                    m.route_idx = r.id;
                    moves.push_back(m);
                } else {
                    // Consecutive empty stations up to card.value, stop on marked or end
                    std::vector<int> candidates;
                    for (size_t i = first_empty_idx; i < r.stations.size() && (int)candidates.size() < card.value; ++i) {
                        if (state.stations_marked.test(r.stations[i])) {
                            break; // Stop! Hit an already marked station
                        }
                        candidates.push_back(r.stations[i]);
                    }

                    // 1. Full fill move
                    Move full_m;
                    full_m.route_idx = r.id;
                    full_m.marked_stations = candidates;
                    moves.push_back(full_m);

                    // 2. If short uses allowed & future transfers exist, optionally stop right before a transfer hub (degree >= 2)
                    if (scenario.settings.allow_short_use && future_transfers_exist && candidates.size() > 1) {
                        for (size_t len = 1; len < candidates.size(); ++len) {
                            int next_station = candidates[len];
                            if (map.station_degrees[next_station] >= 2) {
                                Move underfill_m;
                                underfill_m.route_idx = r.id;
                                underfill_m.marked_stations.assign(candidates.begin(), candidates.begin() + len);
                                moves.push_back(underfill_m);
                            }
                        }
                    }
                }
            }
            else if (card.type == CardType::SKIP) {
                // Find up to card.value empty stations skipping marked ones
                std::vector<int> candidates;
                for (size_t i = 0; i < r.stations.size() && (int)candidates.size() < card.value; ++i) {
                    if (!state.stations_marked.test(r.stations[i])) {
                        candidates.push_back(r.stations[i]);
                    }
                }

                if (candidates.empty()) {
                    Move m;
                    m.route_idx = r.id;
                    moves.push_back(m);
                } else {
                    Move full_m;
                    full_m.route_idx = r.id;
                    full_m.marked_stations = candidates;
                    moves.push_back(full_m);

                    if (scenario.settings.allow_short_use && future_transfers_exist && candidates.size() > 1) {
                        for (size_t len = 1; len < candidates.size(); ++len) {
                            int next_station = candidates[len];
                            if (map.station_degrees[next_station] >= 2) {
                                Move underfill_m;
                                underfill_m.route_idx = r.id;
                                underfill_m.marked_stations.assign(candidates.begin(), candidates.begin() + len);
                                moves.push_back(underfill_m);
                            }
                        }
                    }
                }
            }
        }

        return moves;
    }

    BoardState apply_move(const BoardState& state, int turn, Move& move) {
        BoardState next = state;
        int turn_1based = turn + 1;

        if (move.route_idx != -1) {
            next.windows_used[move.route_idx]++;
        }

        for (int sid : move.marked_stations) {
            next.stations_marked.set(sid);
        }

        if (move.is_transfer) {
            next.transfer_score += move.transfer_points;
        }

        // Check completion across all routes
        for (const auto& r : map.routes) {
            if (!(next.completed_routes_mask & (1U << r.id))) {
                if ((next.stations_marked & r.mask) == r.mask) {
                    next.completed_routes_mask |= (1U << r.id);
                    bool is_gold = (turn_1based <= scenario.opponent_finish_turns[r.id]);
                    int bonus = is_gold ? r.gold_bonus : r.silver_bonus;
                    next.route_score += bonus;

                    move.completed_routes.push_back(r.id);
                    move.completion_bonuses.push_back(bonus);
                    move.is_gold_bonus.push_back(is_gold);
                }
            }
        }

        return next;
    }

    void dfbnb(BoardState state, int turn, std::vector<Move>& current_path) {
        states_evaluated++;

        // Base case: end of turn sequence
        if (turn == (int)scenario.turns.size()) {
            int empty_stations = map.total_stations - (int)state.stations_marked.count();
            int penalty = get_empty_stations_penalty(empty_stations);
            int total = state.route_score + state.transfer_score + penalty;

            if (total > best_score) {
                best_score = total;
                best_playbook = current_path;
                best_solution_breakdown.route_score = state.route_score;
                best_solution_breakdown.transfer_score = state.transfer_score;
                best_solution_breakdown.empty_stations = empty_stations;
                best_solution_breakdown.penalty = penalty;
            }
            return;
        }

        // Upper Bound Pruning
        int ub = compute_upper_bound(state, turn);
        if (ub <= best_score) {
            return; // Prune!
        }

        // Transposition Table Check
        StateKey key = make_key(state, turn);
        int current_accumulated = state.route_score + state.transfer_score;
        auto it = transposition_table.find(key);
        if (it != transposition_table.end() && it->second >= current_accumulated) {
            return; // Prune duplicate or dominated state
        }
        transposition_table[key] = current_accumulated;

        // Generate legal moves
        auto moves = generate_moves(state, turn);

        // Move ordering: prioritize moves that complete routes or score transfers
        struct ScoredMove {
            Move move;
            BoardState next_state;
            int priority;
        };
        std::vector<ScoredMove> scored_moves;
        scored_moves.reserve(moves.size());

        for (auto& m : moves) {
            BoardState nxt = apply_move(state, turn, m);
            int priority = 0;
            // High priority: completed lines
            if (!m.completed_routes.empty()) {
                priority += 100 * (int)m.completed_routes.size();
                for (int b : m.completion_bonuses) priority += b * 10;
            }
            priority += m.transfer_points * 5;
            priority += (int)m.marked_stations.size();

            scored_moves.push_back({m, nxt, priority});
        }

        std::sort(scored_moves.begin(), scored_moves.end(), [](const ScoredMove& a, const ScoredMove& b) {
            return a.priority > b.priority;
        });

        // Recurse
        for (auto& sm : scored_moves) {
            current_path.push_back(sm.move);
            dfbnb(sm.next_state, turn + 1, current_path);
            current_path.pop_back();
        }
    }

    void run_beam_search(int beam_width) {
        struct BeamNode {
            BoardState state;
            std::vector<Move> path;
            int score_heuristic;
        };

        std::vector<BeamNode> beam;
        beam.push_back({BoardState{}, {}, 0});

        for (int t = 0; t < (int)scenario.turns.size(); ++t) {
            std::vector<BeamNode> next_beam;

            for (const auto& node : beam) {
                auto moves = generate_moves(node.state, t);
                for (auto& m : moves) {
                    BeamNode child;
                    child.state = apply_move(node.state, t, m);
                    child.path = node.path;
                    child.path.push_back(m);

                    int h = child.state.route_score * 3 +
                            child.state.transfer_score * 2 +
                            (int)child.state.stations_marked.count();
                    child.score_heuristic = h;
                    next_beam.push_back(child);
                }
            }

            if (next_beam.empty()) break;

            // Keep top beam_width
            if ((int)next_beam.size() > beam_width) {
                std::partial_sort(next_beam.begin(), next_beam.begin() + beam_width, next_beam.end(),
                    [](const BeamNode& a, const BeamNode& b) {
                        return a.score_heuristic > b.score_heuristic;
                    });
                next_beam.resize(beam_width);
            }
            beam = std::move(next_beam);
        }

        // Evaluate leaves of beam search
        for (const auto& node : beam) {
            int empty_stations = map.total_stations - (int)node.state.stations_marked.count();
            int penalty = get_empty_stations_penalty(empty_stations);
            int total = node.state.route_score + node.state.transfer_score + penalty;

            if (total > best_score) {
                best_score = total;
                best_playbook = node.path;
                best_solution_breakdown.route_score = node.state.route_score;
                best_solution_breakdown.transfer_score = node.state.transfer_score;
                best_solution_breakdown.empty_stations = empty_stations;
                best_solution_breakdown.penalty = penalty;
            }
        }
    }
};
