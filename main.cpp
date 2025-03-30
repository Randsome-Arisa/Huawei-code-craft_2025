#include <cstdio>
#include <cassert>
#include <cstdlib>

#include "DiskScheduler.hpp"

// 时间片，对象标签数，磁盘数，存储单元数，每个磁头最多消耗的令牌数
int T, M, N, V, G;

void timestamp_action() {
    int timestamp;
    // %*s 跳过TIMESTAMP
    scanf("%*s%d", &timestamp);
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
}

void delete_action(DiskScheduler& diskScheduler) {
    int n_delete;
    scanf("%d", &n_delete);
    std::vector<int> delete_object_ids(n_delete);
    std::vector<int> aborted_requests_ids;
    for (int i = 0; i < n_delete; i++) {
        scanf("%d", &delete_object_ids[i]);
    }

    for (int i = 0; i < n_delete; i++) {
        int object_id = delete_object_ids[i];
        std::vector<int> deleted_request_ids = diskScheduler.delete_object(object_id);
        if (!deleted_request_ids.empty()) {
            for (int deleted_request_id : deleted_request_ids) {
                aborted_requests_ids.emplace_back(deleted_request_id);
            }
        }
    }

    printf("%d\n", aborted_requests_ids.size());
    for (int id : aborted_requests_ids) {
        printf("%d\n", id);
    }

    fflush(stdout);
}

void write_action(DiskScheduler& diskScheduler, int timestamp)
{
    int n_write;
    scanf("%d", &n_write);
    int epoch = (timestamp - 1) / FRE_PER_SLICING;
    // 优先分配标签热度高的，一样高时优先分配大小大的对象
    auto comp = [epoch, &diskScheduler](const Object& a, const Object& b) {
        float a_heat = diskScheduler.get_heat(a.tag, epoch), b_heat = diskScheduler.get_heat(b.tag, epoch);
        if (a_heat != b_heat)
            return a_heat < b_heat;
        else
            return a.size < b.size;
        };
    std::priority_queue<Object, std::vector<Object>, decltype(comp)> objects_to_be_written(comp);

    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag);
        Object obj = Object(id, size, tag);
        objects_to_be_written.emplace(obj);
    }
    while (!objects_to_be_written.empty()) {
        Object obj = objects_to_be_written.top();
        diskScheduler.write_object(obj);
        printf("%d\n", obj.id);
        for (int j = 0; j < REP_NUM; j++) {
            printf("%d ", obj.replicas[j].disk_id);
            std::vector<int>& units = obj.replicas[j].units;
            for (int i = 1; i <= obj.size; i++) {
                printf("%d ", units[i]);
            }
            printf("\n");
        }
        objects_to_be_written.pop();
    }

    fflush(stdout);
}

void read_action(DiskScheduler& diskScheduler, int timestamp) {
    int n_read;
    scanf("%d", &n_read);
    int request_id, object_id;
    std::vector<std::string> points_action(N + 1); // 每个磁头的动作
    for (auto& s : points_action) s.clear();
    std::vector<int> completed_requests; // 可以上报的请求
    for (int i = 0; i < n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        diskScheduler.add_request(request_id, object_id, timestamp);
    }
    diskScheduler.read_one_timeslice(points_action, completed_requests);
    for (int i = 1; i <= N; i++) {
        printf("%s\n", points_action[i].c_str());
    }
    printf("%d\n", completed_requests.size());
    for (int id : completed_requests) {
        printf("%d\n", id);
    }

    fflush(stdout);
}


int main() {
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
    // (T - 1) / FRE_PER_SLICING + 1 等价于 ceil(T / 1800)
    int n_epoch = (T - 1) / FRE_PER_SLICING + 1;    // 每1800时间片一个epoch
    // 用一个三维数组tag_info[tag][epoch][删/写/读]存储每个标签在每个epoch（从0开始）中删除、写入、读取的对象块数量
    std::vector<std::vector<std::vector<int>>> tag_info(M + 1, std::vector<std::vector<int>>(n_epoch + 1, std::vector<int>(3)));
    // 维护一个二维标签热度数组tag_heat[tag][epoch]，本轮和下一轮中（这个窗口可以调整）该标签读得越多越热，删得越少越热
    std::vector<std::vector<float>> tag_heat(M + 1, std::vector<float>(n_epoch + 1));

    // 读取每个标签分别删除了几个对象块
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= n_epoch; j++) {
            scanf("%d", &tag_info[i][j][0]);
        }
    }
    // 读取每个标签分别写了几个对象块
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= n_epoch; j++) {
            scanf("%d", &tag_info[i][j][1]);
        }
    }
    // 读取每个标签分别读了几个对象块
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= n_epoch; j++) {
            scanf("%d", &tag_info[i][j][2]);
        }
    }

    printf("OK\n");
    fflush(stdout);

    // 磁盘调度器，用于控制读写删操作
    DiskScheduler diskScheduler = DiskScheduler(M, N, V, G, tag_info, tag_heat);

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        // 每个epoch更新一次标签热度
        if ((t - 1) % FRE_PER_SLICING == 0) {
            diskScheduler.update_tag_heat((t - 1) / FRE_PER_SLICING + 1);
        }
        timestamp_action();
        delete_action(diskScheduler);
        write_action(diskScheduler, t);
        read_action(diskScheduler, t);
    }

    return 0;
}