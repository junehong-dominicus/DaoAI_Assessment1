#include <iostream>
#include <fstream>
#include <set>
#include <algorithm>

#include "query_engine.h"

bool Point::operator<(const Point& other) const {
    if (y != other.y) return y < other.y;
    return x < other.x;
}

bool Rectangle::contains(double x, double y) const {
    return x >= x_min && x <= x_max && y >= y_min && y <= y_max;
}

std::set<long long> QueryEngine::get_valid_point_ids(pqxx::work& txn) {
        std::set<long long> valid_ids;

        pqxx::result res = txn.exec_params(
            "SELECT id FROM inspection_region "
            "WHERE coord_x >= $1 AND coord_x <= $2 "
            "AND coord_y >= $3 AND coord_y <= $4",
            valid_region_.x_min, valid_region_.x_max,
            valid_region_.y_min, valid_region_.y_max
        );
        
        for (const auto& row : res) {
            valid_ids.insert(row[0].as<long long>());
        }
        
        return valid_ids;
    }
std::set<long long> QueryEngine::get_proper_groups(pqxx::work& txn) {
        std::set<long long> proper_groups;

        // Find groups where ALL points are valid
        pqxx::result res = txn.exec_params(
            "SELECT group_id FROM inspection_region "
            "GROUP BY group_id "
            "HAVING COUNT(*) = SUM(CASE WHEN coord_x >= $1 AND coord_x <= $2 "
            "AND coord_y >= $3 AND coord_y <= $4 THEN 1 ELSE 0 END)",
            valid_region_.x_min, valid_region_.x_max,
            valid_region_.y_min, valid_region_.y_max
        );
        
        for (const auto& row : res) {
            proper_groups.insert(row[0].as<long long>());
        }
        
        return proper_groups;
    }
std::set<long long> QueryEngine::process_crop(pqxx::work& txn, const json& crop_op) {
        std::set<long long> result_ids;
        
        // Parse rectangle
        Rectangle crop_region;
        crop_region.x_min = crop_op["region"]["p_min"]["x"].get<double>();
        crop_region.y_min = crop_op["region"]["p_min"]["y"].get<double>();
        crop_region.x_max = crop_op["region"]["p_max"]["x"].get<double>();
        crop_region.y_max = crop_op["region"]["p_max"]["y"].get<double>();
        
        // Build query
        std::ostringstream query;
        query << "SELECT id, group_id FROM inspection_region WHERE "
              << "coord_x >= " << crop_region.x_min << " AND coord_x <= " << crop_region.x_max << " AND "
              << "coord_y >= " << crop_region.y_min << " AND coord_y <= " << crop_region.y_max;
        
        // Add category filter
        if (crop_op.contains("category")) {
            int category = crop_op["category"].get<int>();
            query << " AND category = " << category;
        }
        
        // Add group filter
        if (crop_op.contains("one_of_groups")) {
            query << " AND group_id IN (";
            auto groups = crop_op["one_of_groups"];
            for (size_t i = 0; i < groups.size(); ++i) {
                if (i > 0) query << ", ";
                query << groups[i].get<int>();
            }
            query << ")";
        }
        
        pqxx::result res = txn.exec(query.str());
        
        // Collect points
        std::set<long long> candidate_ids;
        std::set<int> candidate_groups;
        
        for (const auto& row : res) {
            long long id = row[0].as<long long>();
            int group_id = row[1].as<int>();
            candidate_ids.insert(id);
            candidate_groups.insert(group_id);
        }
        
        // Handle proper filter
        if (crop_op.contains("proper") && crop_op["proper"].get<bool>()) {
            std::set<long long> proper_groups = get_proper_groups(txn);
            
            // Re-query to filter by proper groups
            for (const auto& row : res) {
                long long id = row[0].as<long long>();
                int group_id = row[1].as<int>();
                
                if (proper_groups.count(group_id) > 0) {
                    // Verify entire group is in crop region
                    std::ostringstream group_query;
                    group_query << "SELECT COUNT(*) FROM inspection_region WHERE group_id = " << group_id;
                    pqxx::result group_count = txn.exec(group_query.str());
                    int total_in_group = group_count[0][0].as<int>();
                    
                    group_query.str("");
                    group_query << "SELECT COUNT(*) FROM inspection_region WHERE group_id = " << group_id
                               << " AND coord_x >= " << crop_region.x_min << " AND coord_x <= " << crop_region.x_max
                               << " AND coord_y >= " << crop_region.y_min << " AND coord_y <= " << crop_region.y_max;
                    pqxx::result crop_count = txn.exec(group_query.str());
                    int in_crop = crop_count[0][0].as<int>();
                    
                    if (total_in_group == in_crop) {
                        result_ids.insert(id);
                    }
                }
            }
        } else {
            result_ids = candidate_ids;
        }
        
        // Filter by valid region
        std::set<long long> valid_ids = get_valid_point_ids(txn);
        std::set<long long> final_result;
        std::set_intersection(
            result_ids.begin(), result_ids.end(),
            valid_ids.begin(), valid_ids.end(),
            std::inserter(final_result, final_result.begin())
        );
        
        return final_result;
    }
