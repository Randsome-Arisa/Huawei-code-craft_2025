#include <cstdio>
#include <cassert>
#include <cstdlib>

#include "DiskScheduler.hpp"

// 时间片，对象标签数，磁盘数，存储单元数，每个磁头最多消耗的令牌数
int T, M, N, V, G;
DiskScheduler diskScheduler;

void timestamp_action() {
    int timestamp;
    // %*s 跳过TIMESTAMP
    scanf("%*s%d", &timestamp);
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
}

void delete_action() {
    int n_delete;
    scanf("%d", &n_delete);
    std::vector<int> delete_object_ids(n_delete);
    std::vector<int> aborted_requests_ids;
    aborted_requests_ids.reverse(n_delete); // 预留空间，节省时间
    for (int i = 0; i < n_delete; i++) {
        scanf("%d", &delete_object_ids[i]);
    }

    
    for (int i = 0; i < n_delete; i++) {
        int object_id = delete_object_ids[i];
        int deleted_request_id = diskScheduler.delete_object(object_id);
        if (deleted_request_id != -1) {
            aborted_requests_ids.emplace_back(deleted_request_id);
        }
    }

    printf("%d\n", aborted_requests_ids.size());
    for (int id : aborted_requests_ids) {
        printf("%d\n", id);
    }

    fflush(stdout);
}

void write_action()
{
    int n_write;
    scanf("%d", &n_write);
    // TODO: 这里可以不按顺序写入，任务书中说输出的写入顺序和输入的可以不一致，或许这里可以预处理后写入得到更好的效果
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag);
        printf("%d\n", id);
        Object obj = diskScheduler.write_object(id, size, tag);
        for (int j = 0; j < REP_NUM; J++) {
            printf("%d ", obj.replicas[j].disk_id);
            for (int unit : obj.replicas[j].units) {
                printf("%d ", unit);
            }
            printf("\n");
        }
    }

    fflush(stdout);
}

// ! 未完善，正在写
void read_action(int timestamp) {
    int n_read;
    scanf("%d", &n_read);
    int request_id, object_id;
    std::vector<std::string> points_action(N); // 每个磁头的动作
    std::vector<int> completed_requests; // 可以上报的请求
    for (int i = 0; i < n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        diskScheduler.add_request(request_id, object_id, timestamp);
    }
    diskScheduler.read_one_timeslice(points_action, completed_requests);

    for (std::string& point_action : points_action) {
        printf("%s\n", point_action.c_str());
    }
    printf("%d\n", completed_requests.size());
    for (int id : completed_requests) {
        printf("%d\n", id);
    }

    fflush(stdout);
}


int main() {
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
    // TODO: 以下全局信息可以被用来优化策略，暂时不利用也不影响

    // 读取每个标签分别删除了几个对象块
    for (int i = 1; i <= M; i++) {
        // (T - 1) / FRE_PER_SLICING + 1 等价于 ceil(T / 1800)
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    // 读取每个标签分别写了几个对象块
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    // TODO: 为读取较多的标签优先分配连续空间
    // 读取每个标签分别读了几个对象块
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    printf("OK\n");
    fflush(stdout);

    // 磁盘调度器，用于控制读写删操作
    diskScheduler = DiskScheduler(N, V, G);

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }

    return 0;
}