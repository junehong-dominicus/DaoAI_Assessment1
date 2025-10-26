#include <gtest/gtest.h>
#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>
#include <set>

// Include the newly created header file for the QueryEngine
#include "../src/query_engine.h"

using json = nlohmann::json;

class QueryEngineTest : public ::testing::Test {
protected:
    const std::string conn_string_ = "dbname=inspection_test_db user=postgres password=postgres host=localhost port=5432";
    pqxx::connection conn_{conn_string_};

    void SetUp() override {
        pqxx::work txn(conn_);
        // Clean up and create schema
        txn.exec("DROP TABLE IF EXISTS inspection_region CASCADE");
        txn.exec("DROP TABLE IF EXISTS inspection_group CASCADE");

        txn.exec(R"(
            CREATE TABLE inspection_group (
                id BIGINT NOT NULL,
                PRIMARY KEY (id)
            )
        )");

        txn.exec(R"(
            CREATE TABLE inspection_region (
                id BIGINT NOT NULL,
                group_id BIGINT,
                coord_x FLOAT,
                coord_y FLOAT,
                category INTEGER,
                PRIMARY KEY (id),
                FOREIGN KEY (group_id) REFERENCES inspection_group(id)
            )
        )");

        // Insert test data
        // Groups
        txn.exec("INSERT INTO inspection_group (id) VALUES (0), (1), (2)");

        // Regions
        // id, group_id, x, y, category
        txn.exec("INSERT INTO inspection_region VALUES (1, 0, 10, 10, 1)");   // In valid_region
        txn.exec("INSERT INTO inspection_region VALUES (2, 0, 20, 20, 2)");   // In valid_region
        txn.exec("INSERT INTO inspection_region VALUES (3, 1, 30, 30, 1)");   // In valid_region
        txn.exec("INSERT INTO inspection_region VALUES (4, 1, 150, 150, 2)"); // Outside valid_region
        txn.exec("INSERT INTO inspection_region VALUES (5, 2, 40, 40, 1)");   // In valid_region
        txn.exec("INSERT INTO inspection_region VALUES (6, 2, 50, 50, 1)");   // In valid_region

        txn.commit();
    }

    void TearDown() override {
        pqxx::work txn(conn_);
        txn.exec("DROP TABLE IF EXISTS inspection_region");
        txn.exec("DROP TABLE IF EXISTS inspection_group");
        txn.commit();
    }

    // Helper to extract IDs for easy comparison
    std::set<long long> getIds(const std::vector<Point>& points) {
        std::set<long long> ids;
        for (const auto& p : points) {
            ids.insert(p.id);
        }
        return ids;
    }
};

TEST_F(QueryEngineTest, BasicCrop) {
    QueryEngine engine(conn_string_);
    json query = R"(
    {
      "valid_region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } },
      "query": {
        "operator_crop": {
          "region": { "p_min": { "x": 15, "y": 15 }, "p_max": { "x": 35, "y": 35 } }
        }
      }
    }
    )"_json;

    auto results = engine.execute_query(query);
    auto result_ids = getIds(results);

    std::set<long long> expected_ids = {2, 3};
    ASSERT_EQ(result_ids, expected_ids);
}

TEST_F(QueryEngineTest, CropWithCategory) {
    QueryEngine engine(conn_string_);
    json query = R"(
    {
      "valid_region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } },
      "query": {
        "operator_crop": {
          "region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } },
          "category": 2
        }
      }
    }
    )"_json;

    auto results = engine.execute_query(query);
    auto result_ids = getIds(results);

    std::set<long long> expected_ids = {2};
    ASSERT_EQ(result_ids, expected_ids);
}

