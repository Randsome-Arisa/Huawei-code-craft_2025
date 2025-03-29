#include <unordered_map>
#include "SegregatedFreeLists.hpp"

class Disk {
public:
    int id;
    int size;
    int used_units;  // 已使用的空间大小
    int head_point;  // 磁头位置
    bool last_action_is_read;  // 上一次的动作是否是读
    // 维护一个 <tag, num> 字典，保存该磁盘上标签为 tag 的数据块数量，用于磁盘选择
    // ? 可以试试保存标签为 tag 的对象数量，看看哪个好
    std::unordered_map<int, int> tag_slot_num;
    // 维护一个分离空闲链表，用于写操作
    SegregatedFreeList sfl;

    Disk(int id, int size) {
        this->id = id;
        this->size = size;
        this->used_units = 0;
        this->head_point = 0;
        this->last_action_is_read = false;
    }
}