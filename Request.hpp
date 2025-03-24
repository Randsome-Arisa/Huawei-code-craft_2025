enum Status {
    PENDING,
    READING,
    COMPLETED
};

class Request {
    int req_id; // 请求id
    int object_id;  // 被请求读取的对象id
    Status status;  // 状态

    int start_timestamp;    // 请求到达的时间戳
    int remaning_time;  // 要得分剩余的最大时间
    double priority;    // 优先级
};