TEST_F(QueryEngineTest, CropWithGroups) {
    QueryEngine engine(conn_string_);
    json query = R"(
    {
      "valid_region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } },
      "query": {
        "operator_crop": {
          "region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } },
          "one_of_groups": [0, 1]
        }
      }
    }
    )"_json;

    auto results = engine.execute_query(query);
    auto result_ids = getIds(results);

    std::set<long long> expected_ids = {1, 2, 3};
    ASSERT_EQ(result_ids, expected_ids);
}

TEST_F(QueryEngineTest, ProperCrop) {
    QueryEngine engine(conn_string_);
    // valid_region is [0,0] to [100,100].
    // Group 0: points (1,2) are inside. It is a proper group.
    // Group 1: point 3 is inside, point 4 is outside. Not a proper group.
    // Group 2: points (5,6) are inside. It is a proper group.
    json query = R"(
    {
      "valid_region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } },
      "query": {
        "operator_crop": {
          "region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } },
          "proper": true
        }
      }
    }
    )"_json;

    auto results = engine.execute_query(query);
    auto result_ids = getIds(results);

    // Only points from groups 0 and 2 should be returned.
    std::set<long long> expected_ids = {1, 2, 5, 6};
    ASSERT_EQ(result_ids, expected_ids);
}

TEST_F(QueryEngineTest, OperatorAnd) {
    QueryEngine engine(conn_string_);
    json query = R"(
    {
      "valid_region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } },
      "query": {
        "operator_and": [
          { "operator_crop": { "region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 35, "y": 35 } } } },
          { "operator_crop": { "region": { "p_min": { "x": 15, "y": 15 }, "p_max": { "x": 55, "y": 55 } } } }
        ]
      }
    }
    )"_json;
    // First crop: {1, 2, 3}
    // Second crop: {2, 3, 5, 6}
    // Intersection: {2, 3}

    auto results = engine.execute_query(query);
    auto result_ids = getIds(results);

    std::set<long long> expected_ids = {2, 3};
    ASSERT_EQ(result_ids, expected_ids);
}

TEST_F(QueryEngineTest, OperatorOr) {
    QueryEngine engine(conn_string_);
    json query = R"(
    {
      "valid_region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } },
      "query": {
        "operator_or": [
          { "operator_crop": { "region": { "p_min": { "x": 5, "y": 5 }, "p_max": { "x": 15, "y": 15 } } } },
          { "operator_crop": { "region": { "p_min": { "x": 45, "y": 45 }, "p_max": { "x": 55, "y": 55 } } } }
        ]
      }
    }
    )"_json;
    // First crop: {1}
    // Second crop: {6}
    // Union: {1, 6}

    auto results = engine.execute_query(query);
    auto result_ids = getIds(results);

    std::set<long long> expected_ids = {1, 6};
    ASSERT_EQ(result_ids, expected_ids);
}

TEST_F(QueryEngineTest, ComplexNestedQuery) {
    QueryEngine engine(conn_string_);
    json query = R"(
    {
      "valid_region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } },
      "query": {
        "operator_and": [
          { "operator_crop": { "region": { "p_min": { "x": 0, "y": 0 }, "p_max": { "x": 100, "y": 100 } }, "one_of_groups": [0, 2] } },
          {
            "operator_or": [
              { "operator_crop": { "region": { "p_min": { "x": 15, "y": 15 }, "p_max": { "x": 25, "y": 25 } } } },
              { "operator_crop": { "region": { "p_min": { "x": 35, "y": 35 }, "p_max": { "x": 45, "y": 45 } } } }
            ]
          }
        ]
      }
    }
    )"_json;
    // First AND operand (groups 0, 2): {1, 2, 5, 6}
    // Second AND operand (OR):
    //   OR-1 (crop 15-25): {2}
    //   OR-2 (crop 35-45): {5}
    //   Union of ORs: {2, 5}
    // Intersection of ANDs: {2, 5}

    auto results = engine.execute_query(query);
    auto result_ids = getIds(results);

    std::set<long long> expected_ids = {2, 5};
    ASSERT_EQ(result_ids, expected_ids);
}
