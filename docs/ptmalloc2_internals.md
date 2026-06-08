# ptmalloc2 内部机制详解

## 1. 核心数据结构

### 1.1 malloc_chunk 结构体

```cpp
// 来自 glibc 源码
struct malloc_chunk
{
    // 前一个 chunk 的大小（如果前一个 chunk 是空闲的）
    INTERNAL_SIZE_T      prev_size;
    
    // 当前 chunk 的大小，包含头部
    // 最低 3 位用作标志位
    INTERNAL_SIZE_T      size;
    
    // 双向链表指针，仅在 chunk 空闲时使用
    struct malloc_chunk* fd;         // 前向指针
    struct malloc_chunk* bk;         // 后向指针
    
    // 仅用于大块的指针（large bins）
    struct malloc_chunk* fd_nextsize; // 下一个更大大小的 chunk
    struct malloc_chunk* bk_nextsize; // 上一个更大大小的 chunk
};
```

### 1.2 malloc_state 结构体

```cpp
// Arena 状态结构体
struct malloc_state
{
    // 互斥锁，保证线程安全
    mutex_t mutex;
    
    // 标志位
    int flags;
    
    // Fastbins - 用于快速分配小内存
    mfastbinptr fastbinsY[NFASTBINS];
    
    // Top chunk - 堆顶的剩余空间
    mchunkptr top;
    
    // 上次分配的剩余部分
    mchunkptr last_remainder;
    
    // Normal bins - 用于管理不同大小的空闲内存块
    mchunkptr bins[NBINS * 2 - 2];
    
    // Bitmap of bins
    unsigned int binmap[BINMAPSIZE];
    
    // 链表指针
    struct malloc_state *next;
    
    // 空闲 arena 链表
    struct malloc_state *next_free;
    
    // 关联的线程数
    INTERNAL_SIZE_T attached_threads;
    
    // 系统内存使用统计
    INTERNAL_SIZE_T system_mem;
    INTERNAL_SIZE_T max_system_mem;
};
```

### 1.3 heap_info 结构体

```cpp
// 堆信息结构体（用于非主线程）
typedef struct _heap_info
{
    mstate ar_ptr; /* Arena for this heap. */
    struct _heap_info *prev; /* Previous heap. */
    size_t size;   /* Current size in bytes. */
    size_t mprotect_size; /* Size in bytes that has been mprotected
                           PROT_READ|PROT_WRITE.  */
    /* Make sure the following data is properly aligned, particularly
     that sizeof (heap_info) + 2 * SIZE_SZ is a multiple of
     MALLOC_ALIGNMENT. */
    char pad[-6 * SIZE_SZ & MALLOC_ALIGN_MASK];
} heap_info;
```

## 2. Bin 系统详解

### 2.1 Fastbins

Fastbins 用于管理小内存块（通常 16-80 字节），采用 LIFO（后进先出）策略。

```cpp
// Fastbin 的特点
/*
1. 数量：10 个
2. 管理的大小：16, 24, 32, 40, 48, 56, 64, 72, 80 字节
3. 策略：LIFO（后进先出）
4. 合并：不会合并相邻的空闲块
5. 标志位：P 位总是设置为 1，防止合并
*/

// Fastbin 数组索引计算
#define fastbin_index(sz) ((((unsigned int)(sz)) >> 4) - 2)

// Fastbin 最大大小
#define MAX_FAST_SIZE     (80 * SIZE_SZ / 4)
#define NFASTBINS         (fastbin_index(request2size(MAX_FAST_SIZE)) + 1)
```

### 2.2 Smallbins

Smallbins 管理 16 到 512 字节的内存块，采用 FIFO（先进先出）策略。

```cpp
// Smallbin 的特点
/*
1. 数量：62 个
2. 管理的大小：16, 24, 32, ..., 512 字节
3. 策略：FIFO（先进先出）
4. 合并：会合并相邻的空闲块
5. 大小：每个 bin 中的 chunk 大小相同
*/

// Smallbin 索引计算
#define smallbin_index(sz)  ((unsigned int)(((sz) >> 4)))

// Smallbin 数量
#define NSMALLBINS          64
#define SMALLBIN_WIDTH      MALLOC_ALIGNMENT
#define SMALLBIN_CORRECTION (MALLOC_ALIGNMENT > 2 * SIZE_SZ)
#define MIN_LARGE_SIZE      ((NSMALLBINS - SMALLBIN_CORRECTION) * SMALLBIN_WIDTH)
```

### 2.3 Largebins

Largebins 管理大于 512 字节的内存块，采用最佳适配策略。

