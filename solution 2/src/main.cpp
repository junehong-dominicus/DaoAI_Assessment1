#include <iostream>
#include <fstream>
#include <filesystem>
#include <gflags/gflags.h>
#include <nlohmann/json.hpp>

#include "query_engine.h"

using json = nlohmann::json;

// --- Command-line Flag Definitions ---
DEFINE_string(query, "", "JSON query file.");

int main(int argc, char* argv[]) {
    try {
        gflags::ParseCommandLineFlags(&argc, &argv, true);
        const std::filesystem::path query_file(FLAGS_query);

        // Validate required --query flag
        if (FLAGS_query.empty()) {
            std::cerr << "Error: --query is a required argument." << std::endl;
            return 1;
        }

        // Read JSON query
        std::ifstream file(query_file);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open query file: " + query_file.string());
        }

        json query_json;
        file >> query_json;

        // Execute query
        QueryEngine engine("dbname=inspection_db user=postgres password=postgres host=localhost port=5432");
        std::vector<Point> results = engine.execute_query(query_json);

        // Write output
        std::string output_file = "output.txt";
        std::ofstream out(output_file);

        for (const auto& point : results) {
            out << point.x << " " << point.y << std::endl;
        }

        std::cout << "Query completed. Found " << results.size() << " points." << std::endl;
        std::cout << "Results written to: " << output_file << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}