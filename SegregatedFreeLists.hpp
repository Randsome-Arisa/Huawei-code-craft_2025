#include <iostream>
#include <list>
#include <vector>
#include <utility>

#include "limit.h"

// 表示一个空闲的磁盘块（区间）
struct Block {
    int start;  // 起始位置
    int size;   // 块大小

    Block(int s, int sz) : start(s), size(sz) {}

    // 计算块的结束位置（不包括）
    int end() {
        return start + size;
    }
};

// 分离空闲链表管理器
class SegregatedFreeList {
private:
    // buckets[i-1] 管理大小为 i 的空闲块（1 <= i <= 5）
    // buckets[5] 用于管理超过 5 的大块空闲空间
    std::vector<std::list<Block>> buckets;

    /*
     * 尝试分配连续的 size 大小的内存块
     * 采用Worst-Fit策略
     * @param size: 要分配的内存块大小
     * @return: 分配成功返回一个被分配地址的 vector，否则返回空 vector
    */
    std::vector<int> allocate_contiguous(int requestSize) {
        for (int i = MAX_OBJ_SIZE; i >= requestSize - 1; --i) {
            std::list<Block>& bucket = buckets[i];
            if (!bucket.empty()) {
                std::list<Block>::iterator best_it;
                if (i == MAX_OBJ_SIZE) {
                    best_it = std::max_element(bucket.begin(), bucket.end(), 
                        [](const Block& a, const Block& b){ return a.size < b.size; });
                }
                else {
                    best_it = bucket.begin();
                }

                Block block = std::move(*best_it);
                bucket.erase(best_it);
    
                // 分割处理
                if (block.size > requestSize) {
                    int remaining = block.size - requestSize;
                    int bucketIdx = (remaining <= MAX_OBJ_SIZE) ? remaining - 1 : MAX_OBJ_SIZE;
                    buckets[bucketIdx].emplace_back(block.start + requestSize, remaining);
                }
    
                std::vector<int> units(requestSize + 1);
                for (int i = 1; i <= requestSize; ++i) {
                    units[i] = block.start + i - 1;
                }

                return units;
            }
        }
        return {};
    }

    /*
     * 尝试分配不连续的 size 大小的内存块
     * 采用Worst-Fit策略
     * @param size: 要分配的内存块大小
     * @return: 分配成功返回一个地址升序的被分配地址 vector，否则返回空 vector
    */
    std::vector<int> allocate_noncontiguous(int requestSize) {
        std::vector<int> units;
        units.reserve(requestSize + 1); // 提前预留空间，省去扩容耗时
        units.emplace_back(0); // 首元素占位
        
        // 生成可能的分割组合（按从大到小排序）,<int,int>表示<分割大小,分割数量>
        // 例如requestSize=5时，partitions={{[4,1],[1,1]}, {[3,1],[2,1]}, {[2,2],[1,1]}, {[1,5]}}
        std::vector<std::unordered_map<int, int>> partitions;
        for (int main_size = requestSize - 1; main_size >= 1; --main_size) {
            int remaining = requestSize - main_size;
            std::unordered_map<int, int> partition;

            partition[main_size] = 1;
            while (remaining >= main_size) {
                partition[remaining]++;
                remaining -= main_size;
            }
            if (remaining > 0) {
                partition[remaining]++;
            }
            partitions.emplace_back(partition);
        }

        // 尝试每个分割方案
        for (const auto& partation : partitions) {
            bool success = true;
            
            // 注意，调用非连续分配方法时，已经没有buckets[MAX_OBJ_SIZE]了
            // 尝试分配每个部分
            for (const std::pair<int, int>& part : partation) {
                if (buckets[part.first - 1].size() < part.second) {
                    success = false;
                    break;
                }
            }
            
            // 按照该方案进行分配
            if (success) {
                for (const std::pair<int, int>& part : partation) {
                    for (int i = 0; i < part.second; ++i) {
                        std::vector<int> allocated = allocate_contiguous(part.first);
                        for (int j = 1; j < allocated.size(); ++j) {
                            units.emplace_back(allocated[j]);
                        }
                    }
                }

                // 按物理地址排序（提升读取效率）
                std::sort(units.begin() + 1, units.end());
                return units;
            }
        }
        return {};
    }