```cpp
// Largebin 的特点
/*
1. 数量：63 个
2. 管理的大小：大于 512 字节
3. 策略：最佳适配
4. 合并：会合并相邻的空闲块
5. 排序：每个 bin 中的 chunk 按大小排序
*/

// Largebin 索引计算
#define largebin_index(sz)  ((unsigned int)(((sz) >> 6) <= 38) ?  \
                           56 + (((sz) >> 6) - 38) : \
                           (unsigned int)(((sz) >> 9) <= 20) ? \
                           91 + (((sz) >> 9) - 20) : \
                           (unsigned int)(((sz) >> 12) <= 10) ? \
                           110 + (((sz) >> 12) - 10) : \
                           (unsigned int)(((sz) >> 15) <= 4) ? \
                           119 + (((sz) >> 15) - 4) : \
                           (unsigned int)(((sz) >> 18) <= 2) ? \
                           124 + (((sz) >> 18) - 2) : \
                           126)
```

### 2.4 Unsorted Bin

Unsorted bin 是一个特殊的 bin，所有释放的 chunk 首先进入这里。

```cpp
// Unsorted bin 的特点
/*
1. 数量：1 个
2. 管理的大小：无限制
3. 策略：先进先出
4. 作用：作为其他 bin 的缓存
5. 处理：在 malloc 时进行整理
*/

// Unsorted bin 索引
#define unsorted_bin(M)     ((M)->bins[1])
```

## 3. 内存分配算法

### 3.1 malloc 分配流程

```cpp
// 简化的 malloc 分配流程
void* _int_malloc(mstate av, size_t bytes)
{
    // 1. 计算实际需要的大小（包含头部）
    size_t nb = request2size(bytes);
    
    // 2. 检查 fastbin
    if (nb <= MAX_FAST_SIZE)
    {
        mchunkptr victim = fastbin_alloc(av, nb);
        if (victim != 0)
        {
            return chunk2mem(victim);
        }
    }
    
    // 3. 检查 smallbin
    if (nb <= MAX_SMALLBIN_SIZE)
    {
        mchunkptr victim = smallbin_alloc(av, nb);
        if (victim != 0)
        {
            return chunk2mem(victim);
        }
    }
    
    // 4. 检查 unsorted bin
    mchunkptr victim = unsorted_bin_alloc(av, nb);
    if (victim != 0)
    {
        return chunk2mem(victim);
    }
    
    // 5. 检查 largebin
    victim = largebin_alloc(av, nb);
    if (victim != 0)
    {
        return chunk2mem(victim);
    }
    
    // 6. 使用 top chunk
    victim = top_chunk_alloc(av, nb);
    if (victim != 0)
    {
        return chunk2mem(victim);
    }
    
    // 7. 扩展堆
    return sysmalloc(nb, av);
}
```

### 3.2 free 释放流程

```cpp
// 简化的 free 释放流程
void _int_free(mstate av, mchunkptr p, int have_lock)
{
    size_t size = chunksize(p);
    
    // 1. 检查是否可以放入 fastbin
    if (size <= MAX_FAST_SIZE)
    {
        // 放入 fastbin，不合并相邻空闲块
        fastbin_insert(av, p, size);
        return;
    }
    
    // 2. 检查相邻 chunk 是否空闲，尝试合并
    mchunkptr consolidated = consolidate_chunks(av, p);
    
    // 3. 放入 unsorted bin
    unsorted_bin_insert(av, consolidated);
}
```

## 4. Arena 管理机制

### 4.1 Arena 创建

```cpp
// Arena 创建过程
static mstate _int_new_arena(size_t size)
{
    // 1. 分配内存
    heap_info* h = new_heap(size, HEAP_MAX_SIZE);
    if (h == 0)
    {
        return 0;
    }
    
    // 2. 初始化 arena
    mstate a = h->ar_ptr;
    malloc_init_state(a);
    
    // 3. 关联线程
    a->attached_threads = 1;
    
    return a;
}
```

### 4.2 Arena 分配策略

```cpp
// Arena 分配策略
static mstate arena_get2(size_t size, mstate avoid_arena)
{
    mstate a;
    
    // 1. 尝试获取现有 arena
    for (a = &main_arena; ; a = a->next)
    {
        if (a != avoid_arena)
        {
            if (mutex_trylock(&a->mutex) == 0)
            {
                return a;
            }
        }
        
        if (a->next == &main_arena)
        {
            break;
        }
    }
    
    // 2. 创建新 arena
    if (narenas < narenas_limit)
    {
        a = _int_new_arena(size);
        if (a != 0)
        {
            return a;
        }
    }
    
    // 3. 等待可用 arena
    return reused_arena(avoid_arena);
}
```

