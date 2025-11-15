#include"Database.h"
#include<iostream>

int main(){
    Database db;

    db.create_table("student");
    db.create_table("teacher");
    db.create_table("student");
    db.print_all_tables();
    std::cout<<"------------------------\n";

    Table* student_table=db.get_table("student");
    if(student_table){
        int id1=student_table->insert_record("张三",20);
        int id2=student_table->insert_record("李四",21);
        std::cout<<"插入记录：ID="<<id1<<",ID="<<id2<<"\n";
        student_table->print_all_records();
    }
    std::cout<<"------------------------\n";

    const Record* record = student_table->query_record(1);
    if (record != nullptr) {
        std::cout << "查询ID=1的记录：\n";
        std::cout << "ID: " << record->id 
                << ", 姓名: " << record->name 
                << ", 年龄: " << record->age << "\n";
    }
    const Record* no_record = student_table->query_record(99);
    if (no_record == nullptr) {  // 无结果时指针为nullptr
        std::cout << "查询ID=99的记录：无结果\n";
    }
    std::cout<<"------------------------\n";

    if(student_table){
        bool delete_ok=student_table->delete_record(2);
        if(delete_ok){
            std::cout<<"删除ID=2的记录成功\n";
            student_table->print_all_records();
        }
    }
    std::cout<<"------------------------\n";

    db.drop_table("teacher");
    db.drop_table("class");
    db.print_all_tables();

    return 0;
}