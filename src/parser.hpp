#pragma once
#include "types.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>

struct Scenario {
    GameMap map;
    std::vector<Card> turns;
    std::vector<int> opponent_finish_turns; // Index aligned with map.routes, 1-based turn (or 999999 if none)
    ScenarioSettings settings;
};

inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

inline std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream token_stream(str);
    while (std::getline(token_stream, token, delimiter)) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

inline Card parse_card_token(const std::string& token) {
    std::string upper = token;
    for (auto& c : upper) c = toupper(c);

    if (upper == "T" || upper == "TRANSFER" || upper == "X") {
        return Card{CardType::TRANSFER, 0};
    }
    if (upper == "FREE" || upper == "F") {
        return Card{CardType::FREE, 0};
    }
    if (upper.rfind("S", 0) == 0 || upper.rfind("SKIP", 0) == 0) {
        int val = 0;
        if (upper.rfind("SKIP", 0) == 0) {
            val = std::stoi(upper.substr(4));
        } else {
            val = std::stoi(upper.substr(1));
        }
        return Card{CardType::SKIP, val};
    }
    // Number card
    int val = std::stoi(upper);
    return Card{CardType::NUMBER, val};
}

inline Scenario parse_scenario_stream(std::istream& is) {
    Scenario scenario;
    std::string line;
    std::string current_section = "";

    std::map<std::string, int> station_name_to_id;
    std::map<std::string, int> route_name_to_idx;
    std::map<std::string, int> opp_finish_map;

    struct RawRoute {
        std::string name;
        int windows;
        int gold;
        int silver;
        std::vector<std::string> station_names;
    };
    std::vector<RawRoute> raw_routes;

    while (std::getline(is, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || (line.size() >= 2 && line.substr(0, 2) == "//")) {
            continue;
        }

        if (line[0] == '[' && line.back() == ']') {
            current_section = line.substr(1, line.size() - 2);
            for (auto& c : current_section) c = toupper(c);
            continue;
        }

        if (current_section == "MAP") {
            auto parts = split(line, '|');
            if (parts.size() >= 5) {
                RawRoute rr;
                rr.name = parts[0];
                rr.windows = std::stoi(parts[1]);
                rr.gold = std::stoi(parts[2]);
                rr.silver = std::stoi(parts[3]);

                std::istringstream station_stream(parts[4]);
                std::string st_name;
                while (station_stream >> st_name) {
                    rr.station_names.push_back(st_name);
                }
                raw_routes.push_back(rr);
            }
        } else if (current_section == "OPPONENTS") {
            auto parts = split(line, ':');
            if (parts.size() >= 2) {
                std::string r_name = parts[0];
                int turn_num = std::stoi(parts[1]);
                opp_finish_map[r_name] = turn_num;
            }
        } else if (current_section == "SETTINGS") {
            auto parts = split(line, ':');
            if (parts.size() < 2) parts = split(line, '=');
            if (parts.size() >= 2) {
                std::string key = parts[0];
                for (auto& c : key) c = tolower(c);
                std::string val = parts[1];
                for (auto& c : val) c = tolower(c);

                if (key == "allow_short_use" || key == "allow_underfill") {
                    scenario.settings.allow_short_use = (val == "true" || val == "1" || val == "yes");
                } else if (key == "disable_short_use" || key == "strict_fill" || key == "force_full_fill") {
                    scenario.settings.allow_short_use = !(val == "true" || val == "1" || val == "yes");
                }
            }
        } else if (current_section == "TURNS") {
            std::istringstream turn_stream(line);
            std::string token;
            while (turn_stream >> token) {
                scenario.turns.push_back(parse_card_token(token));
            }
        }
    }

    // Build unique stations
    for (const auto& rr : raw_routes) {
        for (const auto& sname : rr.station_names) {
            if (station_name_to_id.find(sname) == station_name_to_id.end()) {
                int new_id = (int)scenario.map.station_names.size();
                station_name_to_id[sname] = new_id;
                scenario.map.station_names.push_back(sname);
            }
        }
    }
    scenario.map.total_stations = (int)scenario.map.station_names.size();
    scenario.map.station_degrees.assign(scenario.map.total_stations, 0);

    // Build routes
    for (size_t i = 0; i < raw_routes.size(); ++i) {
        const auto& rr = raw_routes[i];
        Route route;
        route.id = (int)i;
        route.name = rr.name;
        route.max_windows = rr.windows;
        route.gold_bonus = rr.gold;
        route.silver_bonus = rr.silver;

        for (const auto& sname : rr.station_names) {
            int sid = station_name_to_id[sname];
            route.stations.push_back(sid);
            route.mask.set(sid);
            scenario.map.station_degrees[sid]++;
        }
        route_name_to_idx[route.name] = (int)i;
        scenario.map.routes.push_back(route);
    }

    // Transfer points = 2 * degree
    scenario.map.station_transfer_pts.resize(scenario.map.total_stations);
    for (int s = 0; s < scenario.map.total_stations; ++s) {
        scenario.map.station_transfer_pts[s] = 2 * scenario.map.station_degrees[s];
    }

    // Opponent finish turns aligned to route IDs
    scenario.opponent_finish_turns.assign(scenario.map.routes.size(), 999999);
    for (const auto& [rname, turn_val] : opp_finish_map) {
        if (route_name_to_idx.find(rname) != route_name_to_idx.end()) {
            scenario.opponent_finish_turns[route_name_to_idx[rname]] = turn_val;
        }
    }

    return scenario;
}

inline Scenario parse_scenario_string(const std::string& text) {
    std::istringstream iss(text);
    return parse_scenario_stream(iss);
}

inline Scenario load_scenario(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open scenario file: " + filepath);
    }
    return parse_scenario_stream(file);
}
