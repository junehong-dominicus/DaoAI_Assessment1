# DaoAI_Assessment1
DaoAI Assessment1 solution 1, 2, 3

# Inspection Region Manager

A C++ application for managing and querying 2D inspection regions stored in PostgreSQL.

## Features

- Load inspection region data from text files into PostgreSQL
- Query regions using JSON-based query language
- Support for complex queries with AND/OR operators
- Category and group-based filtering
- Proper region validation (entire group must be valid)

## Prerequisites

- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15+
- PostgreSQL 10+
- libpqxx library
- nlohmann/json library

## Installation

### Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install -y postgresql postgresql-contrib libpqxx-dev nlohmann-json3-dev cmake ninja-build
```

### macOS (using Homebrew)

```bash
brew install postgresql libpqxx nlohmann-json cmake ninja
```

### Database Setup

```bash
# Start PostgreSQL service
sudo service postgresql start

# Create database
sudo -u postgres createdb inspection_db

# Create user (optional, if needed)
sudo -u postgres psql -c "CREATE USER postgres WITH PASSWORD 'postgres';"
sudo -u postgres psql -c "GRANT ALL PRIVILEGES ON DATABASE inspection_db TO postgres;"
```

## Building

```bash
mkdir build
cd build
cmake ..
make
```

Or using Ninja:

```bash
mkdir build
cd build
cmake -G Ninja ..
ninja
```

## Usage

### Task 1: Data Loading

Load inspection region data from text files into the database:

```bash
./data_loader --data_directory=/path/to/data
```

The data directory should contain:
- `points.txt` - Coordinates (x y per line)
- `categories.txt` - Category IDs (one per line)
- `groups.txt` - Group IDs (one per line)

### Task 2 & 3: Querying Regions

Execute queries using JSON query files:

```bash
./query_engine --query=q1.json
```

Results are written to `output.txt` sorted by (y, x) coordinates.

## Query Format

### Basic Crop Query

```json
{
  "valid_region": {
    "p_min": { "x": 0, "y": 0 },
    "p_max": { "x": 400, "y": 300 }
  },
  "query": {
    "operator_crop": {
      "region": {
        "p_min": { "x": 200, "y": 200 },
        "p_max": { "x": 400, "y": 300 }
      },
      "category": 1,
      "one_of_groups": [0, 5],
      "proper": true
    }
  }
}
```

### Complex Query with AND/OR

```json
{
  "valid_region": {
    "p_min": { "x": 0, "y": 0 },
    "p_max": { "x": 1000, "y": 1000 }
  },
  "query": {
    "operator_and": [
      {
        "operator_crop": {
          "region": {
            "p_min": { "x": 200, "y": 200 },
            "p_max": { "x": 400, "y": 300 }
          }
        }
      },
      {
        "operator_or": [
          {
            "operator_crop": {
              "region": {
                "p_min": { "x": 100, "y": 100 },
                "p_max": { "x": 250, "y": 1000 }
              }
            }
          },
          {
            "operator_crop": {
              "region": {
                "p_min": { "x": 350, "y": 100 },
                "p_max": { "x": 500, "y": 1000 }
              },
              "proper": true
            }
          }
        ]
      }
    ]
  }
}
```

## Query Operators

### operator_crop

Selects points within a rectangular region with optional filters:

- `region` (required): Rectangular bounds with `p_min` and `p_max`
- `category` (optional): Filter by category ID
- `one_of_groups` (optional): Filter by list of group IDs
- `proper` (optional): If true, only include points whose entire group is within the valid region

### operator_and

Computes the intersection of multiple query results.

### operator_or

Computes the union of multiple query results.

## Database Schema

```sql
CREATE TABLE inspection_group (
    id BIGINT NOT NULL,
    PRIMARY KEY (id)
);

CREATE TABLE inspection_region (
    id BIGINT NOT NULL,
    group_id BIGINT,
    coord_x FLOAT,
    coord_y FLOAT,
    category INTEGER,
    PRIMARY KEY (id),
    FOREIGN KEY (group_id) REFERENCES inspection_group(id)
);
```

## Configuration

To modify the database connection string, edit the connection parameters in both `data_loader.cpp` and `main.cpp`:

```cpp
pqxx::connection conn("dbname=inspection_db user=postgres password=postgres host=localhost port=5432");
```

## Error Handling

Both programs include comprehensive error handling for:
- Missing or invalid command line arguments
- File I/O errors
- Database connection failures
- Invalid JSON format
- Data consistency issues

## Architecture

- **data_loader**: Reads text files and populates PostgreSQL tables
- **query_engine**: Parses JSON queries and executes them against the database
- **Modular design**: Separate functions for parsing, querying, and data processing
- **Set-based operations**: Uses C++ STL sets for efficient intersection/union operations

## Performance Considerations

- Indexes on `coord_x`, `coord_y`, `category`, and `group_id` columns can improve query performance
- Proper filtering reduces the number of database round-trips
- Set operations are performed in memory for efficiency

## License

This is a sample implementation for asseessment purposes.
