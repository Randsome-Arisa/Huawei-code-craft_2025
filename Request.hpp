enum class Status {
    PENDING,    // 还没开始读的
    READING     // 正在连续读的
}

class Request {
    int req_id; // 请求id
    int object_id;  // 被请求读取的对象id
    Status status;  // 状态

    int start_timestamp;    // 请求到达的时间戳
    int deadline_timestamp;  // 最晚完成时间戳，超过没分
    int processed_units = 0; // 已处理单元数
    double priority;    // 优先级
    int responsible_disk_id;    // 负责该请求的磁盘id，-1表示还没分配

    Request(int req_id, int object_id, int start_timestamp) {
        this->req_id = req_id;
        this->object_id = object_id;
        this->start_timestamp = start_timestamp;
        this->deadline_timestamp = start_timestamp + 105;
        this->status = Status::PENDING;
        this->responsible_disk_id = -1;
    }

    // TODO: 完善优先级算法
    void update_priority(int current_time) {
        float time_factor = 1.0f - (deadline - current_time)/100.0f;
        float delete_factor = is_marked_delete ? 2.0f : 1.0f;
        priority = delete_factor * (time_factor + processed_units/100.0f);
    }
};