#include <stdio.h>
#include <stdlib.h>
#include <bits/stdc++.h>
#include <inttypes.h>
#include "sim.h"
// #include "cache.cc"
#include <string>

using namespace std;

class Cache
{
public:
   uint32_t tag;
   bool validity;
   bool dirty;
   int lru;
};

class Prefetch
{
public:
   uint32_t address;
   int lru;
};

class Address
{
public:
   int num_of_sets;
   int num_of_index_bits;
   int num_of_offset_bits;
   int num_of_tag_bits;
   uint32_t address;
   uint32_t index_value;
   uint32_t tag_value;

   Address(uint32_t size, uint32_t assoc, uint32_t blockSize)
   {
      if ((size != 0) && (assoc != 0))
      {
         num_of_sets = size / (assoc * blockSize);
         num_of_index_bits = log2(num_of_sets);
         num_of_offset_bits = log2(blockSize);
         num_of_tag_bits = 32 - (num_of_index_bits + num_of_offset_bits);
      }
   }
};

int getTagBits(uint32_t address, int num_of_offset_bits, int num_of_index_bits)
{
   uint32_t tag_value = address >> (num_of_offset_bits + num_of_index_bits);
   return tag_value;
}

int getTagForCache2(uint32_t tag, uint32_t index, int num_of_index_bits_cache1, int num_of_index_bits_cache2)
{
   uint32_t newAddress = tag << num_of_index_bits_cache1;
   newAddress = newAddress + index;
   return newAddress >> num_of_index_bits_cache2;
}

int getIndexForCache2(uint32_t tag, uint32_t index, int num_of_index_bits_cache1, int num_of_index_bits_cache2, int num_of_offset_bits_cache2, int num_of_tag_bits_cache2)
{
   uint32_t newAddress = tag << num_of_index_bits_cache1;
   newAddress = newAddress + index;
   newAddress = newAddress << (num_of_offset_bits_cache2 + num_of_tag_bits_cache2);
   return newAddress >> (num_of_offset_bits_cache2 + num_of_tag_bits_cache2);
}

int getIndexBits(uint32_t address, int num_of_offset_bits, int num_of_index_bits, uint32_t num_of_tag_bits)
{
   uint32_t index_value;
   index_value = address << num_of_tag_bits;
   if ((num_of_offset_bits + num_of_tag_bits) < 32)
   {
      index_value = index_value >> (num_of_offset_bits + num_of_tag_bits);
   }
   else
   {
      index_value = 0;
   }
   return index_value;
}

void resetSetAddress(Cache *cache, int assoc)
{
   for (j = 0; j < assoc; j++)
   {
      cache[j].lru = j;
      cache[j].dirty = 0;
      cache[j].tag = 0;
      cache[j].validity = 0;
   }
}

void cache_hit_update_lru(Cache *cache, int block_number, uint32_t assoc)
{
   for (j = 0; j < assoc; j++)
   {
      if (cache[j].lru < cache[block_number].lru)
      {
         cache[j].lru += 1;
      }
   }
   cache[block_number].lru = 0;
}

void cache_write(Cache *cache, uint32_t assoc, uint32_t tag, bool is_dirty)
{
   for (i = 0; i < assoc; i++)
   {
      if (cache[i].lru == assoc - 1)
      {
         cache[i].tag = tag;
         cache[i].lru = 0;
         cache[i].validity = 1;
         if ((is_dirty == 1))
         {
            cache[i].dirty = 1;
         }
         else
         {
            cache[i].dirty = 0;
         }
      }
      else
      {
         cache[i].lru += 1;
      }
   }
}

int update_dirty_bit(char rw)
{
   bool flag;
   if (rw == 'r')
   {
      flag = 0;
   }
   else
   {
      flag = 1;
   }
   return flag;
}

int searchPrefetchRow(Prefetch *prefetch_row, int size, uint32_t address)
{
   int i;
   int addr_found_in_stream_at_position = -1;
   for (i = 0; i < size; i++)
   {
      if (address == prefetch_row[i].address)
      {
         addr_found_in_stream_at_position = i;
         break;
      }
   }
   return addr_found_in_stream_at_position;
}

