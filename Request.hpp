enum class Status {
    PENDING,    // 还没开始读的
    READING     // 正在连续读的
};

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
    /*
     * 我的设想是：
     * 1. 如果该对象正在被读，那优先级应该最高
     * 2. 否则，取决于对象大小和距离开始请求的时间
     * 3. 还可以考虑的因素包括对象的连续性等
     */ 
    void set_priority(int current_time) {
        
    }
};