## 5. 内存合并机制

### 5.1 相邻 chunk 合并

```cpp
// 合并相邻空闲 chunk
static mchunkptr consolidate_chunks(mstate av, mchunkptr p)
{
    size_t size = chunksize(p);
    
    // 检查前一个 chunk
    if (!prev_inuse(p))
    {
        mchunkptr prev = prev_chunk(p);
        size += chunksize(prev);
        unlink(av, prev, &bck, &fwd);
        p = prev;
    }
    
    // 检查后一个 chunk
    mchunkptr next = next_chunk(p);
    if (next != av->top)
    {
        if (!inuse(next))
        {
            size += chunksize(next);
            unlink(av, next, &bck, &fwd);
        }
    }
    
    // 更新 chunk 大小
    set_head(p, size | PREV_INUSE);
    
    return p;
}
```

## 6. 线程安全机制

### 6.1 锁机制

```cpp
// Arena 锁机制
class ArenaLock
{
private:
    mstate arena;
    
public:
    ArenaLock(mstate av) : arena(av)
    {
        mutex_lock(&arena->mutex);
    }
    
    ~ArenaLock()
    {
        mutex_unlock(&arena->mutex);
    }
};

// 使用示例
void* thread_malloc(size_t size)
{
    // 获取当前线程的 arena
    mstate av = arena_get(size, 0);
    
    // 加锁
    ArenaLock lock(av);
    
    // 执行分配
    void* result = _int_malloc(av, size);
    
    // 锁会在作用域结束时自动释放
    return result;
}
```

## 7. 性能优化技术

### 7.1 Fastbin 优化

```cpp
// Fastbin 优化技术
/*
1. LIFO 策略：最近使用的 chunk 最先被分配，提高缓存命中率
2. 不合并：避免合并操作的开销
3. 单向链表：减少指针操作
4. 固定大小：快速索引
*/

// Fastbin 分配示例
static mchunkptr fastbin_alloc(mstate av, size_t size)
{
    size_t idx = fastbin_index(size);
    mfastbinptr* fb = &fastbin(av, idx);
    mchunkptr victim = *fb;
    
    if (victim != 0)
    {
        *fb = victim->fd;
        return victim;
    }
    
    return 0;
}
```

### 7.2 Binmap 优化

```cpp
// Binmap 优化技术
/*
1. 位图：使用位图记录哪些 bin 非空
2. 快速查找：避免遍历空 bin
3. 批量处理：一次查找多个 bin
*/

// Binmap 查找示例
static mchunkptr find_bin(mstate av, size_t size)
{
    unsigned int binmap = av->binmap[idx / BINMAPSHIFT];
    unsigned int block = binmap >> (idx % BINMAPSHIFT);
    
    if (block != 0)
    {
        // 找到非空 bin
        int i = __builtin_ctz(block);
        return bin_at(av, idx + i);
    }
    
    return 0;
}
```

## 8. 内存碎片管理

### 8.1 碎片产生原因

```cpp
/*
内存碎片产生的原因：
1. 外部碎片：已分配的内存块之间的空闲空间
2. 内部碎片：分配的内存块中未使用的空间
3. 碎片化模式：频繁分配释放不同大小的内存
*/

// 碎片化示例
void fragmentation_example()
{
    // 分配多个小块
    void* blocks[10];
    for (int i = 0; i < 10; i++)
    {
        blocks[i] = malloc(64);
    }
    
    // 释放偶数索引的块
    for (int i = 0; i < 10; i += 2)
    {
        free(blocks[i]);
    }
    
    // 尝试分配大块 - 可能失败
    void* large = malloc(256);
    
    // 释放剩余块
    for (int i = 1; i < 10; i += 2)
    {
        free(blocks[i]);
    }
    free(large);
}
```

### 8.2 碎片减少策略

```cpp
/*
碎片减少策略：
1. 使用内存池
2. 批量分配释放
3. 使用合适的分配大小
4. 避免频繁分配释放
*/

// 内存池示例
class MemoryPool
{
private:
    struct Block
    {
        Block* next;
    };
    
    Block* free_list;
    char* pool;
    size_t block_size;
    size_t pool_size;
    
public:
    MemoryPool(size_t block_size, size_t num_blocks)
        : block_size(block_size), pool_size(block_size * num_blocks)
    {
        pool = static_cast<char*>(malloc(pool_size));
        free_list = nullptr;
        
        // 初始化空闲链表
        for (size_t i = 0; i < num_blocks; i++)
        {
            Block* block = reinterpret_cast<Block*>(pool + i * block_size);
            block->next = free_list;
            free_list = block;
        }
    }
    
    ~MemoryPool()
    {
        free(pool);
    }
    
    void* allocate()
    {
        if (free_list == nullptr)
        {
            return nullptr;
        }
        
        Block* block = free_list;
        free_list = block->next;
        return block;
    }
    
    void deallocate(void* ptr)
    {
        Block* block = static_cast<Block*>(ptr);
        block->next = free_list;
        free_list = block;
    }
};
```

