#include "Database.h"
#include<iostream>

bool Database::create_table(const std::string& table_name){
    if(tables.find(table_name)!=tables.end()){
        std::cout<<"错误：表 "<<table_name<<" 已存在！\n";
        return false;
    }
    tables.emplace(table_name,std::make_unique<Table>(table_name));
    std::cout<<"成功创建表："<<table_name<<"\n";
    return true;
}

bool Database::drop_table(const std::string& table_name){
    auto it=tables.find(table_name);
    if(it==tables.end()){
        std::cout<<"错误：表 "<<table_name<<" 不存在！\n";
        return false;
    }
    tables.erase(it);
    std::cout<<"成功删除表："<<table_name<<"\n";
    return true;
}

Table* Database::get_table(const std::string& table_name){
    auto it=tables.find(table_name);
    return it!=tables.end()? it->second.get():nullptr;
}

void Database::print_all_tables() const{
    std::cout<<"数据库中所以表：\n";
    if(tables.empty()){
        std::cout<<"（无表）\n";
        return;
    }
    for(const auto& [name,_]:tables){
        std::cout<<"- "<<name<<"\n";
    }
}