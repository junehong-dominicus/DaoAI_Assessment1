#ifndef QUERY_ENGINE_H
#define QUERY_ENGINE_H

#include <string>
#include <vector>
#include <set>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct Point {
    long long id;
    double x, y;
    int category;
    int group_id;

    bool operator<(const Point& other) const;
};

struct Rectangle {
    double x_min, y_min, x_max, y_max;

    bool contains(double x, double y) const;
};

class QueryEngine {
public:
    QueryEngine(const std::string& connection_string);
    std::vector<Point> execute_query(const json& query_json);

private:
    pqxx::connection conn_;
    Rectangle valid_region_;

    std::set<long long> get_valid_point_ids(pqxx::work& txn);
    std::set<long long> get_proper_groups(pqxx::work& txn);
    std::set<long long> process_crop(pqxx::work& txn, const json& crop_op);
    std::set<long long> process_query(pqxx::work& txn, const json& query_obj);
};

#endif // QUERY_ENGINE_H