## 9. 调试和监控

### 9.1 内存统计

```cpp
// 获取内存使用信息
void print_memory_stats()
{
    struct mallinfo info = mallinfo();
    
    std::cout << "=== 内存统计 ===" << std::endl;
    std::cout << "总分配空间: " << info.arena << " 字节" << std::endl;
    std::cout << "已使用空间: " << info.uordblks << " 字节" << std::endl;
    std::cout << "空闲空间: " << info.fordblks << " 字节" << std::endl;
    std::cout << "已释放空间: " << info.keepcost << " 字节" << std::endl;
    std::cout << "fastbin 空间: " << info.smblks << " 字节" << std::endl;
    std::cout << "使用 mmap 分配的空间: " << info.hblkhd << " 字节" << std::endl;
    std::cout << "mmap 分配的块数: " << info.hblks << std::endl;
}
```

### 9.2 内存泄漏检测

```cpp
// 简单的内存泄漏检测
class LeakDetector
{
private:
    static std::unordered_map<void*, size_t> allocations;
    static size_t total_allocated;
    static size_t total_freed;
    
public:
    static void* track_malloc(size_t size)
    {
        void* ptr = malloc(size);
        if (ptr != nullptr)
        {
            allocations[ptr] = size;
            total_allocated += size;
        }
        return ptr;
    }
    
    static void track_free(void* ptr)
    {
        if (ptr != nullptr && allocations.find(ptr) != allocations.end())
        {
            total_freed += allocations[ptr];
            allocations.erase(ptr);
            free(ptr);
        }
    }
    
    static void report()
    {
        std::cout << "=== 内存泄漏报告 ===" << std::endl;
        std::cout << "总分配: " << total_allocated << " 字节" << std::endl;
        std::cout << "总释放: " << total_freed << " 字节" << std::endl;
        std::cout << "未释放: " << (total_allocated - total_freed) << " 字节" << std::endl;
        std::cout << "未释放块数: " << allocations.size() << std::endl;
    }
};

// 初始化静态成员
std::unordered_map<void*, size_t> LeakDetector::allocations;
size_t LeakDetector::total_allocated = 0;
size_t LeakDetector::total_freed = 0;
```

## 10. 环境变量配置

```bash
# 设置 mmap 分配阈值
export MALLOC_MMAP_THRESHOLD_=131072

# 设置 trim 阈值
export MALLOC_TRIM_THRESHOLD_=131072

# 设置 mmap 最大数量
export MALLOC_MMAP_MAX_=65536

# 设置 arena 最大数量
export MALLOC_ARENA_MAX=32

# 启用内存统计
export MALLOC_STATS_=1

# 启用内存检查
export MALLOC_CHECK_=1
```

## 11. 最佳实践

### 11.1 内存分配最佳实践

```cpp
/*
1. 避免频繁分配释放小块内存
2. 使用内存池管理小对象
3. 合理设置分配大小
4. 使用智能指针避免内存泄漏
5. 避免内存碎片化
*/

// 最佳实践示例
void best_practices()
{
    // 1. 使用智能指针
    auto ptr = std::make_unique<int[]>(100);
    
    // 2. 使用内存池
    MemoryPool pool(64, 1000);
    void* block = pool.allocate();
    
    // 3. 批量分配
    std::vector<void*> blocks;
    for (int i = 0; i < 100; i++)
    {
        blocks.push_back(malloc(64));
    }
    
    // 4. 批量释放
    for (void* ptr : blocks)
    {
        free(ptr);
    }
    
    pool.deallocate(block);
}
```

## 12. 参考资料

1. [glibc Malloc Internals](https://sourceware.org/glibc/wiki/MallocInternals)
2. [Understanding glibc malloc](https://sploitfun.wordpress.com/2015/02/10/understanding-glibc-malloc/)
3. [ptmalloc2 源代码分析](http://www.valleytalk.org/wp-content/uploads/2015/02/glibc%E5%86%85%E5%AD%98%E7%AE%A1%E7%90%86ptmalloc%E6%BA%90%E4%BB%A3%E7%A0%81%E5%88%86%E6%9E%901.pdf)
4. [glibc 源代码](https://sourceware.org/git/?p=glibc.git)