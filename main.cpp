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
    // 优先分配标签热度高的，一样高时优先分配大小大的对象
    auto comp = [](const Object& a, const Object& b) {
        if (diskScheduler.tag_heat[a.tag] != diskScheduler.tag_heat[b.tag])
            return diskScheduler.tag_heat[a.tag] < diskScheduler.tag_heat[b.tag];
        return a.size > b.size;
    }
    std::priority_queue<Object, std::vector<Object>, decltype(comp)> objects_to_be_written(comp);

    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag);
        printf("%d\n", id);
        Object obj = Object(id, size, tag);
        objects_to_be_written.push(obj);
    }
    while (!objects_to_be_written.empty()) {
        Object obj = diskScheduler.write_object(objects_to_be_written.top());
        for (int j = 0; j < REP_NUM; J++) {
            printf("%d ", obj.replicas[j].disk_id);
            for (int unit : obj.replicas[j].units) {
                printf("%d ", unit);
            }
            printf("\n");
        }
        objects_to_be_written.pop();
    }

    fflush(stdout);
}

void read_action(int timestamp) {
    int n_read;
    scanf("%d", &n_read);
    int request_id, object_id;
    std::vector<std::string> points_action(N + 1, ""); // 每个磁头的动作
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
    // (T - 1) / FRE_PER_SLICING + 1 等价于 ceil(T / 1800)
    int n_epoch = (T - 1) / FRE_PER_SLICING + 1;    // 每1800时间片一个epoch
    // 用一个三维数组tag_info[tag][epoch][删/写/读]存储每个标签在每个epoch中删除、写入、读取的对象块数量
    std::vector<std::vector<std::array<int, 3>>> tag_info(M + 1, std::vector<std::array<int, 3>>(n_epoch + 1, {0, 0, 0}))

    // 读取每个标签分别删除了几个对象块
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &tag_info[i][j][0]);
        }
    }
    // 读取每个标签分别写了几个对象块
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &tag_info[i][j][1]);
        }
    }
    // 读取每个标签分别读了几个对象块
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%d", &tag_info[i][j][2]);
        }
    }

    printf("OK\n");
    fflush(stdout);

    // 磁盘调度器，用于控制读写删操作
    diskScheduler = DiskScheduler(M, N, V, G);

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        // 每个epoch更新一次标签热度
        if ((t - 1) % FRE_PER_SLICING == 0) {
            diskScheduler.update_tag_heat((t - 1) / FRE_PER_SLICING);
        }
        timestamp_action();
        delete_action();
        write_action();
        read_action(t);
    }

    return 0;
}