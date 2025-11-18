#include <iostream>
#include <cstddef>
#include <mutex>
#include <cassert>
#include <iomanip>

//Testing Memory Allocation and dellocation :)

class MemLLOC {
private:
    struct BlockHeader {
        size_t size;       
        bool free;        
        BlockHeader* next; 
        uint64_t guard;    
    };

    void* memoryPool;
    size_t poolSize;
    BlockHeader* freeList;
    std::mutex mtx;       // For thread-safety

    // Debugging guard
    static constexpr uint64_t GUARD_VALUE = 0xDEADBEEFDEADBEEF;
    static constexpr size_t ALIGNMENT = 16;

    size_t align(size_t size) {
        return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    }

public:
    MemLLOC(size_t size) : poolSize(size) {
        memoryPool = ::operator new(poolSize);
        freeList = static_cast<BlockHeader*>(memoryPool);
        freeList->size = poolSize;
        freeList->free = true;
        freeList->next = nullptr;
        freeList->guard = GUARD_VALUE;
    }

    ~MemLLOC() {
        ::operator delete(memoryPool);
    }

    void* allocate(size_t bytes) {
        std::lock_guard<std::mutex> lock(mtx);
        size_t totalSize = align(bytes + sizeof(BlockHeader));

        BlockHeader* prev = nullptr;
        BlockHeader* current = freeList;

        while (current) {
            if (current->free && current->size >= totalSize) {
                // Split if block is large enough
                if (current->size >= totalSize + sizeof(BlockHeader) + 16) {
                    auto* newBlock = reinterpret_cast<BlockHeader*>(
                        reinterpret_cast<char*>(current) + totalSize
                    );
                    newBlock->size = current->size - totalSize;
                    newBlock->free = true;
                    newBlock->next = current->next;
                    newBlock->guard = GUARD_VALUE;

                    current->size = totalSize;
                    current->next = newBlock;
                }
                current->free = false;
                return reinterpret_cast<void*>(current + 1);
            }
            prev = current;
            current = current->next;
        }

        throw std::bad_alloc();
    }

    void deallocate(void* ptr) {
        if (!ptr) return;

        std::lock_guard<std::mutex> lock(mtx);
        BlockHeader* block = reinterpret_cast<BlockHeader*>(ptr) - 1;

        assert(block->guard == GUARD_VALUE && "Memory corruption detected!");

        block->free = true;

        BlockHeader* current = freeList;
        while (current) {
            if (current->free && current->next && current->next->free) {
                current->size += current->next->size;
                current->next = current->next->next;
            } else {
                current = current->next;
            }
        }
    }

    void printStats() {
        size_t used = 0, freeMem = 0;
        BlockHeader* current = freeList;
        while (current) {
            if (current->free) freeMem += current->size;
            else used += current->size;
            current = current->next;
        }

        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Memory Stats: Total: " << poolSize 
                  << " | Used: " << used 
                  << " | Free: " << freeMem << "\n";
    }
};

// Testing Yayyyyyy

int main() {
    MemLLOC allocator(1024); 

    void* a = allocator.allocate(100);
    void* b = allocator.allocate(200);
    allocator.printStats();

    allocator.deallocate(a);
    allocator.printStats();

    void* c = allocator.allocate(50);
    allocator.printStats();

    allocator.deallocate(b);
    allocator.deallocate(c);
    allocator.printStats();

    return 0;
}
