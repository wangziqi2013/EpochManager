
#pragma once

#ifndef _VAR_LEN_POOL_H
#define _VAR_LEN_POOL_H

/*
 * class VarLenPool - A memory allocator that groups smaller allocations
 *                    
 */
class VarLenPool {
 private:
  /*
   * class ChunkHeader - The header of a chunk being accessed and CAS-ed
   *                     by multiple threads
   *
   * Reference count records how many chunks are allocated in this chunk
   * and offset records the next base address offset from the base of this
   * chunk for fast stack allocation 
   */
  class ChunkHeader {
   public:
    uint32_t ref_count;
    uint32_t offset;
    
    /*
     * Constructor
     */
    ChunkHeader(uint32_t p_ref_count, uint32_t p_offset) :
      ref_count{p_ref_count},
      offset{p_offset}
    {}
  };
  
  class Chunk;
  
  /*
   * class Mem - Represents the memory we actually allocate to caller
   *
   * We need to keep a backward reference to the chunk header in order to 
   * decrease the reference count when it is freed
   */
  class Mem {
   public:
    Chunk *chunk_p;
    // This points to the next available byte
    char data[0]; 
  };
  
  /*
   * class Chunk - A consecutive memory region that is allocated in a 
   *               lock-free manner
   */
  class Chunk {
   public:
    // Count references and base pointer
    std::atomic<ChunkHeader> header;
    // Records the next chunk in a linked list and is linked upon
    // in a lock-free manner
    std::atomic<Chunk *> next_p; 
    // The epoch that this chunk is deleted; this is the first epoch
    // when this chunk is safe for reclamation
    // Only valid if reference count == 0 and this chunk is not the most
    // recent in the queue
    uint64_t delete_epoch;
    
    // Actual data being allocated
    char *data;
    // The first byte after data region
    char *end_data;
    
    /*
     * Constructor
     */
    Chunk(size_t sz) {
      data = new char[sz];
      assert(data != nullptr);
      
      // The end pointer
      end_data = data + sz;
      
      // next pointer is always nullptr since we only append a
      // chunk to the end of the delta chain
      next_p = nullptr;
      
      // Ref count = 0; starting offset = 0
      header.store({0, 0});
      
      delete_epoch = static_cast<decltype(delete_epoch)>(-1);
      
      return;
    }
    
    /*
     * Allocate() - Allocate a memory of size sz from this chunk
     *
     * This function issues CAS to contend with other threads trying to
     * allocate from this chunk. Return the base address and increase 
     * ref count if it succeeds; o.w. it returns nullptr and nothing is changed
     */
    void *Allocate(size_t sz) {
      ChunkHeader expected_header = header.load();
      
      // Either out of memory in this chunk or succeed
      while(1) {
        // This is the base address for next allocation
        // This could not be larger than the end address + 1 of the
        // current chunk
        char *next_base = expected_header.offset + sz + 8;
        
        if(next_base > end_data) {
          return nullptr; 
        }
        
        // We will update this and CAS this into the chunk header
        ChunkHeader new_header{expected_header.ref_count + 1,
                               expected_header.offset + sz + 8};
        
        // If success this will exchange into expected_header                     
        bool ret = \
          header.compare_exchange_strong(expetced_header, new_header);
        
        // If the allocation is successful just return the address
        if(ret == true) {
          // If CAS succeeds then we have reserved the space, and could
          // use the header content just read from the chunk
          Mem *mem_p = reinterpret_cast<Mem *>(data + expected_header.offset);
          // Set the chunk backward pointer
          mem_p->chunk_p = this;
          
          return reinterpret_cast<void *>(mem_p->data);
        }
      } // while(1)
      
      assert(false);
      return nullptr;
    }
  };
  
