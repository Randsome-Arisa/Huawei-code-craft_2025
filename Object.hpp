#include "limit.h"

// 记录备份信息
struct Replica {
    int disk_id;    // 所在磁盘id
    // ? 是否用Block类而不是int存储每个单独的内存块更好？
    std::vector<int> units; // 存储单元位置数组
}

class Object {
    int id; // 对象ID
    int tag;    // 对象标签
    int size;   // 对象大小

    Replica replicas[REP_NUM]; // 3个备份的信息

    // int create_timestamp;   // 创建的时间片
    // TODO: 动态存储读取优先级
    // TODO: 添加磁头位置缓存以进行优化
};