#pragma once
#include<vector>
#include<string>
#include<memory>

struct Record
{
    int id;
    std::string name;
    int age;

    Record(int id,std::string name,int age):id(id),name(std::move(name)),age(age){}
};

class Table{
private:
    std::string Table_name;
    std::vector<std::unique_ptr<Record>> records;
    int next_id=1;
public:
    explicit Table(std::string name):Table_name(move(name)){}

    int insert_record(const std::string& name,int age);

    const Record* query_record(int id) const;  // 返回指针，nullptr表示无结果

    bool delete_record(int id);

    std::string get_name() const { return Table_name; }

    void print_all_records() const;
};