// ptmalloc2 内存分配器演示程序
// 编译: g++ -o ptmalloc2_demo ptmalloc2_demo.cpp -lpthread
// 运行: ./ptmalloc2_demo

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include <vector>
#include <chrono>
#include <iomanip>

// 1. 内存分配基本示例
void basic_memory_operations()
{
    std::cout << "\n=== 基本内存操作演示 ===" << std::endl;
    
    // 分配内存
    int* numbers = static_cast<int*>(malloc(10 * sizeof(int)));
    if (numbers == nullptr)
    {
        std::cerr << "内存分配失败" << std::endl;
        return;
    }
    
    std::cout << "分配了 10 个整数的内存" << std::endl;
    std::cout << "内存地址: " << numbers << std::endl;
    
    // 初始化内存
    for (int i = 0; i < 10; i++)
    {
        numbers[i] = i * 10;
    }
    
    // 打印内容
    std::cout << "内存内容: ";
    for (int i = 0; i < 10; i++)
    {
        std::cout << numbers[i] << " ";
    }
    std::cout << std::endl;
    
    // 重新分配内存
    numbers = static_cast<int*>(realloc(numbers, 20 * sizeof(int)));
    if (numbers == nullptr)
    {
        std::cerr << "内存重新分配失败" << std::endl;
        return;
    }
    
    std::cout << "重新分配内存到 20 个整数" << std::endl;
    std::cout << "新内存地址: " << numbers << std::endl;
    
    // 使用新内存
    for (int i = 10; i < 20; i++)
    {
        numbers[i] = i * 10;
    }
    
    // 打印新内容
    std::cout << "新内存内容: ";
    for (int i = 0; i < 20; i++)
    {
        std::cout << numbers[i] << " ";
    }
    std::cout << std::endl;
    
    // 释放内存
    free(numbers);
    std::cout << "内存已释放" << std::endl;
}

// 2. 多线程内存分配演示
#define NUM_THREADS 4
#define ALLOCATIONS_PER_THREAD 1000

struct ThreadData
{
    int thread_id;
    size_t total_allocated;
    size_t allocation_count;
};

void* thread_function(void* arg)
{
    ThreadData* data = static_cast<ThreadData*>(arg);
    std::vector<void*> allocations;
    
    data->total_allocated = 0;
    data->allocation_count = 0;
    
    // 每个线程进行多次分配
    for (int i = 0; i < ALLOCATIONS_PER_THREAD; i++)
    {
        // 随机大小分配，模拟真实使用场景
        size_t size = (rand() % 1000) + 1;
        void* ptr = malloc(size);
        if (ptr != nullptr)
        {
            allocations.push_back(ptr);
            data->total_allocated += size;
            data->allocation_count++;
            
            // 写入一些数据
            memset(ptr, 0xAB, size);
        }
    }
    
    // 释放所有分配的内存
    for (void* ptr : allocations)
    {
        free(ptr);
    }
    
    return nullptr;
}

