#include <unordered_map>
#include <queue>

#include "Disk.hpp"
#include "Object.hpp"
#include "Request.hpp"

// TODO: 用自定义的比较函数
using RequestQueue = std::priority_queue<
        int, 
        std::vector<int>, 
        std::function<bool(int, int)>
    >;

class DiskScheduler {
private:
    static int numDisks;
    const int G;
    std::vector<Disk> disks;
    std::unordered_map<int, Object> saved_objects;    // <object_id, Object>
    std::unordered_map<int, Request> requests;    // <request_id，Request>
    RequestQueue requests_queue;  // 请求的优先队列，存储请求id

    // 磁盘负责的请求任务
    struct Task {
        int request_id = 1;  // 请求id，-1表示没有任务
        int objrct_id = - 1;  // 对象id
        int disk_id = -1;     // 磁盘id
        std::queue<int> unit_to_be_read;  // 待读取的存储块
    };
    std::vector<Task> working_disks(numDisks + 1);  // 有工作的磁盘，working_disks[i]表示i号磁盘负责的任务

    /*
     * @Description: 选择最合适的3个不同磁盘作为写入磁盘
     * @param object_id: 要写入的对象ID
     * @return: 选择的3个不同磁盘ID，-1表示没有合适的磁盘
    */
    std::vector<int> select_write_disk(int object_id, int tag, int size) {
        // <Disk, score>，score为优先级得分
        std::vector<std::pair<Disk, float>> candidate_disks;
        
        for (Disk& disk : disks) {
            // 对象越大，越需要有连续的空间可以存储该对象，否则每次读的耗时就越大
            // 用 size_ratio 表示这个对象对连续空间的依赖程度
            float size_ratio = (float)size / MAX_OBJ_SIZE;
            // 1. 足够存放的连续空间（70%~90%权重，减少碎片）
            float contiguous_weight = 0.7f + size_ratio * 0.2f;
            // 2. 尽量选择与该对象同标签少的磁盘（10%~20%权重，平衡负载），有利于不同标签对象的并行读取
            float tag_weight = 1.0f - contiguous_weight;

            float contiguous_score = disk.sfl.get_largest_free_block_size() / MAX_OBJ_SIZE; // 归一化到[0,1]范围
            float tag_score = 1.0f - (disk.tag_slot_num[tag] / disk.size);
            float total_score = (contiguous_score * contiguous_weight) + (tag_score * tag_weight);
            // 如果已使用空间超过90%，则不能选择该磁盘
            // 用下面这种判断只是为了免去转换到float
            if (disk.used_units * 10 > 9 * disk.size)
                total_score = 0;
            
            candidate_disks.emplace_back(disk, total_score);
        }


        std::sort(candidate_disks.begin(), candidate_disks.end(),
            [](const auto a, const auto b) {
                return a.second > b.second;
            });

        return candidate_disks.empty() ? -1 :
            std::vector<int>{candidate_disks[0].first.id, candidate_disks[1].first.id, candidate_disks[2].first.id};
    }


public:
    DiskScheduler(int numDisks, int disk_size, int token_G) : numDisks(numDisks), G(token_G){
        disks.emplace_back();   // 从1开始索引
        for (int i = 1; i <= numDisks; ++i) {
            disks.emplace_back(i, disk_size, 0);
        }
    }

    void add_request(int req_id, int object_id, int start_timestamp) {
        Request req(req_id, object_id, start_timestamp);
        requests[req_id] = req;
        // ? 如果优先级是动态更新的，这里需要处理
        requests_queue.push(req_id);
    }

    /*
     * @Description: 删除对象，释放磁盘空间
     * @param object_id: 要删除的对象ID
     * @return: 若删除的对象在请求队列中，返回被删除的请求id，否则返回-1
     */
    int delete_object(int object_id) {
        if (!saved_objects.count(object_id)) return;
        
        Object& obj = saved_objects[object_id];
        // 释放三个副本
        for (int i = 0; i < REP_NUM; i++) {
            int disk_id = obj.replicas[i].disk_id;
            // 获取该副本的存储单元分配信息
            std::vector<int> units = obj.replicas[i].units;
            
            // 调用对应磁盘的释放函数
            disks[disk_id].sfl.freeBlock(units);
            disks[disk_id].tag_block_num[tag] -= size;
            disks[disk_id].used_units -= size;
        }
        saved_objects.erase(object_id);
        // 如果删除时还没读完，就撤销
        for (auto& pair : requests) {
            if (pair.second.object_id == object_id) {
                int deleted_request_id = pair.first;
                requests.erase(deleted_request_id);
                return deleted_request_id;
            }
        }
        return -1;
    }

