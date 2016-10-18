
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
  
  /*
   * class Mem - Represents the memory we actually allocate to caller
   *
   * We need to keep a backward reference to the chunk header in order to 
   * decrease the reference count when it is freed
   */
  class Mem {
   public:
    ChunkHeader *header_p;
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
      
      header.store({0, 0});
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
          Mem *mem_p = reinterpret_cast<Mem *>(expected_header.offset);
          // Set the header
          mem_p->header_p = &header;
          
          return mem_p->data;
        }
      } // while(1)
      
      assert(false);
      return nullptr;
    }
  };
  
};

#endif