    // 用于合并新释放的块 newBlock 与相邻的空闲块（如果存在）
    void mergeAndInsert(Block newBlock) {
        bool merged = true;
        // 如果成功合并，重复直到不能合并
        while (merged) {
            merged = false;
            // 遍历所有桶
            for (int b = 0; b < buckets.size(); ++b) {
                auto it = buckets[b].begin();
                while (it != buckets[b].end()) {
                    // 若该空闲块在 newBlock 之前且正好相邻
                    if (it->end() == newBlock.start) {
                        newBlock.start = it->start;         // 合并到前面
                        newBlock.size += it->size;
                        buckets[b].erase(it);
                        merged = true;
                        // break;
                    }
                    // 或者该空闲块在 newBlock 之后，正好相邻
                    else if (newBlock.end() == it->start) {
                        newBlock.size += it->size;
                        buckets[b].erase(it);
                        merged = true;
                        // break;
                    }
                    else {
                        ++it;
                    }
                }
                if (merged) break;
            }
        }
        int bucketIndex = (newBlock.size <= MAX_OBJ_SIZE) ? (newBlock.size - 1) : MAX_OBJ_SIZE;
        buckets[bucketIndex].emplace_back(newBlock);
    }

public:
    SegregatedFreeList() : buckets(MAX_OBJ_SIZE + 1) {}
    // 初始化时整个磁盘内存从 1 到 totalSize 为连续空闲区域
    // TODO: 考虑有没有更好的初始化方法，例如为预处理得知的读取较多的对象预先分配专属的空间区域
    SegregatedFreeList(int totalSize) : buckets(MAX_OBJ_SIZE + 1) {
        buckets[MAX_OBJ_SIZE].emplace_back(1, totalSize);
    }
    
    // 分配 requestSize 大小的内存块
    // 返回分配的内存块，如果分配失败返回空数组
    std::vector<int> allocate(int requestSize) {
        std::vector<int> allocated = allocate_contiguous(requestSize);
        // 优先尝试分配连续空间
        if (allocated.size() > 1) {
            return allocated;
        }
        else {
            allocated = allocate_noncontiguous(requestSize);
            if (allocated.size() > 1) {
                return allocated;
            }
            else {
                return {};
            }
        }
    }

    // 释放磁盘块，将其归还到对应的空闲链表中
    void freeBlock(const std::vector<int>& allocated_units) {
        // 将分散的存储单元转换为连续块（默认已排序）
        std::vector<Block> to_free;
        int current_start = allocated_units[1]; // 跳过首元素
        int current_size = 1;
        
        for (int i = 2; i < allocated_units.size(); ++i) {
            if (allocated_units[i] == allocated_units[i-1] + 1) {
                current_size++;
            } else {
                to_free.emplace_back(current_start, current_size);
                current_start = allocated_units[i];
                current_size = 1;
            }
        }
        to_free.emplace_back(current_start, current_size);

        // 合并每个连续块
        for (auto& block : to_free) {
            mergeAndInsert(block);
        }
    }

    // 返回最大的空闲块size
    int get_largest_free_block_size() {
        if (!buckets[4].empty() || !buckets[5].empty()) {
            return 5;
        }
        else if (!buckets[3].empty()) {
            return 4;
        }
        else if (!buckets[2].empty()) {
            return 3;
        }
        else if (!buckets[1].empty()) {
            return 2;
        }
        else {
            return 1;
        }
    }

    // 用于调试，打印所有桶中的空闲块信息
    /*void printBuckets() {
        for (size_t i = 0; i < buckets.size(); ++i) {
            std::cout << "Bucket " << i << " (size " << (i+1) << (i==MAX_OBJ_SIZE?" or larger": "") << "): ";
            for (const auto& blk : buckets[i]) {
                std::cout << "[" << blk.start << ", " << blk.end() << ") ";
            }
            std::cout << "\n";
        }
    }*/
};
