#include "Table.h"
#include<iostream>

int Table::insert_record(const std::string& name,int age){
    records.emplace_back(std::make_unique<Record>(next_id,name,age));
    return next_id++;
}

const Record* Table::query_record(int id) const {
    for (const auto& record : records) {
        if (record->id == id) {
            return record.get();
        }
    }
    return nullptr;
}

bool Table::delete_record(int id){
    for(auto it=records.begin();it!=records.end();++it){
        if((*it)->id==id){
            records.erase(it);
            return true;
        }
    }
    return false;
}

void Table::print_all_records() const{
    std::cout<<"表 "<<Table_name<<" 所有记录：\n";
    for(const auto& record:records){
        std::cout<<"ID: "<<record->id
                 <<",姓名: "<<record->name
                 <<",年龄: "<<record->age<<"\n";
    }
    if(records.empty()){
        std::cout<<"（空表）\n";
    }
}