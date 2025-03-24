#include <unordered_map>
#include "SegregatedFreeList.hpp"

class Disk {
public:
    int id;
    int size;
    int point;  // 磁头位置
    // 维护一个 <tag, num> 字典，保存该磁盘上标签为 tag 的数据块数量，用于磁盘选择
    // ? 可以试试保存标签为 tag 的对象数量，看看哪个好
    unordered_map<int, int> tag_block_num;
    // 维护一个分离空闲链表，用于写操作
    SegregatedFreeList sfl;

    Disk(int id, int size) {
        this->id = id;
        this->size = size;
        this->point = 0;
    }
}