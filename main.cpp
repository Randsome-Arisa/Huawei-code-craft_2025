#include <cstdio>
#include <cassert>
#include <cstdlib>

#include "SegregatedFreeLists.hpp"

#define MAX_DISK_NUM (10 + 1)
#define MAX_DISK_SIZE (16384 + 1)
#define MAX_REQUEST_NUM (30000000 + 1)
#define MAX_OBJECT_NUM (100000 + 1)
#define REP_NUM (3)
#define FRE_PER_SLICING (1800)
#define EXTRA_TIME (105)

typedef struct Request_ {
    int object_id;
    int prev_id;
    bool is_done;
} Request;

typedef struct Object_ {
    int replica[REP_NUM + 1];   // 存在哪3个磁盘
    int* unit[REP_NUM + 1];     // 存在哪3个存储单元数组
    int size;
    int last_request_point;     // 
    bool is_delete;
} Object;

Request request[MAX_REQUEST_NUM];
Object object[MAX_OBJECT_NUM];

int T, M, N, V, G;
int disk[MAX_DISK_NUM][MAX_DISK_SIZE];
int disk_point[MAX_DISK_NUM];   // 磁头位置

std::vector<SegregatedFreeList> segregatedFreeLists(MAX_DISK_NUM);

void timestamp_action()
{
    int timestamp;
    // %*s 跳过TIMESTAMP
    scanf("%*s%d", &timestamp);
    printf("TIMESTAMP %d\n", timestamp);

    fflush(stdout);
}

// 删除disk_unit号磁盘上的size个存储单元
void do_object_delete(const int* object_unit, int* disk_unit, int size)
{
    for (int i = 1; i <= size; i++) {
        disk_unit[object_unit[i]] = 0;
    }
}

/**
 * 调用分离空闲链表时的删除动作
*/
void do_object_delete(const int* object_unit, int disk_num, int size) {
    std::vector<int> allocated(size + 1);
    for (int i = 1; i <= size; i++) {
        disk[disk_num][object_unit[i]] = 0;
        allocated[i] = object_unit[i];
    }
    segregatedFreeLists[disk_num].freeBlock(allocated);
}

void delete_action()
{
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

void do_object_write(int* object_unit, int* disk_unit, int size, int object_id)
{
    int current_write_point = 0;
    for (int i = 1; i <= V; i++) {
        if (disk_unit[i] == 0) {
            disk_unit[i] = object_id;
            object_unit[++current_write_point] = i;
            if (current_write_point == size) {
                break;
            }
        }
    }

    assert(current_write_point == size);
}

/**
 * 调用分离空闲链表的写动作
 */
void do_object_write(int* object_unit, int disk_num, int size, int object_id)
{
    std::vector<int> allocated = segregatedFreeLists[disk_num].allocate(size);
    // 为了与demo代码保持一致
    for (int i = 1; i <= size; ++i) {
        disk[disk_num][allocated[i]] = object_id;
        object_unit[i] = allocated[i];
    }
}

void write_action()
{
    int n_write;
    scanf("%d", &n_write);
    for (int i = 1; i <= n_write; i++) {
        int id, size, tag;
        scanf("%d%d%d", &id, &size, &tag);
        object[id].last_request_point = 0;

        for (int j = 1; j <= REP_NUM; j++) {
            object[id].replica[j] = (id + j) % N + 1;   // 磁盘调度算法，选中第 object[id].replica[j] 号磁盘
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

void clean()
{
    for (auto& obj : object) {
        for (int i = 1; i <= REP_NUM; i++) {
            if (obj.unit[i] == nullptr)
                continue;
            free(obj.unit[i]);
            obj.unit[i] = nullptr;
        }
    }
}

int main()
{
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

    // 初始化每个磁盘的磁头位置为1
    for (int i = 1; i <= N; i++) {
        disk_point[i] = 1;
    }

    for (int t = 1; t <= T + EXTRA_TIME; t++) {
        timestamp_action();
        delete_action();
        write_action();
        read_action();
    }
    clean();

    return 0;
}