  /*
   * AllocateChunk() - Allocate a chunk of size at least sz, usually much
   *                   more larger to amortize memory allocation cost
   *
   * This function allocates a chunk of size at least sz + 8 (one extra
   * pointer for the extra backward reference), but if sz is lower than a
   * threshold it falls back to malloc()
   *
   * If CAS fails to append a Chunk object to the end of the delta chain then
   * return nullptr, and the caller thread should try to retry reloading the
   * appending head chunk pointer and then retry allocation. Otherwise
   * the newly allocated chunk pointer is returned as an indication of success
   */
  Chunk *AllocateChunk(size_t sz) {
    if(sz > chunk_size) {
      sz = sz + sizeof(Mem);
    } else {
      // This is the normal chunk size for any allocation size smaller
      // than the chunk size
      sz = chunk_size; 
    }
    
    // Allocate a new chunk object
    Chunk *chunk_p = new Chunk{sz};
    assert(chunk_p != nullptr);
    
    // CAS. If it fails then some thread has already appended a new chunk
    // so instead we just retry
    bool ret = \
      appending_tail_p->next_p->compare_exchange_strong(nullptr, chunk_p);
    if(ret == false) {
      delete chunk_p;
      
      return nullptr;
    } 
    
    // This does not matter since we always CAS with expected being nullptr
    // before we change this pointer the CAS through appending_tail_p
    // would always fail
    appending_tail_p->store(chunk_p);
    
    return chunk_p;
  }
  
 public:
  static const size_t ALIGNMENT = 8;
   
  /*
   * Allocate() - Allocates a 8 byte aligned memory with little alloc/free
   *              overhead
   */
  void *Allocate(size_t sz) {
    // Promote it to the nearest 8 byte boundary
    sz = (sz + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
    assert((sz % ALIGNMENT == 0));
    
    Chunk *chunk_p = appending_tail_p->load(); 
    while(1) {
      void *p = chunk_p->Allocate(sz);
      if(p == nullptr) {
        chunk_p = AllocateChunk(sz);
        // If allocating new chunk failed - some thread must have
        // already done that, so we just retry
        if(chunk_p == nullptr) {
          Chunk *chunk_p = appending_tail_p->load();  
        }
        
        continue;
      }
      
      return p; 
    }
    
    assert(false);
    return nullptr;
  }
  
  /*
   * Free() - Frees a chunk of memory previously allocated
   *
   * This function does lazy free - it could not directly reclaim memory
   * but instead it decreases the reference count of the chunk this pointer
   * lies in, and let the GC thread to compress unused chunks
   */
  void Free(void *p) {
    // Go to the actual starting address of the allocated memory piece
    Mem *mem_p = reinterpret_cast<Mem *>(p) - 1;
    // Load the chunk header field at the header of the chunk
    ChunkHeader header = mem_p->chunk_p->header.load();
    
    // This must succeed - either the chunk becomes full and some thread
    // creates a new chunk, st. no thread could modify the header causing
    // CAS to fail, or it succeeds
    while(1) {
      // Decrease the ref count until succees
      // If return false then header is updated to reflect the newest
      // and we just retry always with ref_count - 1
      bool ret = \
        mem_p->chunk_p->header.compare_exchange_strong(header, 
                                                       {header.ref_count - 1, 
                                                        header.offset});
      if(ret == true) {
        // TODO: Refresh delete epoch here
        //mem_p->chunk_p->delete_epoch = ...
        
        break; 
      }
    } // while not successful
    
    return;
  }
  
  /*
   * Constructor
   */
  ValLenPool(size_t p_chunk_size) :
    chunk_size{p_chunk_size} {
    // Allocate a chunk of standard size
    scanning_head_p = new Chunk{chunk_size};
    assert(scanning_head_p != nullptr);
    
    // This is the pointer to do CAS
    appending_tail_p.store(scanning_head_p);  
      
    return;
  }

 private:  
  // This is the size of each chunk if the requested allocation size is
  // less than this
  uint64_t chunk_size;
  
  // This is the tail we append chunk to
  std::atomic<Chunk *> appending_tail_p;
  // This is the head we start scanning
  Chunk *scanning_head_p;
};

#endif
