#include <unordered_map>
#include <queue>

#include "Disk.hpp"
#include "Request.hpp"

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
    std::unordered_map<int, int> working_disks;  // 有事儿干的磁盘<disk_id, 负责的请求id>

    /*
     * @Description: 选择最合适的3个不同磁盘作为写入磁盘
     * @param object_id: 要写入的对象ID
     * @return: 选择的3个不同磁盘ID，-1表示没有合适的磁盘
    */
    std::vector<int> select_write_disk(int object_id, int tag, int size) {
        // TODO: 任何时间存储系统中空余的存储单元数占总存储单元数的至少 10%
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
            
            candidate_disks.emplace_back(disk, total_score);
        }


        std::sort(candidate_disks.begin(), candidate_disks.end(),
            [](const auto a, const auto b) {
                return a.second > b.second;
            });

        return candidate_disks.empty() ? -1 :
            std::vector<int>{candidate_disks[0].first.id, candidate_disks[1].first.id, candidate_disks[2].first.id};
    }

    /*
     * @Description: 选择一个磁盘负责一个请求的读取
     */
    int select_read_disk(int object_id, std::vector<int>& working_disks) {
        return rand() % numDisks;
    }

public:
    DiskScheduler(int numDisks, int disk_size, int token_G) : G(token_G){
        this->numDisks = numDisks;
        for (int i = 1; i <= numDisks; ++i) {
            disks.emplace_back(i, disk_size, 0);
        }
        this->working_disks.reverse(numDisks);
    }

    void add_request(int req_id, int object_id, int start_timestamp) {
        Request req(req_id, object_id, start_timestamp);
        requests[req_id] = req;
        // TODO: 如果优先级是动态更新的，这里需要处理
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
            obj.replicas[i] = { 
                disk_id, 
                allocated,
                // calculate_access_score(disks[disk_id].head_pos, allocated)
            };
            disks[disk_id].tag_block_num[tag] += size;
        }
        
        saved_objects[object_id] = obj;
        return obj;
    }

    // ! 未完善，正在写
    /*
     * @Description: 在一个时间片中对每个磁盘进行动作，一个请求在一个磁盘上完成读取
     * @param points_action: 存储每个磁头的动作
     * @param completed_requests: 存储可以上报的请求id
     */
    // TODO: 之后针对比较碎片化的对象，可以改成并行读取。
    void read_one_timeslice(std::vector<std::string>& points_action, std::vector<int>& completed_requests) {
        std::vector<int> staging_requests;  // 用于存储优先级较高但没有磁盘可以负责的请求，之后重新入队
        // 让每个磁盘都有工作
        while (working_disks.size() < N && !requests_queue.empty()) {
            // 从请求队列中取出优先级最高的请求
            int req_id = requests_queue.top();
            requests_queue.pop();

            // // 如果该请求已经有了负责磁盘，就继续用该磁盘读
            // if (requests[req_id].responsible_disk_id != -1) {
            //     working_disks.insert(requests[req_id].responsible_disk_id);
            //     continue;
            // }
            else {
                // 否则，为该请求分配一个负责磁盘
                requests[req_id].responsible_disk_id = select_read_disk(requests[req_id].object_id, working_disks);
            }
        }


        // // 处理每个磁盘的请求
        // for (auto& disk : disks) {
        //     int disk_id = disk.id;
        //     std::string& action = points_action[disk_id];
        //     int remaining_tokens = G;

        //     // 优先处理队列前端的请求
        //     if (!disk_queues[disk_id].empty()) {
        //         int req_id = disk_queues[disk_id].front();
        //         ReadTarget& target = active_requests[req_id];
                
        //         // 查找该磁盘上该请求的副本
        //         for (int rep_id = 0; rep_id < REP_NUM; ++rep_id) {
        //             if (target.replica_blocks[rep_id][0] == disk.id) {
        //                 // 计算最优读取路径
        //                 // ...（路径规划算法）
                        
        //                 // 生成动作序列
        //                 int prev_action = -1; // -1:初始状态 0:Read 1:Pass
        //                 for (auto block : target.replica_blocks[rep_id]) {
        //                     // 计算磁头移动
        //                     // ...（移动逻辑）
                            
        //                     // 添加Read动作
        //                     if (prev_action != 0) {
        //                         remaining_tokens -= 64;
        //                     } else {
        //                         remaining_tokens -= std::max(16, static_cast<int>(std::ceil(prev_token * 0.8)));
        //                     }
        //                     action += 'r';
        //                     prev_action = 0;
                            
        //                     // 检查是否完成
        //                     target.read_blocks[rep_id].insert(block);
        //                 }
        //             }
        //         }
                
        //         // 检查请求是否完成
        //         bool all_blocks_read = true;
        //         for (const auto& rep : target.read_blocks) {
        //             if (rep.size() < target.replica_blocks[0].size()) {
        //                 all_blocks_read = false;
        //                 break;
        //             }
        //         }
        //         if (all_blocks_read) {
        //             completed_requests.push_back(req_id);
        //             active_requests.erase(req_id);
        //             disk_queues[disk_id].pop();
        //         }
        //     }
            
        //     // 结束动作
        //     action += '#';
        // }
    }
}