std::set<long long> QueryEngine::process_query(pqxx::work& txn, const json& query_obj) {
        if (query_obj.contains("operator_crop")) {
            return process_crop(txn, query_obj["operator_crop"]);
        }
        // else if (query_obj.contains("operator_and")) {
        //     std::set<long long> result;
        //     bool first = true;
            
        //     for (const auto& operand : query_obj["operator_and"]) {
        //         std::set<long long> operand_result = process_query(txn, operand);
                
        //         if (first) {
        //             result = operand_result;
        //             first = false;
        //         } else {
        //             std::set<long long> intersection;
        //             std::set_intersection(
        //                 result.begin(), result.end(),
        //                 operand_result.begin(), operand_result.end(),
        //                 std::inserter(intersection, intersection.begin())
        //             );
        //             result = intersection;
        //         }
        //     }
            
        //     return result;
        // }
        // else if (query_obj.contains("operator_or")) {
        //     std::set<long long> result;
            
        //     for (const auto& operand : query_obj["operator_or"]) {
        //         std::set<long long> operand_result = process_query(txn, operand);
        //         result.insert(operand_result.begin(), operand_result.end());
        //     }
            
        //     return result;
        // }
        
        return std::set<long long>();
    }
QueryEngine::QueryEngine(const std::string& connection_string)
        : conn_(connection_string) {
    }

std::vector<Point> QueryEngine::execute_query(const json& query_json) {
        // Parse valid region
        valid_region_.x_min = query_json["valid_region"]["p_min"]["x"].get<double>();
        valid_region_.y_min = query_json["valid_region"]["p_min"]["y"].get<double>();
        valid_region_.x_max = query_json["valid_region"]["p_max"]["x"].get<double>();
        valid_region_.y_max = query_json["valid_region"]["p_max"]["y"].get<double>();
        
        pqxx::work txn(conn_);
        // Process query
        std::set<long long> result_ids = process_query(txn, query_json["query"]);
        
        // Fetch full point data
        std::vector<Point> points;
        
        for (long long id : result_ids) {
            pqxx::result res = txn.exec_params(
                "SELECT id, coord_x, coord_y, category, group_id FROM inspection_region WHERE id = $1",
                id
            );
            
            if (!res.empty()) {
                Point p;
                p.id = res[0][0].as<long long>();
                p.x = res[0][1].as<double>();
                p.y = res[0][2].as<double>();
                p.category = res[0][3].as<int>();
                p.group_id = res[0][4].as<int>();
                points.push_back(p);
            }
        }
        
        txn.commit();

        // Sort by (y, x)
        std::sort(points.begin(), points.end());
        
        return points;
    }
