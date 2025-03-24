#incllude "Disk.hpp"

class DiskScheduler {
private:
    static int numDisks;
    std::vector<Disk> disks;

    // TODO: 选择最合适的磁盘作为写入磁盘
    int select_disk(int object_id, int tag) {
        int select_disk_id = -1;
        // 尽量选择与该对象同标签少的磁盘
        // 任何时间存储系统中空余的存储单元数占总存储单元数的至少 10%

        return selected_disk_id;
    }
public:
    DiskScheduler(int numDisks) {
        this->numDisks = numDisks;
        for (int i = 0; i < numDisks; ++i) {
            disks.emplace_back(i);
        }
    }

    // TODO: 调用分离空闲链表的删除动作
    void delete_object() {
    }

    // TODO: 调用分离空闲链表的写动作
    void do_object_write() {
    }
}