    /*
     * @Description: 写入对象到磁盘，保存写入对象信息到saved_objects
     * @param object_id: 要写入的对象ID
     * @param size: 要写入的对象大小
     * @param tag: 要写入的对象标签
     * @return: 写入的对象信息
     */
    Object write_object(int object_id, int size, int tag) {
        Object obj;
        obj.id = object_id;
        obj.tag = tag;
        obj.size = size;
        
        // 为三个副本选择不同磁盘
        std::vector<int> selected_disks = select_write_disk(object_id, tag, size);
        if (selected_disks.size() < REP_NUM) {
            std::cerr << "Not enough available disks" << std::endl;
            return;
        }

        for (int i = 0; i < REP_NUM; i++) {
            int disk_id = selected_disks[i];
            auto allocated = disks[disk_id].sfl.allocate(size);
            // 记录分配信息（带磁头优化标记）
            obj.replicas[i] = {disk_id, allocated};
            disks[disk_id].tag_block_num[tag] += size;
            disks[disk_id].used_units += size;
        }
        
        saved_objects[object_id] = obj;
        return obj;
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
        while (working_disks.size() < N && !requests_queue.empty()) {
            // 从请求队列中取出优先级最高的请求
            int best_req_id = requests_queue.top();
            int request_object_id = requests[best_req_id].object_id;

            // 简单地查看该请求对象的三个副本所在磁盘哪个是空闲的
            // TODO: 可以优化选择算法
            bool found_free_disk = false;
            for (int i = 0; i < REP_NUM; i++) {
                int cur_disk_id = saved_objects[request_object_id].replicas[i].disk_id;
                // working_disks 中没有该磁盘，用该磁盘负责该请求
                if (working_disks[cur_disk_id].request_id == -1) {
                    // ? 如果需要的话可以单独存储一个正在处理的请求数组

                    std::queue<int> to_be_read;
                    for (int unit : saved_objects[request_object_id].replicas[i].units) {
                        to_be_read.push(unit);
                    }
                    Task task = {best_req_id, request_object_id, cur_disk_id, to_be_read};
                    working_disks[cur_disk_id] = task;
                    requests[best_req_id].responsible_disk_id = cur_disk_id;
                    found_free_disk = true;
                    requests_queue.pop();
                    break;
                }
            }
            if (!found_free_disk) {
                staging_requests.push_back(best_req_id);
            }
        }

        // 归还取出之后没有被负责的请求
        for (int req_id : staging_requests) {
            requests_queue.push(req_id);
        }

        // 开始读取操作
        for (int i = 1; i <= numDisks; i++) {
            if (working_disks[i] == -1)
                continue;  // 该磁盘没有工作
            int cur_req_id = working_disks[i].request_id;
            int cur_obj_id = working_disks[i].objrct_id;
            std::queue<int>& unit_to_be_read = working_disks[i].unit_to_be_read;

            int tokens = this->G;

            while (tokens > 0 && !unit_to_be_read.empty()) {
                // 将磁头移动到第一个要读取的存储块
                int head_pos = disks[i].head_point;
                int cur_unit = unit_to_be_read.front();
                int distance = (cur_unit + disks[i].size - head_pos) % disks[i].size;   // 计算距离不用考虑索引从0还是1开始的问题
                // 如果使用全部token也空转不过去，就直接跳过去
                if (distance >= tokens) {
                    tokens = 0;
                    points_action[i] = "j " + std::to_string(cur_unit);
                    disks[i].head_point = cur_unit;
                    disks[i].last_action_is_read = false;
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
                }
                // distance==0，能读就读，否则等到下个时间片
                else {
                    int cost_token = disks[i].last_action_is_read ? std::max(16, static_cast<int>(std::ceil(static_cast<float>tokens * 0.8))) : 64;
                    // 等到下个时间片再读
                    if (tokens < cost_token) {
                        break;
                    }
                    else {
                        tokens -= cost_token;
                        points_action[i] += "r";
                        disks[i].head_point = (disks[i].head_point % disks[i].size) + 1;    // 索引从1开始
                        disks[i].last_action_is_read = true;
                        unit_to_be_read.pop();
                    }
                }
            }
            if (points_action[i][0] != 'j') {
                points_action[i] += "#";
            }

            if (unit_to_be_read.empty()) {
               completed_requests.emplace_back(cur_req_id); 
            }
        }
    }
}