void updatePrefetchStreamValues(Prefetch *prefetch_row, int size, uint32_t address, int offset)
{
   int i;
   int count = 1;
   if ((offset >= 0) && ((size - offset - 1) > 0))
   {
      for (i = 0; i < (size - offset - 1); i++)
      {
         prefetch_row[i].address = prefetch_row[offset + i + 1].address;
      }
      for (i = (size - offset - 1); i < size; i++)
      {
         num_cache1_prefetches++;
         prefetch_row[i].address = prefetch_row[i - 1].address + 1;
      }
   }
   else
   {
      for (i = 0; i < size; i++)
      {
         num_cache1_prefetches++;
         prefetch_row[i].address = address + count;
         count++;
      }
   }
}

void prefetchHitUpdateLRU(Prefetch **prefetch, int num_of_stream_buffers, int block_number)
{
   int j;
   for (j = 0; j < num_of_stream_buffers; j++)
   {
      if (prefetch[block_number]->lru > prefetch[j]->lru)
      {
         prefetch[j]->lru += 1;
      }
   }
   prefetch[block_number]->lru = 0;
}

int updatePrefetch(Prefetch **prefetch, int num_of_stream_buffers, int size, uint32_t address)
{
   int i, j, k, addr_found_in_stream_at_position = -1;
   for (i = 0; i < num_of_stream_buffers; i++)
   {
      for (int k = 0; k < num_of_stream_buffers; k++)
      {
         if (prefetch[k]->lru == i)
         {
            addr_found_in_stream_at_position = searchPrefetchRow(prefetch[k], size, address);
            if (addr_found_in_stream_at_position > -1)
            {
               updatePrefetchStreamValues(prefetch[k], size, address, addr_found_in_stream_at_position);
               prefetchHitUpdateLRU(prefetch, num_of_stream_buffers, k);
               break;
            }
         }
      }
      if (addr_found_in_stream_at_position > -1)
      {
         break;
      }
   }
   return addr_found_in_stream_at_position;
}

void insertNewPrefetchStreamValues(Prefetch **prefetch, int num_of_streams, int size, uint32_t address){
   int i;
   for (i = 0; i < num_of_streams; i++)
   {
      if (prefetch[i]->lru == (num_of_streams - 1))
      {
         updatePrefetchStreamValues(prefetch[i], size, address, -1);
         prefetch[i]->lru = 0;
      }
      else
      {
         prefetch[i]->lru += 1;
      }
   }
}

