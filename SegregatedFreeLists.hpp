#include <iostream>
#include <list>
#include <vector>
#include <cassert>

#define MAX_OBJ_SIZE (5)

// TODO: 考虑对象标签进行优化

// 表示一个空闲的磁盘块（区间）
struct Block {
    int start;  // 起始位置
    int size;   // 块大小

    Block(int s, int sz) : start(s), size(sz) {}

    int end() const {
        return start + size;
    }
};

// 分离空闲链表管理器
class SegregatedFreeList {
private:
    // buckets[i-1] 管理大小为 i 的空闲块（1 <= i <= 5）
    // buckets[5] 用于管理超过 5 的大块空闲空间
    std::vector<std::list<Block>> buckets;

    // 用于合并新释放的块 newBlock 与相邻的空闲块（如果存在）
    void mergeAndInsert(Block newBlock) {
        bool merged = true;
        // 如果成功合并，重复直到不能合并
        while (merged) {
            merged = false;
            // 遍历所有桶
            for (size_t b = 0; b < buckets.size(); ++b) {
                for (auto it = buckets[b].begin(); it != buckets[b].end(); ++it) {
                    // 若该空闲块在 newBlock 之前且正好相邻
                    if (it->end() == newBlock.start) {
                        newBlock.start = it->start;         // 合并到前面
                        newBlock.size += it->size;
                        buckets[b].erase(it);
                        merged = true;
                        break;
                    }
                    // 或者该空闲块在 newBlock 之后，正好相邻
                    else if (newBlock.end() == it->start) {
                        newBlock.size += it->size;
                        buckets[b].erase(it);
                        merged = true;
                        break;
                    }
                }
                if (merged) break;
            }
        }
        int bucketIndex = (newBlock.size <= MAX_OBJ_SIZE) ? (newBlock.size - 1) : MAX_OBJ_SIZE;
        buckets[bucketIndex].push_back(newBlock);
    }

public:
    SegregatedFreeList() : buckets(MAX_OBJ_SIZE + 1) {}
    // 初始化时整个磁盘内存从 0 到 totalSize 为连续空闲区域
    // TODO: 考虑有没有更好的初始化方法
    SegregatedFreeList(int totalSize) : buckets(MAX_OBJ_SIZE + 1) {
        buckets[MAX_OBJ_SIZE].push_back(Block(1, totalSize));
    }
    
    // 分配 requestSize 大小的连续内存块
    // 返回分配的内存块，如果分配失败返回 nullptr
    // // TODO: 返回值返回需要打印的数组
    std::vector<int> allocate(int requestSize) {
        // 为保持与demo风格一致，首位不存元素
        std::vector<int> allocated(requestSize + 1);
        // 在 requestSize 对应的桶中查找
        // ? 这里从size为1开始优先找小块，算是best-fit算法，也可以试试worst-fit优先找大块
        for (int i = requestSize - 1; i <= MAX_OBJ_SIZE; ++i) {
            if (!buckets[i].empty()) {
                // 找到一个块，取出它
                Block block = buckets[i].front();
                buckets[i].pop_front();

                // 如果块大小正好等于请求大小，则直接返回，否则需要处理
                if (block.size > requestSize) {
                    // 分割出一个 requestSize 大小的块，剩余部分需要重新插入合适桶中
                    int remainingSize = block.size - requestSize;
                    int remainingStart = block.start + requestSize;
                    // 根据剩余大小放入对应的桶
                    int bucketIndex = remainingSize <= MAX_OBJ_SIZE ? remainingSize - 1 : MAX_OBJ_SIZE;
                    buckets[bucketIndex].push_back(Block(remainingStart, remainingSize));
                }

                for (int j = 1; j <= requestSize; j++) {
                    allocated[j] = block.start + j - 1;
                }
                return allocated;
            }
        }
        // 如果上面没找到合适的块，就必须分配分离的存储单元
        // 此时一定 2 <= requestSize <= 5
        // 优先从比请求小的块中较大的块分配，有利于连续读
        // int index = requestSize - 2;
        // while (buckets[index].empty()) {
        //     --index;
        //     assert(index >= 0);
        // }
        // Block block = buckets[index].front();
        // buckets[index].pop_front();
        // for (int i = 1; i <= block.size; ++i) {
        //     allocated[i] = block.start + i - 1;
        // }
        // requestSize -= index + 1;
        // std::vector<int> v2 = allocate(requestSize);
        // allocated.insert(allocated.begin() + 1 + block.size, v2.begin() + 1, v2.end());
        // return allocated;
        return std::vector<int>();
    }

    // 释放磁盘块，将其归还到对应的空闲链表中
    // // TODO: 考虑合并相邻空闲块
    void freeBlock(std::vector<int>& allocated) {
        if (allocated.size() <= 1) return; // 无有效数据

        // 根据分配的地址（从下标1开始）将连续的区域分段释放
        int segStart = allocated[1];
        // int segSize = 1;
        // for (int i = 2; i < allocated.size(); ++i) {
        //     // 如果当前地址与前一地址连续
        //     if (allocated[i] == allocated[i - 1] + 1) {
        //         segSize++;
        //     } else {
        //         // 释放前一段连续区域
        //         Block freed(segStart, segSize);
        //         mergeAndInsert(freed);
        //         // 开始新段
        //         segStart = allocated[i];
        //         segSize = 1;
        //     }
        // }
        int segSize = allocated.size() - 1;

        // 释放最后一段
        Block freed(segStart, segSize);
        mergeAndInsert(freed);
    }

    // 用于调试，打印所有桶中的空闲块信息
    void printBuckets() {
        for (size_t i = 0; i < buckets.size(); ++i) {
            std::cout << "Bucket " << i << " (size " << (i+1) << (i==MAX_OBJ_SIZE?" or larger": "") << "): ";
            for (const auto& blk : buckets[i]) {
                std::cout << "[" << blk.start << ", " << blk.end() << ") ";
            }
            std::cout << "\n";
        }
    }
};
