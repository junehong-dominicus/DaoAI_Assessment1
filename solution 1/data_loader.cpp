#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <filesystem>
#include <gflags/gflags.h>
#include <pqxx/pqxx>

namespace fs = std::filesystem;

struct Point {
    double x, y;
};

struct RegionData {
    Point coord;
    int category;
    int group_id;
};

std::vector<Point> read_points(const std::string& filepath) {
    std::vector<Point> points;
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        Point p;
        if (iss >> p.x >> p.y) {
            points.push_back(p);
        }
    }
    
    return points;
}

std::vector<int> read_integers(const std::string& filepath) {
    std::vector<int> values;
    std::ifstream file(filepath);
    
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        double val;
        if (iss >> val) {
            values.push_back(static_cast<int>(val));
        }
    }
    
    return values;
}

void create_schema(pqxx::connection& conn) {
    pqxx::work txn(conn);
    
    // Create tables
    txn.exec(R"(
        CREATE TABLE IF NOT EXISTS inspection_group (
            id BIGINT NOT NULL,
            PRIMARY KEY (id)
        )
    )");
    
    txn.exec(R"(
        CREATE TABLE IF NOT EXISTS inspection_region (
            id BIGINT NOT NULL,
            group_id BIGINT,
            PRIMARY KEY (id)
        )
    )");
    
    // Add columns if they don't exist
    txn.exec("ALTER TABLE inspection_region ADD COLUMN IF NOT EXISTS coord_x FLOAT");
    txn.exec("ALTER TABLE inspection_region ADD COLUMN IF NOT EXISTS coord_y FLOAT");
    txn.exec("ALTER TABLE inspection_region ADD COLUMN IF NOT EXISTS category INTEGER");
    
    // Add foreign key if it doesn't exist
    try {
        txn.exec(R"(
            DO $$ 
            BEGIN
                IF NOT EXISTS (
                    SELECT 1 FROM pg_constraint 
                    WHERE conname = 'fk_inspection_region_group'
                ) THEN
                    ALTER TABLE inspection_region 
                    ADD CONSTRAINT fk_inspection_region_group 
                    FOREIGN KEY (group_id) REFERENCES inspection_group(id);
                END IF;
            END $$;
        )");
    } catch (const std::exception& e) {
        std::cerr << "Warning: Could not add foreign key constraint: " << e.what() << std::endl;
    }
    
    txn.commit();
    std::cout << "Schema created successfully." << std::endl;
}

void load_data(pqxx::connection& conn, const std::vector<RegionData>& regions) {
    pqxx::work txn(conn);
    
    // Clear existing data
    txn.exec("DELETE FROM inspection_region");
    txn.exec("DELETE FROM inspection_group");
    
    // Collect unique groups
    std::set<int> unique_groups;
    for (const auto& region : regions) {
        unique_groups.insert(region.group_id);
    }
    
    // Insert groups
    for (int group_id : unique_groups) {
        txn.exec_params(
            "INSERT INTO inspection_group (id) VALUES ($1) ON CONFLICT (id) DO NOTHING",
            group_id
        );
    }
    
    // Insert regions
    for (size_t i = 0; i < regions.size(); ++i) {
        const auto& region = regions[i];
        txn.exec_params(
            "INSERT INTO inspection_region (id, group_id, coord_x, coord_y, category) "
            "VALUES ($1, $2, $3, $4, $5)",
            static_cast<long long>(i),
            region.group_id,
            region.coord.x,
            region.coord.y,
            region.category
        );
    }
    
    txn.commit();
    std::cout << "Loaded " << regions.size() << " regions into database." << std::endl;
}

// --- Command-line Flag Definitions ---
DEFINE_string(data_directory, "", "Path to the directory containing data files (points.txt, categories.txt, groups.txt).");

int main(int argc, char* argv[]) {
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    const std::filesystem::path data_dir(FLAGS_data_directory);

    // Validate required --data_directory flag
    if (FLAGS_data_directory.empty()) {
        std::cerr << "Error: --data_directory is a required argument." << std::endl;
        return 1;
    }

    std::cout << "Data directory: " << data_dir << std::endl;

    try {
        // Read data files
        fs::path points_file = fs::path(data_dir) / "points.txt";
        fs::path categories_file = fs::path(data_dir) / "categories.txt";
        fs::path groups_file = fs::path(data_dir) / "groups.txt";
        
        std::cout << "Reading points from: " << points_file << std::endl;
        auto points = read_points(points_file.string());
        
        std::cout << "Reading categories from: " << categories_file << std::endl;
        auto categories = read_integers(categories_file.string());
        
        std::cout << "Reading groups from: " << groups_file << std::endl;
        auto groups = read_integers(groups_file.string());
        
        // Validate data consistency
        if (points.size() != categories.size() || points.size() != groups.size()) {
            throw std::runtime_error("Data file sizes don't match!");
        }
        
        std::cout << "Read " << points.size() << " regions." << std::endl;
        
        // Combine data
        std::vector<RegionData> regions;
        for (size_t i = 0; i < points.size(); ++i) {
            regions.push_back({points[i], categories[i], groups[i]});
        }
        
        // Connect to PostgreSQL
        // Modify connection string as needed
        pqxx::connection conn("dbname=inspection_db user=postgres password=postgres host=localhost port=5432");
        
        if (!conn.is_open()) {
            throw std::runtime_error("Cannot connect to database");
        }
        
        std::cout << "Connected to database: " << conn.dbname() << std::endl;
        
        // Create schema
        create_schema(conn);
        
        // Load data
        load_data(conn, regions);
        
        std::cout << "Data loading completed successfully!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
