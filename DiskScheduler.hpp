#include <queue>
#include <algorithm>
#include <string>
#include <cmath>
#include <functional>

#include "Disk.hpp"
#include "Object.hpp"
#include "Request.hpp"

class DiskScheduler {
private:
    int numTag;   // tag数
    int numDisks;
    int G;
    std::vector<std::vector<std::vector<int>>> tag_info; // 每个标签在每个epoch中删除、写入、读取的对象块数量
    // 维护一个二维标签热度数组tag_heat[tag][epoch]，本轮和下一轮中（这个窗口可以调整）该标签读得越多越热，删得越少越热
    std::vector<std::vector<float>> tag_heat;
    std::vector<Disk> disks;
    std::unordered_map<int, Object> saved_objects;    // <object_id, Object>
    std::unordered_map<int, Request> requests;    // <request_id，Request>
    
    using RequestQueue = std::priority_queue<int, std::vector<int>, std::function<bool(int, int)>>;
    RequestQueue requests_queue;  // 请求的优先队列，存储请求id

    // 磁盘负责的请求任务
    struct Task {
        int request_id = -1;  // 请求id，-1表示没有任务
        int object_id = -1;  // 对象id
        int disk_id = -1;     // 磁盘id
        std::queue<int> unit_to_be_read;  // 待读取的存储块
    };
    std::vector<Task> working_disks;  // 有工作的磁盘，working_disks[i]表示i号磁盘负责的任务

    bool have_idle_disk() {
        for (int i = 1; i <= numDisks; ++i) {
            if (working_disks[i].request_id == -1) {
                return true;
            }
        }
        return false;
    }

    /*
     * @Description: 选择最合适的3个不同磁盘作为写入磁盘
     * @param object_id: 要写入的对象ID
     * @return: 选择的3个不同磁盘ID，-1表示没有合适的磁盘
    */
    std::vector<int> select_write_disk(int object_id, int tag, int size) {
        // <int, score>，int为disk_id, score为优先级得分
        std::vector<std::pair<int, float>> candidate_disks;
        
        for (int i = 1; i <= numDisks; ++i) {
            Disk& disk = disks[i];
            // 对象越大，越需要有连续的空间可以存储该对象，否则每次读的耗时就越大
            // 用 size_ratio 表示这个对象对连续空间的依赖程度
            float size_ratio = (float)size / (float)MAX_OBJ_SIZE;
            // 1. 足够存放的连续空间（70%~90%权重，减少碎片）
            float contiguous_weight = 0.7f + size_ratio * 0.2f;
            // 2. 尽量选择与该对象同标签少的磁盘（10%~20%权重，平衡负载），有利于不同标签对象的并行读取
            float tag_weight = 1.0f - contiguous_weight;

            float contiguous_score = static_cast<float>(disk.sfl.get_largest_free_block_size()) / (float)MAX_OBJ_SIZE; // 归一化到[0,1]范围
            float tag_score = 1.0f - (static_cast<float>(disk.tag_slot_num[tag]) / static_cast<float>(disk.size));
            float total_score = (contiguous_score * contiguous_weight) + (tag_score * tag_weight);
            // 如果已使用空间超过90%，则不能选择该磁盘
            // 用下面这种判断只是为了免去转换到float
            if (disk.used_units * 10 > 9 * disk.size)
                total_score = -1;
            
            candidate_disks.emplace_back(disk.id, total_score);
        }


        std::sort(candidate_disks.begin(), candidate_disks.end(),
            [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
                return a.second > b.second;
            });

        if (candidate_disks.size() < 3)
            return {};
        else
            return std::vector<int>({candidate_disks[0].first, candidate_disks[1].first, candidate_disks[2].first});
    }

     // TODO: 完善优先级算法
    /*
     * 我的设想是：
     * 1. 如果该对象正在被读，那优先级应该最高
     * 2. 否则，取决于对象大小和距离开始请求的时间
     * 3. 还可以考虑的因素包括对象的连续性等
     */ 
     void set_priority(int request_id) {
        Request& req = requests[request_id];
        if (req.status == Status::COMPLETED) {  // 如果已经完成，优先级最低
            req.priority = 0;
            return;
        }
        if (req.status == Status::READING) {  // 如果正在读，优先级最高
            req.priority = 10000000;
            return;
        }
        // 简单地计算三个副本所在磁盘的磁头分别到该对象第一个存储单元的距离作为距离权重
        float distance_weight = 0.0f;
        for (int i = 0; i < REP_NUM; i++) {
            distance_weight += static_cast<float>((saved_objects[req.object_id].replicas[i].units[0] + disks[i].size - disks[saved_objects[req.object_id].replicas[i].disk_id].head_point) % disks[i].size);
        }
        // 标签热度越高越优先读
        int epoch = (req.start_timestamp - 1) / FRE_PER_SLICING + 1;
        float tag_weight = tag_heat[saved_objects[req.object_id].tag][epoch];

        // 综合计算优先级
        req.priority = distance_weight * 0.4f + tag_weight * 0.6f;
    }

public:
    DiskScheduler(int M, int numDisks, int disk_size, int G, std::vector<std::vector<std::vector<int>>> tag_info, std::vector<std::vector<float>> tag_heat)
        : disks(MAX_DISK_NUM), working_disks(MAX_DISK_NUM),
        requests_queue([this](int request_id1, int request_id2) -> bool {
            Request& req1 = requests[request_id1];
            Request& req2 = requests[request_id2];
            return req1.priority < req2.priority;
            })
    {
        this->numTag = M;
        this->numDisks = numDisks;
        this->G = G;
        for (int i = 1; i <= numDisks; ++i) {
            disks[i] = Disk(i, disk_size);
        }
        this->tag_info = tag_info;
        this->tag_heat = tag_heat;
    }