void multithread_allocation_demo()
{
    std::cout << "\n=== 多线程内存分配演示 ===" << std::endl;
    
    pthread_t threads[NUM_THREADS];
    ThreadData thread_data[NUM_THREADS];
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 创建线程
    for (int i = 0; i < NUM_THREADS; i++)
    {
        thread_data[i].thread_id = i;
        pthread_create(&threads[i], nullptr, thread_function, &thread_data[i]);
    }
    
    // 等待所有线程完成
    for (int i = 0; i < NUM_THREADS; i++)
    {
        pthread_join(threads[i], nullptr);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 打印统计信息
    std::cout << "线程数量: " << NUM_THREADS << std::endl;
    std::cout << "每线程分配次数: " << ALLOCATIONS_PER_THREAD << std::endl;
    std::cout << "总耗时: " << duration.count() << " 毫秒" << std::endl;
    
    size_t total_allocated = 0;
    size_t total_count = 0;
    
    for (int i = 0; i < NUM_THREADS; i++)
    {
        std::cout << "线程 " << i << ": 分配 " << thread_data[i].allocation_count 
                  << " 次，总计 " << thread_data[i].total_allocated << " 字节" << std::endl;
        total_allocated += thread_data[i].total_allocated;
        total_count += thread_data[i].allocation_count;
    }
    
    std::cout << "总计: 分配 " << total_count << " 次，总计 " 
              << total_allocated << " 字节" << std::endl;
}

// 3. 不同大小内存分配模式演示
void allocation_patterns_demo()
{
    std::cout << "\n=== 不同分配模式演示 ===" << std::endl;
    
    // 测试不同大小的分配
    size_t sizes[] = {8, 16, 32, 64, 128, 256, 512, 1024, 4096, 8192};
    const int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    for (int i = 0; i < num_sizes; i++)
    {
        size_t size = sizes[i];
        void* ptr = malloc(size);
        
        if (ptr != nullptr)
        {
            std::cout << "分配 " << std::setw(5) << size << " 字节 -> 地址: " << ptr << std::endl;
            
            // 写入数据
            memset(ptr, 0xCD, size);
            
            free(ptr);
        }
        else
        {
            std::cerr << "分配 " << size << " 字节失败" << std::endl;
        }
    }
}

// 4. 内存碎片化演示
void fragmentation_demo()
{
    std::cout << "\n=== 内存碎片化演示 ===" << std::endl;
    
    const int NUM_BLOCKS = 10;
    void* blocks[NUM_BLOCKS];
    
    // 分配多个内存块
    std::cout << "分配 " << NUM_BLOCKS << " 个内存块..." << std::endl;
    for (int i = 0; i < NUM_BLOCKS; i++)
    {
        blocks[i] = malloc(64);
        if (blocks[i] != nullptr)
        {
            std::cout << "块 " << i << ": " << blocks[i] << std::endl;
        }
    }
    
    // 释放偶数索引的块
    std::cout << "\n释放偶数索引的块..." << std::endl;
    for (int i = 0; i < NUM_BLOCKS; i += 2)
    {
        free(blocks[i]);
        blocks[i] = nullptr;
    }
    
    // 尝试分配更大的块
    std::cout << "\n尝试分配 128 字节的块..." << << std::endl;
    void* large_block = malloc(128);
    if (large_block != nullptr)
    {
        std::cout << "成功分配大块: " << large_block << std::endl;
        free(large_block);
    }
    else
    {
        std::cout << "分配失败，可能因为碎片化" << std::endl;
    }
    
    // 释放剩余块
    std::cout << "\n释放剩余块..." << std::endl;
    for (int i = 1; i < NUM_BLOCKS; i += 2)
    {
        if (blocks[i] != nullptr)
        {
            free(blocks[i]);
        }
    }
}

// 5. calloc vs malloc 演示
void calloc_vs_malloc_demo()
{
    std::cout << "\n=== calloc vs malloc 演示 ===" << std::endl;
    
    const int SIZE = 10;
    
    // malloc 分配
    int* malloc_array = static_cast<int*>(malloc(SIZE * sizeof(int)));
    if (malloc_array == nullptr)
    {
        std::cerr << "malloc 分配失败" << std::endl;
        return;
    }
    
    // calloc 分配
    int* calloc_array = static_cast<int*>(calloc(SIZE, sizeof(int)));
    if (calloc_array == nullptr)
    {
        std::cerr << "calloc 分配失败" << std::endl;
        free(malloc_array);
        return;
    }
    
    // 检查初始化状态
    bool malloc_initialized = true;
    bool calloc_initialized = true;
    
    for (int i = 0; i < SIZE; i++)
    {
        if (malloc_array[i] != 0)
        {
            malloc_initialized = false;
        }
        if (calloc_array[i] != 0)
        {
            calloc_initialized = false;
        }
    }
    
    std::cout << "malloc 初始化状态: " << (malloc_initialized ? "已初始化" : "未初始化") << std::endl;
    std::cout << "calloc 初始化状态: " << (calloc_initialized ? "已初始化" : "未初始化") << std::endl;
    
    // 释放内存
    free(malloc_array);
    free(calloc_array);
}

// 6. 性能测试
void performance_test()
{
    std::cout << "\n=== 性能测试 ===" << std::endl;
    
    const int ITERATIONS = 100000;
    const size_t BLOCK_SIZE = 64;
    
    // 测试 malloc/free 性能
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < ITERATIONS; i++)
    {
        void* ptr = malloc(BLOCK_SIZE);
        if (ptr != nullptr)
        {
            free(ptr);
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "malloc/free 性能测试:" << std::endl;
    std::cout << "迭代次数: " << ITERATIONS << std::endl;
    std::cout << "块大小: " << BLOCK_SIZE << " 字节" << std::endl;
    std::cout << "总耗时: " << duration.count() << " 微秒" << std::endl;
    std::cout << "平均每次操作: " << std::fixed << std::setprecision(2) 
              << static_cast<double>(duration.count()) / ITERATIONS << " 微秒" << std::endl;
}

// 7. 内存使用信息
#include <malloc.h>

void memory_info_demo()
{
    std::cout << "\n=== 内存使用信息 ===" << std::endl;
    
    struct mallinfo info = mallinfo();
    
    std::cout << "总分配空间: " << info.arena << " 字节" << std::endl;
    std::cout << "已使用空间: " << info.uordblks << " 字节" << std::endl;
    std::cout << "空闲空间: " << info.fordblks << " 字节" << std::endl;
    std::cout << "已释放空间: " << info.keepcost << " 字节" << std::endl;
    
    // 分配一些内存后再查看
    std::vector<void*> allocations;
    for (int i = 0; i < 100; i++)
    {
        allocations.push_back(malloc(1024));
    }
    
    info = mallinfo();
    std::cout << "\n分配 100KB 后:" << std::endl;
    std::cout << "总分配空间: " << info.arena << " 字节" << std::endl;
    std::cout << "已使用空间: " << info.uordblks << " 字节" << std::endl;
    
    // 释放内存
    for (void* ptr : allocations)
    {
        free(ptr);
    }
}

// 主函数
int main()
{
    std::cout << "ptmalloc2 内存分配器演示程序" << std::endl;
    std::cout << "==============================" << std::endl;
    
    // 运行各种演示
    basic_memory_operations();
    allocation_patterns_demo();
    calloc_vs_malloc_demo();
    fragmentation_demo();
    multithread_allocation_demo();
    performance_test();
    memory_info_demo();
    
    std::cout << "\n==============================" << std::endl;
    std::cout << "演示完成" << std::endl;
    
    return 0;
}