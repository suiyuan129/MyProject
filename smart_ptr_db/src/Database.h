#pragma once
#include<unordered_map>
#include<string>
#include<memory>
#include "Table.h"

class Database{
private:
    std::unordered_map<std::string,std::unique_ptr<Table>> tables;
public:
    bool create_table(const std::string& table_name);

    bool drop_table(const std::string& table_name);

    Table* get_table(const std::string& table_name);

    void print_all_tables() const;
};