int main(int argc, char *argv[])
{
   FILE *fp;              // File pointer.
   char *trace_file;      // This variable holds the trace file name.
   cache_params_t params; // Look at the sim.h header file for the definition of struct cache_params_t.
   char rw;               // This variable holds the request's type (read or write) obtained from the trace.
   uint32_t addr;         // This variable holds the request's address obtained from the trace.
                          // The header file <inttypes.h> above defines signed and unsigned integers of various sizes in a machine-agnostic way.  "uint32_t" is an unsigned integer of 32 bits.
   bool is_block_dirty;
   bool cache1_writeback_complete;
   bool cache2_writeback_complete;
   int addr_found_in_stream_at_position;
   uint32_t dirty_block_tag_value;
   uint32_t dirty_block_index_value;
   uint32_t address_for_prefetch_with_cache1;
   uint32_t address_for_prefetch_with_cache2;
   float cache1_miss_rate = 0;
   float cache2_miss_rate = 0;
   int memory_traffic = 0;
   bool is_cache2_present = 0;

   // Exit with an error if the number of command-line arguments is incorrect.
   if (argc != 9)
   {
      printf("Error: Expected 8 command-line arguments but was provided %d.\n", (argc - 1));
      exit(EXIT_FAILURE);
   }

   // "atoi()" (included by <stdlib.h>) converts a string (char *) to an integer (int).
   params.BLOCKSIZE = (uint32_t)atoi(argv[1]);
   params.L1_SIZE = (uint32_t)atoi(argv[2]);
   params.L1_ASSOC = (uint32_t)atoi(argv[3]);
   params.L2_SIZE = (uint32_t)atoi(argv[4]);
   params.L2_ASSOC = (uint32_t)atoi(argv[5]);
   params.PREF_N = (uint32_t)atoi(argv[6]);
   params.PREF_M = (uint32_t)atoi(argv[7]);
   trace_file = argv[8];

   // Open the trace file for reading.
   fp = fopen(trace_file, "r");
   if (fp == (FILE *)NULL)
   {
      // Exit with an error if file open failed.
      printf("Error: Unable to open file %s\n", trace_file);
      exit(EXIT_FAILURE);
   }

   // Print simulator configuration.
   printf("===== Simulator configuration =====\n");
   printf("BLOCKSIZE:  %u\n", params.BLOCKSIZE);
   printf("L1_SIZE:    %u\n", params.L1_SIZE);
   printf("L1_ASSOC:   %u\n", params.L1_ASSOC);
   printf("L2_SIZE:    %u\n", params.L2_SIZE);
   printf("L2_ASSOC:   %u\n", params.L2_ASSOC);
   printf("PREF_N:     %u\n", params.PREF_N);
   printf("PREF_M:     %u\n", params.PREF_M);
   printf("trace_file: %s\n", trace_file);

   Address cache1Address(params.L1_SIZE, params.L1_ASSOC, params.BLOCKSIZE); // initialise cache1
   Cache **cache1 = new Cache *[cache1Address.num_of_sets];
   for (i = 0; i < cache1Address.num_of_sets; i++)
   {
      cache1[i] = new Cache[params.L1_ASSOC];
      resetSetAddress(cache1[i], params.L1_ASSOC);
   }

   Address cache2Address(params.L2_SIZE, params.L2_ASSOC, params.BLOCKSIZE); // initialise cache2
   Cache **cache2 = new Cache *[cache2Address.num_of_sets];
   for (i = 0; i < cache2Address.num_of_sets; i++)
   {
      cache2[i] = new Cache[params.L2_ASSOC];
      resetSetAddress(cache2[i], params.L2_ASSOC);
   }

   Prefetch **prefetch = new Prefetch *[params.PREF_N]; // initialise prefetch
   for (i = 0; i < params.PREF_N; i++)
   {
      prefetch[i] = new Prefetch[params.PREF_M];
      prefetch[i]->lru = i;
      prefetch[i]->address = 0;
   }

   if (params.PREF_M != 0 && params.PREF_N != 0)
   {
      is_prefetch_present = 1;
   }

   if ((params.L2_SIZE != 0) && (params.L2_ASSOC != 0))
   {
      is_cache2_present = 1;
      is_prefetch_for_cache1 = 0;
   }

   // Read requests from the trace file and echo them back.
   while (fscanf(fp, "%c %x\n", &rw, &addr) == 2)
   { // Stay in the loop if fscanf() successfully parsed two tokens as specified.
      // creating cache1
      if (rw == 'r')
      {
         // printf("r %x\n", addr);
         num_cache1_reads++;
      }
      else if (rw == 'w')
      {
         // printf("w %x\n", addr);
         num_cache1_writes++;
      }
      else
      {
         printf("Error: Unknown request type %c.\n", rw);
         exit(EXIT_FAILURE);
      }

      ///////////////////////////////////////////////////////
      // Issue the request to the L1 cache instance here.
      ///////////////////////////////////////////////////////
      addr_found_in_stream_at_position = -1;
      is_cache1_hit = 0;
      is_cache2_hit = 0;
      cache1_writeback_complete = 0;
      cache2_writeback_complete = 0;
      tag_value_for_cache1 = getTagBits(addr, cache1Address.num_of_offset_bits, cache1Address.num_of_index_bits);
      index_value_for_cache1 = getIndexBits(addr, cache1Address.num_of_offset_bits, cache1Address.num_of_index_bits, cache1Address.num_of_tag_bits);
      tag_value_for_cache2 = getTagBits(addr, cache2Address.num_of_offset_bits, cache2Address.num_of_index_bits);
      index_value_for_cache2 = getIndexBits(addr, cache2Address.num_of_offset_bits, cache2Address.num_of_index_bits, cache2Address.num_of_tag_bits);
      if (!is_cache2_present && is_prefetch_present && is_prefetch_for_cache1)
      {
         address_for_prefetch_with_cache1 = addr >> cache1Address.num_of_offset_bits;
      }
      else
      {
         address_for_prefetch_with_cache2 = addr >> cache2Address.num_of_offset_bits;
      }

      for (i = 0; i < params.L1_ASSOC; i++) // checking cache1 for hit
      {
         if (cache1[index_value_for_cache1][i].tag == tag_value_for_cache1)
         {
            cache_hit_update_lru(cache1[index_value_for_cache1], i, params.L1_ASSOC); // cache1 is hit
            is_cache1_hit = 1;
            if (rw == 'w')
            {
               cache1[index_value_for_cache1][i].dirty = 1;
            }
            if (is_prefetch_for_cache1 && is_prefetch_present) // prefetch when cache1 is missed
            {
               updatePrefetch(prefetch, params.PREF_N, params.PREF_M, address_for_prefetch_with_cache1);
            }
            break;
         }
      }

      if (is_cache1_hit == 0) // increase cache 1 read and write misses
      {
         if (is_cache2_present)
         {
            num_cache2_reads++;
         }
         if (is_prefetch_for_cache1 && is_prefetch_present) // prefetch when cache1 is missed
         {
            addr_found_in_stream_at_position = updatePrefetch(prefetch, params.PREF_N, params.PREF_M, address_for_prefetch_with_cache1);
            if (addr_found_in_stream_at_position == -1)
            {
               insertNewPrefetchStreamValues(prefetch, params.PREF_N, params.PREF_M, address_for_prefetch_with_cache1);
            }
         }

         if ((rw == 'r') && (addr_found_in_stream_at_position == -1))
         {
            num_cache1_read_misses++;
         }
         else if ((rw == 'w') && (addr_found_in_stream_at_position == -1))
         {
            num_cache1_write_misses++;
         }
      }

      if (is_cache1_hit == 0) // c1 to c2 writeback operation
      {
         for (i = 0; i < params.L1_ASSOC; i++)
         {
            if (cache1[index_value_for_cache1][i].lru == (params.L1_ASSOC - 1))
            {
               if (cache1[index_value_for_cache1][i].dirty == 1)
               {
                  if (is_cache2_present == 1)
                  {
                     dirty_block_tag_value = getTagForCache2(cache1[index_value_for_cache1][i].tag, index_value_for_cache1, cache1Address.num_of_index_bits, cache2Address.num_of_index_bits);
                     dirty_block_index_value = getIndexForCache2(cache1[index_value_for_cache1][i].tag, index_value_for_cache1, cache1Address.num_of_index_bits, cache2Address.num_of_index_bits, cache2Address.num_of_offset_bits, cache2Address.num_of_tag_bits);

                     for (j = 0; j < params.L2_ASSOC; j++)
                     {
                        if (dirty_block_tag_value == cache2[dirty_block_index_value][j].tag)
                        {
                           cache2[dirty_block_index_value][j].dirty = 1;
                           cache_hit_update_lru(cache2[dirty_block_index_value], j, params.L2_ASSOC);
                           cache2_writeback_complete = 1;
                        }
                     }
                     if (cache2_writeback_complete == 0)
                     {
                        num_cache2_write_misses++;
                        for (j = 0; j < params.L2_ASSOC; j++)
                        {
                           if (cache2[dirty_block_index_value][j].lru == (params.L2_ASSOC - 1))
                           {
                              if (cache2[dirty_block_index_value][j].dirty == 1)
                              {
                                 num_cache2_writebacks++;
                              }
                              cache2[dirty_block_index_value][j].dirty = 1;
                              cache2[dirty_block_index_value][j].tag = dirty_block_tag_value;
                              cache_hit_update_lru(cache2[dirty_block_index_value], j, params.L2_ASSOC);
                           }
                        }
                     }
                     num_cache2_writes++;
                  }
                  num_cache1_writebacks++;
               }
               break;
            }
         }
      }

      if ((is_cache1_hit == 0) && (is_cache2_present == 1)) // checking cache2 for hit
      {
         for (i = 0; i < params.L2_ASSOC; i++)
         {
            if (cache2[index_value_for_cache2][i].tag == tag_value_for_cache2)
            {
               cache_hit_update_lru(cache2[index_value_for_cache2], i, params.L2_ASSOC); // cache2 is hit
               is_cache2_hit = 1;
               break;
            }
         }
      }

      if ((is_cache1_hit == 0) && (is_cache2_hit == 0) && (is_cache2_present == 1)) // c2 writeback operation
      {
         num_cache2_read_misses++;
         for (i = 0; i < params.L2_ASSOC; i++)
         {
            if (cache2[index_value_for_cache2][i].lru == (params.L2_ASSOC - 1))
            {
               if (cache2[index_value_for_cache2][i].dirty == 1)
               {
                  num_cache2_writebacks++;
                  break;
               }
            }
         }
         cache_write(cache2[index_value_for_cache2], params.L2_ASSOC, tag_value_for_cache2, 0);
      }

      if (is_cache1_hit == 0) // c1 write operation
      {
         is_block_dirty = update_dirty_bit(rw);
         cache_write(cache1[index_value_for_cache1], params.L1_ASSOC, tag_value_for_cache1, is_block_dirty);
      }

      // printf("%x\n", address_for_prefetch_with_cache1);
   }

   if (is_cache2_present)
   {
      memory_traffic = num_cache2_read_misses + num_cache2_write_misses + num_cache2_writebacks + num_cache2_prefetches;
      cache2_miss_rate = ((float)num_cache2_read_misses + (float)num_cache2_write_misses) / ((float)num_cache2_reads);
   }
   else
   {
      memory_traffic = num_cache1_read_misses + num_cache1_write_misses + num_cache1_writebacks + num_cache1_prefetches;
   }
   cache1_miss_rate = ((float)num_cache1_read_misses + (float)num_cache1_write_misses) / ((float)num_cache1_reads + (float)num_cache1_writes);

   printf("\n ===== L1 contents =====\n");
   for (i = 0; i < cache1Address.num_of_sets; i++) // printing L1 contents
   {
      printf("set\t%d:\t", i);
      for (j = 0; j < params.L1_ASSOC; j++)
      {
         for (int k = 0; k < params.L1_ASSOC; k++)
         {
            if (cache1[i][k].lru == j)
            {
               printf("%x ", cache1[i][k]);
               if (cache1[i][k].dirty)
               {
                  printf("D\t");
               }
               break;
            }
         }
      }
      printf("\n");
   }

   if (is_cache2_present) // printing L2 contents
   {
      printf("\n ===== L2 contents =====\n");
      for (i = 0; i < cache2Address.num_of_sets; i++)
      {
         printf("set\t%d:\t", i);
         for (j = 0; j < params.L2_ASSOC; j++)
         {
            for (int k = 0; k < params.L2_ASSOC; k++)
            {
               if (cache2[i][k].lru == j)
               {
                  printf("%x\t", cache2[i][k]);
                  if (cache2[i][k].dirty)
                  {
                     printf("D\t");
                  }
                  break;
               }
            }
         }
         printf("\n");
      }
   }

   if(is_prefetch_present){
      printf("\n===== Stream Buffer(s) contents =====\n");
      for (i = 0; i < params.PREF_N; i++)
      {
         for (j = 0; j < params.PREF_N; j++)
         {
            if(prefetch[j]->lru == i)
            {
               for (int k = 0; k < params.PREF_M ; k++)
               {
                  printf("%x\t", prefetch[i][k]);
               }
               printf("\n");
               break;
            }
         }
      }
   }

   printf("\n===== Measurements =====\n");
   printf("a. L1 reads:\t%d\n", num_cache1_reads);
   printf("b. L1 read misses:\t%d\n", num_cache1_read_misses);
   printf("c. L1 writes:\t%d\n", num_cache1_writes);
   printf("d. L1 write misses:\t%d\n", num_cache1_write_misses);
   printf("e. L1 miss rate:\t%.4f\n", cache1_miss_rate);
   printf("f. L1 writebacks:\t%d\n", num_cache1_writebacks);
   printf("g. L1 prefetches:\t%d\n", num_cache1_prefetches);
   printf("h. L2 reads (demand):\t%d\n", num_cache2_reads);
   printf("i. L2 read misses (demand):\t%d\n", num_cache2_read_misses);
   printf("j. L2 reads (prefetch):\t%d\n", num_cache2_prefetch_reads);
   printf("k. L2 read misses (prefetch):\t%d\n", num_cache2_prefetch_read_misses);
   printf("l. L2 writes:\t%d\n", num_cache2_writes);
   printf("m. L2 write misses:\t%d\n", num_cache2_write_misses);
   printf("n. L2 miss rate:\t%.4f\n", cache2_miss_rate);
   printf("o. L2 writebacks:\t%d\n", num_cache2_writebacks);
   printf("p. L2 prefetches:\t%d\n", num_cache2_prefetches);
   printf("q. memory traffic:\t%d \n", memory_traffic);
   return (0);
}