    void add_request(int req_id, int object_id, int timestamp) {
        Request req(req_id, object_id, timestamp);
        requests[req_id] = req;
        // ? 如果优先级是动态更新的，这里需要处理
        set_priority(req_id);  // 计算优先级
        requests_queue.push(req_id);
    }

    void update_tag_heat(int epoch) {
        // 计算每个标签在每个epoch中的热度
        // 在一个窗口内的epoch中，读得越多越热，删得越少越热，tag_heat[t][e] = tag_info[t][e, e + 1, ...][read] /...[delete]
        std::vector<int> read_sum(numTag + 1, 0);    // 每个标签在每个epoch中的读的数量
        std::vector<int> delete_sum(numTag + 1, 0);  // 每个标签在每个epoch中的删除的数量
        for (int tag = 1; tag <= numTag; tag++) {
            for (int i = epoch; i < tag_info[1].size() && i < epoch + WINDOW_SIZE; i++) {
                read_sum[tag] += tag_info[tag][i][2];
                delete_sum[tag] += tag_info[tag][i][0]; 
            }
            tag_heat[tag][epoch] = static_cast<float>(read_sum[tag]) / (static_cast<float>(delete_sum[tag]) + 1.0); // 加1防止除以0;
        }
    }

    float get_heat(int tag, int epoch) const {
        return tag_heat[tag][epoch];
    }

    /*
     * @Description: 删除对象，释放磁盘空间
     * @param object_id: 要删除的对象ID
     * @return: 若删除的对象在请求队列中，返回被删除的请求id数组，否则返回{}
     */
    std::vector<int> delete_object(int object_id) {
        if (saved_objects.find(object_id) == saved_objects.end())
            return {};
        
        Object& obj = saved_objects[object_id];
        // 释放三个副本
        for (int i = 0; i < REP_NUM; i++) {
            int disk_id = obj.replicas[i].disk_id;
            // 获取该副本的存储单元分配信息
            std::vector<int>& units = obj.replicas[i].units;
            
            // 调用对应磁盘的释放函数
            disks[disk_id].sfl.freeBlock(units);
            disks[disk_id].tag_slot_num[obj.tag] -= obj.size;
            disks[disk_id].used_units -= obj.size;
        }

        saved_objects.erase(object_id);
        // 如果删除时还没读完，就撤销
        std::vector<int> deleted_request_ids;
        for (std::pair<const int, Request>& Irequest : requests) {
            Request& request = Irequest.second;
            if (request.object_id == object_id) {
                request.status = Status::COMPLETED;
                for (int k = 1; k <= numDisks; k++) {
                    if (working_disks[k].request_id == request.req_id) {
                        working_disks[k].request_id = -1;
                        working_disks[k].unit_to_be_read = std::queue<int>();
                        break;
                    }
                }

                // 从requests_queue中删除该请求，创建一个新的临时 priority_queue
                RequestQueue new_queue;
                // 逐个取出元素，重新插入到新队列（跳过要删除的元素）
                while (!requests_queue.empty()) {
                    int top = requests_queue.top();
                    requests_queue.pop();
                    if (top != request.req_id) {
                        new_queue.push(top);
                    }
                }
                // 用新的队列替换旧的队列
                requests_queue = std::move(new_queue);

                deleted_request_ids.emplace_back(request.req_id);
            }
        }
        if (deleted_request_ids.empty()) {
            return {};
        } else {
            return deleted_request_ids;
        }
    }

    /*
     * @Description: 写入对象到磁盘，保存写入对象信息到saved_objects
     * @param obj: 要写入的对象
     * @return: 写入的对象信息
     */
    void write_object(Object& obj) {        
        // 为三个副本选择不同磁盘
        std::vector<int> selected_disks = select_write_disk(obj.id, obj.tag, obj.size);
        if (selected_disks.size() < REP_NUM) {
            printf("Not enough available disks\n");
            return;
        }

        for (int i = 0; i < REP_NUM; i++) {
            int disk_id = selected_disks[i];
            std::vector<int> allocated = disks[disk_id].sfl.allocate(obj.size);
            if (allocated.empty()) {
                printf("fail to allocate units\n");
                return;
            }
            // 记录分配信息（带磁头优化标记）
            obj.replicas[i] = {disk_id, allocated};
            disks[disk_id].tag_slot_num[obj.tag] += obj.size;
            disks[disk_id].used_units += obj.size;
        }
        
        saved_objects[obj.id] = obj;
    }

