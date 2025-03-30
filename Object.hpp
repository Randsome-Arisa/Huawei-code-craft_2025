#include <vector>

#include "limit.h"

// 记录备份信息
struct Replica {
    int disk_id;    // 所在磁盘id
    // ? 是否用Block类而不是int存储每个单独的内存块更好？
    std::vector<int> units; // 存储单元位置数组
};

class Object {
public:
    int id; // 对象ID
    int size;   // 对象大小
    int tag;    // 对象标签
    bool is_deleted = false;  // 是否被删除

    Replica replicas[REP_NUM]; // 3个备份的信息

<<<<<<< HEAD
    Object() {
        this->id = -1;
        is_deleted = true;
    }

    Object(int id, int size, int tag) : id(id), size(size), tag(tag) {
        for (int i = 0; i < REP_NUM; ++i) {
            replicas[i].units.clear();
            replicas[i].units.reserve(size + 1);
        }
        is_deleted = false;
    }
=======
    Object(int id, int tag, int size) : id(id), tag(tag), size(size) {}
>>>>>>> c44131c43bc27aee550194525e48326be8fbaf08
};