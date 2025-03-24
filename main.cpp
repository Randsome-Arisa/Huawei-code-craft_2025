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

// TODO: 用新结构写删除操作，具体操作放到DiskScheduler类中
void delete_action() {
    int n_delete;
    int abort_num = 0;
    static int _id[MAX_OBJECT_NUM];

    scanf("%d", &n_delete);
    for (int i = 1; i <= n_delete; i++) {
        scanf("%d", &_id[i]);
    }

    // 如果删除时还没读完，就撤销
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        int current_id = object[id].last_request_point;
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                abort_num++;
            }
            current_id = request[current_id].prev_id;
        }
    }

    printf("%d\n", abort_num);
    for (int i = 1; i <= n_delete; i++) {
        int id = _id[i];
        // 请求号
        int current_id = object[id].last_request_point;
        while (current_id != 0) {
            if (request[current_id].is_done == false) {
                printf("%d\n", current_id);
            }
            current_id = request[current_id].prev_id;
        }
        // 删除object[id]号对象在replica[j]号磁盘上的size个存储单元，j = 1~3
        for (int j = 1; j <= REP_NUM; j++) {
            // do_object_delete(object[id].unit[j], disk[object[id].replica[j]], object[id].size);
            do_object_delete(object[id].unit[j], object[id].replica[j], object[id].size);
        }
        object[id].is_delete = true;
    }

    fflush(stdout);
}

// TODO: 用新结构写操作，具体操作放到DiskScheduler类中
void write_action()
{
    int n_write;
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag);
        object[id].last_request_point = 0;

        // 备份三份
        for (int j = 1; j <= REP_NUM; j++) {
            // 选中第 object[id].replica[j] 号磁盘
            // object[id].replica[j] = (id + j) % N + 1;   // 默认磁盘调度算法
            object[id].replica[j] = select_disk(id, j, tag);
            object[id].unit[j] = static_cast<int*>(malloc(sizeof(int) * (size + 1)));
            object[id].size = size;
            object[id].is_delete = false;
            // do_object_write(object[id].unit[j], disk[object[id].replica[j]], size, id);
            do_object_write(object[id].unit[j], object[id].replica[j], size, id);
        }

        printf("%d\n", id);
        for (int j = 1; j <= REP_NUM; j++) {
            printf("%d", object[id].replica[j]);
            for (int k = 1; k <= size; k++) {
                printf(" %d", object[id].unit[j][k]);
            }
            printf("\n");
        }
    }

    fflush(stdout);
}

// TODO: 用新结构写读取操作，具体操作放到DiskScheduler类中
void read_action()
{
    int n_read;
    int request_id, object_id;
    scanf("%d", &n_read);
    for (int i = 1; i <= n_read; i++) {
        scanf("%d%d", &request_id, &object_id);
        request[request_id].object_id = object_id;
        request[request_id].prev_id = object[object_id].last_request_point;
        object[object_id].last_request_point = request_id;
        request[request_id].is_done = false;
    }

    static int current_request = 0;
    static int current_phase = 0;
    if (!current_request && n_read > 0) {
        current_request = request_id;
    }
    if (!current_request) {
        for (int i = 1; i <= N; i++) {
            printf("#\n");
        }
        printf("0\n");
    } else {
        current_phase++;
        object_id = request[current_request].object_id;
        for (int i = 1; i <= N; i++) {
            if (i == object[object_id].replica[1]) {
                if (current_phase % 2 == 1) {
                    printf("j %d\n", object[object_id].unit[1][current_phase / 2 + 1]);
                } else {
                    printf("r#\n");
                }
            } else {
                printf("#\n");
            }
        }

        if (current_phase == object[object_id].size * 2) {
            if (object[object_id].is_delete) {
                printf("0\n");
            } else {
                printf("1\n%d\n", current_request);
                request[current_request].is_done = true;
            }
            current_request = 0;
            current_phase = 0;
        } else {
            printf("0\n");
        }
    }

    fflush(stdout);
}

int main() {
    scanf("%d%d%d%d%d", &T, &M, &N, &V, &G);
    // 分离空闲连表管理存储单元分配算法
    for (int i = 1; i <= N; ++i) {
        segregatedFreeLists[i] = SegregatedFreeList(V);
    }

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

    // 读取每个标签分别读了几个对象块
    for (int i = 1; i <= M; i++) {
        for (int j = 1; j <= (T - 1) / FRE_PER_SLICING + 1; j++) {
            scanf("%*d");
        }
    }

    printf("OK\n");
    fflush(stdout);

    // 初始化磁盘
    std::vector<Disk> disks;
    for (int i = 1; i <= N; ++i) {
        disks.emplace_back(i, V, 0);
    }
    // 磁盘调度器，用于控制读写删操作
    DiskScheduler diskScheduler(N, disks);

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }

    return 0;
}