    /*
     * @Description: 在一个时间片中对每个磁盘进行动作，一个请求在一个磁盘上完成读取
     * @param points_action: 存储每个磁头的动作
     * @param completed_requests: 存储可以上报的请求id
     */
    // TODO: 之后针对比较碎片化的对象，可以改成并行读取。
    void read_one_timeslice(std::vector<std::string>& points_action, std::vector<int>& completed_requests) {
        std::vector<int> staging_requests;  // 用于存储优先级较高但没有磁盘可以负责的请求，之后重新入队
        // 尽量让每个磁盘都有工作
        while (!requests_queue.empty() && have_idle_disk()) {
            // 从请求队列中取出优先级最高的请求
            int best_req_id = requests_queue.top();
            // 如果是已经被删除的对象，状态会是 COMPLETED
            // 注意这里不会是经过读取后的完成状态，那种情况会在后面被直接处理
            if (requests.find(best_req_id) == requests.end()) {
                requests_queue.pop();
                continue;
            }

            // 简单地查看该请求对象的三个副本所在磁盘哪个是空闲的
            // TODO: 可以优化选择算法
            int request_object_id = requests[best_req_id].object_id;
            bool found_free_disk = false;
            for (int i = 0; i < REP_NUM; i++) {
                int cur_disk_id = saved_objects[request_object_id].replicas[i].disk_id;
                // working_disks 中没有该磁盘，用该磁盘负责该请求
                if (working_disks[cur_disk_id].request_id == -1) {
                    std::queue<int> to_be_read;
                    for (int j = 1; j < saved_objects[request_object_id].replicas[i].units.size(); j++) {
                        to_be_read.push(saved_objects[request_object_id].replicas[i].units[j]);
                    }
                    Task task = {best_req_id, request_object_id, cur_disk_id, to_be_read};
                    working_disks[cur_disk_id] = task;
                    requests[best_req_id].responsible_disk_id = cur_disk_id;
                    requests[best_req_id].status = Status::READING;
                    found_free_disk = true;
                    requests_queue.pop();
                    break;
                }
            }
            if (!found_free_disk) {
                staging_requests.emplace_back(best_req_id);
            }
        }

        // 归还取出之后没有被负责的请求
        for (int req_id : staging_requests) {
            requests_queue.push(req_id);
        }

        // 开始读取操作
        for (int i = 1; i <= numDisks; i++) {
            // 该磁盘没有工作
            if (working_disks[i].request_id == -1) {
                points_action[i] = "#";
                continue;
            }
            int cur_req_id = working_disks[i].request_id;
            int cur_obj_id = working_disks[i].object_id;
            std::queue<int>& unit_to_be_read = working_disks[i].unit_to_be_read;

            int tokens = this->G;
            while (tokens > 0 && !unit_to_be_read.empty()) {
                // 将磁头移动到第一个要读取的存储块
                int head_pos = disks[i].head_point;
                int cur_unit = unit_to_be_read.front();
                int distance = (cur_unit + disks[i].size - head_pos) % disks[i].size;   // 计算距离不用考虑索引从0还是1开始的问题
                // 如果使用全部token也空转不过去，就直接跳过去
                if (tokens == G && distance >= tokens) {
                    tokens = 0;
                    points_action[i] = "j " + std::to_string(cur_unit);
                    disks[i].head_point = cur_unit;
                    disks[i].last_action_is_read = false;
                    disks[i].last_token_cost = G;
                }
                // 空转过去
                else if (distance != 0) {
                    // 计算移动距离所需的时间
                    tokens -= distance; // 消耗时间
                    for (int k = 0; k < distance; k++) {
                        points_action[i] += "p";
                    }
                    disks[i].head_point = cur_unit;
                    disks[i].last_action_is_read = false;
                    disks[i].last_token_cost = distance;
                }
                // distance==0，能读就读，否则等到下个时间片
                else {
                    int cost_token = disks[i].last_action_is_read ? std::max(16, static_cast<int>(std::ceil(static_cast<double>(disks[i].last_token_cost) * 0.8))) : 64;
                    // 等到下个时间片再读
                    if (tokens < cost_token) {            
                        if (unit_to_be_read.empty()) {
                           completed_requests.emplace_back(cur_req_id);
                           working_disks[i].request_id = -1;
                           requests[cur_req_id].status = Status::COMPLETED;
                           requests.erase(cur_req_id);
                        }
                        break;
                    }
                    else {
                        tokens -= cost_token;
                        points_action[i] += "r";
                        disks[i].head_point = (disks[i].head_point % disks[i].size) + 1;    // 索引从1开始
                        disks[i].last_action_is_read = true;
                        disks[i].last_token_cost = cost_token;
                        unit_to_be_read.pop();
                        if (unit_to_be_read.empty()) {
                            completed_requests.emplace_back(cur_req_id);
                            working_disks[i].request_id = -1;
                            requests[cur_req_id].status = Status::COMPLETED;
                            requests.erase(cur_req_id);
                        }
                    }
                }
            }
            if (points_action[i].empty() || points_action[i][0] != 'j') {
                points_action[i] += "#";
            }
        }
    }
};