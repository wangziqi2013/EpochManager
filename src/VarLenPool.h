
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
    
    /*
     * Allocate() - Allocate a memory of size sz from this chunk
     *
     * This function issues CAS to contend with other threads trying to
     * allocate from this chunk. Return the base address and increase 
     * ref count if it succeeds; o.w. it returns nullptr and nothing is changed
     */
    void *Allocate(size_t sz) {
      ChunkHeader expected_header = header.load();
      // We will update this and CAS this into the chunk header
      ChunkHeader new_header{expected_header.ref_count + 1,
                             expected_header.offset + sz + 8};
                             
      bool ret = \
        chunk_header.compare_exchange_strong(expetced_header, new_header);
        
      if(ret == true) {
         
      }
      
      
    }
  };
  
};

#endif
