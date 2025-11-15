# smart_ptr_db: 基于智能指针的简单内存数据库

## 项目概述
`smart_ptr_db` 是一个用 C++17 实现的轻量级内存数据库，核心特点是通过 **智能指针（`std::unique_ptr`）** 自动管理数据库资源（表和记录），彻底避免手动内存管理导致的泄漏问题。项目专注于基础数据库功能的实现，适合学习智能指针的实际应用和数据库底层资源管理逻辑。

## 目录结构
```
smart_ptr_db/
├── src/                  # 源码目录
│   ├── main.cpp          # 主函数（测试数据库功能）
│   ├── Database.h        # 数据库类声明（管理表集合）
│   ├── Database.cpp      # 数据库类实现（创建/删除表等）
│   ├── Table.h           # 表类声明（管理记录集合）
│   └── Table.cpp         # 表类实现（插入/查询/删除记录等）
├── build/                # 编译输出目录（自动生成，存放可执行文件）
└── README.md             # 项目说明文档（本文档）
```

## 核心功能
| 模块       | 功能描述                                                                 | 核心函数                          |
|------------|--------------------------------------------------------------------------|-----------------------------------|
| 数据库     | 管理多个表的生命周期，支持创建、删除和查看表                              | `create_table`、`drop_table`、`print_all_tables` |
| 表         | 管理单表内的记录，支持插入、查询、删除和查看记录                          | `insert_record`、`query_record`、`delete_record`、`print_all_records` |

## 技术亮点
1. 智能指针自动内存管理
   - 用 `std::unique_ptr<Table>` 管理表资源：数据库（`Database`）独占所有表的所有权，删除表时无需手动释放，`unique_ptr` 会自动调用析构函数。
   - 用 `std::unique_ptr<Record>` 管理记录资源：表（`Table`）独占所有记录的所有权，删除记录时只需从容器中移除 `unique_ptr`，内存自动释放。

2. C++17 特性深度应用
   - `std::optional`：查询记录时返回 `optional<Record&>`，优雅处理“记录不存在”的场景（避免返回空指针导致崩溃）。
   - 结构化绑定：遍历表和记录时使用 `for (const auto& [name, table] : tables)` 简化代码。
   - 列表初始化与移动语义：通过 `emplace_back` 直接构造 `unique_ptr`，减少内存拷贝。

## 环境依赖
- 操作系统：Linux（推荐 Ubuntu 20.04+）
- 编译器：GCC 9.0+（支持 C++17，通过 `g++ --version` 确认版本）
- 开发工具：VS Code（远程连接 Linux 环境，已配置 `tasks.json` 和 `launch.json`）

## 快速上手

### 1. 配置项目到 MyProject 环境
确保 `MyProject/.vscode/tasks.json` 和 `launch.json` 的 `options` 数组中添加项目名，步骤：
```json
// 在 tasks.json 和 launch.json 中找到 "inputs" -> "options"，添加如下：
"options": ["proj1", "proj2", "smart_ptr_db"]  // 新增 "smart_ptr_db"
```

### 2. 编译项目
1. 用 VS Code 打开 `MyProject` 根目录（远程连接状态）；
2. 按 `Ctrl+Shift+B`，在弹出的项目列表中选择 `smart_ptr_db`；
3. 编译成功后，`smart_ptr_db/build/` 目录会生成可执行文件 `smart_ptr_db`。

### 3. 调试项目
1. 在源码中设置断点（例如 `main.cpp` 中插入记录的位置）；
2. 按 `F5`，在弹出的项目列表中选择 `smart_ptr_db`；
3. 调试面板会显示变量状态（如 `records` 智能指针数组中的记录数据），支持单步执行、查看内存等操作。

### 4. 直接运行
编译成功后，在终端执行以下命令查看运行结果：
```bash
# 进入编译输出目录
cd MyProject/smart_ptr_db/build

# 运行程序
./smart_ptr_db
```

## 示例输出
`main.cpp` 包含一套测试流程，运行后典型输出如下：
```
成功创建表：student
成功创建表：teacher
错误：表 student 已存在！  # 测试重复创建表
数据库中所有表：
- student
- teacher
------------------------
插入记录：ID=1、ID=2  # 向 student 表插入2条记录
表 student 所有记录：
ID: 1, 姓名: 张三, 年龄: 20
ID: 2, 姓名: 李四, 年龄: 21
------------------------
查询ID=1的记录：  # 测试查询存在的记录
ID: 1, 姓名: 张三, 年龄: 20
查询ID=99的记录：无结果  # 测试查询不存在的记录
------------------------
删除ID=2的记录成功  # 测试删除记录
表 student 所有记录：
ID: 1, 姓名: 张三, 年龄: 20  # 仅剩ID=1的记录
------------------------
成功删除表：teacher  # 测试删除表
错误：表 class 不存在！  # 测试删除不存在的表
数据库中所有表：
- student  # 仅剩 student 表
```

## 扩展方向
1. 支持更多数据类型：目前记录仅包含 `int`（ID/年龄）和 `std::string`（姓名），可扩展为支持 `double`（成绩）、`bool`（是否在校）等。
2. 记录修改功能：新增 `update_record` 函数，允许根据 ID 更新记录的字段值（如修改学生年龄）。
3. 持久化存储：将内存中的表和记录序列化到文件（如 JSON/CSV），程序重启后从文件加载数据（需依赖 `nlohmann/json` 等库）。
4. 多线程安全：用 `std::shared_ptr` 配合 `std::mutex` 实现多线程对数据库的并发访问（避免资源竞争）。

## 注意事项
- 智能指针不可手动调用 `delete`：`unique_ptr` 内部已管理内存，手动 `delete` 会导致二次释放（程序崩溃）。
- 编译器标准必须为 C++17：若修改 `tasks.json` 中的 `-std` 参数（如改为 `c++14`），`std::optional` 等特性会报错。
- 新增源码文件需放在 `src` 目录：编译配置会自动匹配 `src/*.cpp`，无需修改 `tasks.json`。

## 许可证
本项目为学习用途，开源可复用，无需保留版权声明。