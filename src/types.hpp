#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <bitset>
#include <unordered_map>
#include <cstdint>
#include <algorithm>

constexpr int MAX_STATIONS = 128;
constexpr int MAX_LINES = 32;

// 128-bit station bitmask using std::bitset
using StationMask = std::bitset<MAX_STATIONS>;

enum class CardType {
    NUMBER,
    SKIP,
    TRANSFER,
    FREE
};

struct Card {
    CardType type;
    int value; // Used for NUMBER and SKIP (e.g. 2, 3, 4, 5, 6)

    std::string to_string() const {
        switch (type) {
            case CardType::NUMBER:   return std::to_string(value);
            case CardType::SKIP:     return "S" + std::to_string(value);
            case CardType::TRANSFER: return "TRANSFER";
            case CardType::FREE:     return "FREE";
        }
        return "?";
    }
};

struct Route {
    int id;
    std::string name;
    int max_windows;
    int gold_bonus;
    int silver_bonus;
    std::vector<int> stations; // Station IDs in order from Start to End
    StationMask mask;          // Bitmask of all stations in this route
};

struct GameMap {
    std::vector<Route> routes;
    int total_stations = 0;
    std::vector<std::string> station_names;
    std::vector<int> station_degrees;       // How many routes pass through each station
    std::vector<int> station_transfer_pts;  // 2 * degree
};

struct ScenarioSettings {
    bool allow_short_use = true; // If false, forces using all available squares (no voluntary underfilling)
};

// Represents a step taken in the game
struct Move {
    int route_idx = -1;              // -1 for FREE card
    std::vector<int> marked_stations;
    bool is_transfer = false;
    int transfer_station = -1;
    int transfer_points = 0;
    
    // Line completion events triggered by this move
    std::vector<int> completed_routes;
    std::vector<int> completion_bonuses;
    std::vector<bool> is_gold_bonus;
};

// Represents a board state during search
struct BoardState {
    StationMask stations_marked;
    uint32_t completed_routes_mask = 0; // bitmask of completed routes
    uint8_t windows_used[MAX_LINES] = {0};
    int16_t transfer_score = 0;
    int16_t route_score = 0;

    bool operator==(const BoardState& other) const {
        if (stations_marked != other.stations_marked) return false;
        if (completed_routes_mask != other.completed_routes_mask) return false;
        if (transfer_score != other.transfer_score) return false;
        if (route_score != other.route_score) return false;
        for (int i = 0; i < MAX_LINES; ++i) {
            if (windows_used[i] != other.windows_used[i]) return false;
        }
        return true;
    }
};

// Penalty table for empty stations according to official rules
inline int get_empty_stations_penalty(int empty_count) {
    if (empty_count <= 4) return 0;
    if (empty_count == 5) return -1;
    if (empty_count == 6) return -2;
    if (empty_count == 7) return -3;
    if (empty_count <= 9) return -4;
    if (empty_count <= 11) return -5;
    if (empty_count <= 13) return -6;
    if (empty_count <= 15) return -7;
    if (empty_count <= 17) return -8;
    if (empty_count <= 19) return -9;
    return -10;
}
