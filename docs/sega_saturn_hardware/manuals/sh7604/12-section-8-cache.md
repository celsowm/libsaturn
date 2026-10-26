# Section 8 Cache

SH7604 Hardware Manual — Hitachi ADE-602-085C, revision 4.0, September 19, 1995.

[Manual index](README.md) · [Source PDF](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=229)

Physical PDF pages 229–246. Text extraction; consult the PDF for figures.

## Bookmarks

- [8.1 Introduction](#pdf-page-229)
- [8.2 Cache Control Register (CCR)](#pdf-page-230)
- [8.3 Address Space and the Cache](#pdf-page-232)
- [8.4 Cache Operation](#pdf-page-232)
- [8.4.1 Cache Reads](#pdf-page-232)
- [8.4.2 Write Access](#pdf-page-235)
- [8.4.3 Cache-Through Access](#pdf-page-236)
- [8.4.4 The TAS Instruction](#pdf-page-237)
- [8.4.5 Pseudo-LRU and Cache Replacement](#pdf-page-238)
- [8.4.6 Cache Initialization](#pdf-page-240)
- [8.4.7 Associative Purges](#pdf-page-240)
- [8.4.8 Data Array Access](#pdf-page-240)
- [8.4.9 Address Array Access](#pdf-page-241)
- [8.5 Cache Use](#pdf-page-242)
- [8.5.1 Initialization](#pdf-page-242)
- [8.5.2 Purge of Specific Lines](#pdf-page-243)
- [8.5.3 Cache Data Coherency](#pdf-page-243)
- [8.5.4 Two-Way Cache Mode](#pdf-page-244)
- [8.5.5 Usage Notes](#pdf-page-245)

## PDF page 229

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=229)

```text
                                           Section 8   Cache


8.1        Introduction


                       Cache address array             Cache data array


                            Cache                                                Cache
                     address bus                                                 data bus          Way 0

                    LRU              Tag                                                              Way 1
                information                                           Data (16 bytes/line)addressV       Way 2
                                                                                                            Way 3


         64
    entries











                          Tag address                Internal                         Internal
                          match signal               address bus                      data bus

V: Valid bit

                                      Figure 8.1   Cache Configuration

The SH7604 incorporates 4 kbytes of 4-way cache memory of mixed instruction/data type. The
SH7604 can also be used as 2-kbyte RAM and 2-kbyte cache memory (mixed instruction/data
type) by a setting in the cache control register CCR (two-way cache mode). CCR can specify that
either instructions or data do not use cache.

Each line of cache memory consists of 16 bytes. Cache memory is always updated in line units.
Four 32-bit accesses are required to update a line. Since the number of entries is 64, the six bits
(A9 to A4) in each address determine the entry. A four-way set associative configuration is used,
so up to four different instructions/data can be stored in the cache even when entry addresses
match. To efficiently use four ways having the same entry address, replacement is provided based
on a pseudo-LRU (least-recently used) replacement algorithm.




                                                                                                                   213
```

## PDF page 230

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=230)

```text
                31          28                                                                   9                     30
   Address
                31964

                Access space                             Tag address                     Entry address
           specification address                                                                            Byte address
                                                                                                                 in line

                                                   Figure 8.2   Address

8.2        Cache Control Register (CCR)

Table 8.1      Cache Control Register

Name                                         Abbrev.                  R/W       Initial Value            Address
Cache control register               CCR                        R/W       H'00                        H'FFFFFE92


The cache control register (CCR) is used for cache control. CCR must be set and the cache must
be initialized before use.

                    Bit:         7              6              5              4              3              2              1              0
            Bit name:       W1           W0           —            CP           TW          OD           ID           CE
        Initial value:         0              0              0              0              0              0              0              0
                  R/W:      R/W         R/W           R           R/W         R/W         R/W         R/W         R/W


•    Bits 7 and 6—Way Specification (W1 to W0): W1 and W0 specify the way when an address
    array is directly accessed by address specification.

Bit 7:  W1              Bit 6:  W0              Description
0                            0                            Way 0                                                                 (Initial value)
                         1                            Way 1
1                            0                            Way 2
                         1                            Way 3


•    Bit 5—Reserved: This bit always reads 0. The write value should always be 0.

•    Bit 4—Cache Purge (CP): CP is a cache purge bit. When 1 is written to CP, all cache entries
    and all valid bits and LRU bits of the way are initialized to 0. After initialization is complete,
    the CP bit reverts to 0. The CP bit always reads 0.




214
```

## PDF page 231

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=231)

```text
Bit 4:  CP              Description
0                            Normal operation                                                                              (Initial value)
1                            Cache purge
Note:   Always read 0.


•    Bit 3—Two-Way Mode (TW): TW is the two-way mode bit. The cache operates as a four-way
    set associative cache when TW is 0 and as a two-way set associative cache and 2-kbyte RAM
    when TW is 1. In the two-way mode, ways 2 and 3 are cache and ways 0 and 1 are RAM.
    Ways 0 and 1 are read or written by direct access of the data array according to address space
    specification.

Bit 3:  TW              Description
0                            Four-way mode                                                                                 (Initial value)
1                            Two-way mode


•    Bit 2—Data Replacement Disable (OD): OD is the bit for disabling data replacement. When
    this bit is 1, data fetched from external memory is not written to the cache even if there is a
    cache miss. Cache data is, however, read or updated during cache hits. OD is valid only when
    CE is 1.

Bit 2:  OD              Description
0                            Normal operation                                                                              (Initial value)
1                            Data not replaced even when cache miss occurs in data access


•    Bit 1—Instruction Replacement Disable (ID): ID is the bit for disabling instruction
    replacement. When this bit is 1, an instruction fetched from external memory is not written to
    the cache even if there is a cache miss. Cache data is, however, read or updated during cache
    hits. ID is valid only when CE is 1.

Bit 1:  ID                Description
0                            Normal operation                                                                              (Initial value)
1                            Data not replaced even when cache miss occurs in instruction fetch


•    Bit 0—Cache Enable (CE): CE is the cache enable bit. Cache can be used when CE is set to 1.

Bit 0:  CE              Description
0                            Cache disabled                                                                                 (Initial value)
1                            Cache enabled



                                                                                                                                     215
```

## PDF page 232

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=232)

```text
8.3        Address Space and the Cache

The address space is divided into six partitions. The cache access operation is specified by
addresses. Table 8.2 lists the partitions and their cache operations. For more information on
address spaces, see section 7, Bus State Controller. Note that the spaces of the cache area and
cache-through area are the same.

Table 8.2      Address Space and Cache Operation

Addresses
A31–A29      Partition                                    Cache Operation
000                Cache area                                Cache is used when the CE bit in CCR is 1.
001                Cache-through area                  Cache is not used.
010                Associative purge area              Cache line of the specified address is purged
                                                         (disabled).
011                Address array read/write area   Cache address array is accessed directly.
110                Data array read/write area         Cache data array is accessed directly.
111                I/O area                                     Cache is not used.


8.4        Cache Operation


8.4.1        Cache Reads

This section describes cache operation when the cache is enabled and data is read from the CPU.
One of the 64 entries is selected by the entry address part of the address output from the CPU on
the cache address bus. The tag addresses of ways 0 through 3 are compared to the tag address parts
of the addresses output from the CPU. A match to the tag address of a way is called a cache hit. In
proper use, the tag addresses of each way differ from each other, and the tag address of only one
way will match. When none of the way tag addresses match, it is called a cache miss. Tag
addresses of entries with valid bits of 0 will not match in any case.

When a cache hit occurs, data is read from the data array of the way that was matched according to
the entry address, the byte address within the line, and the access data size. The data is then sent to
the CPU. The address output on the cache address bus is calculated in the CPU’s instruction
execution phase and the results of the read are written during the CPU’s write-back stage. The
cache address bus and cache data bus both operate as pipelines in concert with the CPU’s pipeline
structure. From address comparison to data read requires 1 cycle; since the address and data
operate as a pipeline, consecutive reads can be performed at each cycle with no waits.





216
```

## PDF page 233

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=233)

```text
                  CPU pipeline stage               EX              MA             WB
                                                                   EX              MA
                                                                                   EX

                  Cache address bus                    Address A        Address B


                                                            Cache tag comparison

                      Cache data bus                                    Address A       Address B

                                                                                 Data array read

    EX: Instruction execution
    MA: Memory access
    WB: Write-back
                             Figure 8.3   Read Access in Case of a Cache Hit

When a cache miss occurs, the way for replacement is determined using the LRU information, and
the read address from the CPU is written in the address array for that way. Simultaneously, the
valid bit is set to 1. Since the 16 bytes of data for replacing the data array are simultaneously read,
the address on the cache address bus is output to the internal address bus and 4 longwords are read
consecutively. Access starts with whatever address output to the internal address bus will make the
longword that contains the address to be read from the cache come last as the byte address within
the line as the order + 4. The data read on the internal data bus is written sequentially to the cache
data array. When the last data is written to the cache data array, it is simultaneously written to the
cache data bus and the read data is sent to the CPU.

The internal address bus and internal data bus also function as pipelines, just like the cache bus.


















                                                                                                                     217
```

## PDF page 234

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=234)

```text
        CPU
     pipeline         EX             MA                                                                        WB
        stage                         EX                                                                        MA

      Cache
    address              Address A                                Address B
          bus
      Cache                       Cache tag comparison                                     Cache tag comparison
    data bus                                                   Data array write                     Address A

     Internal
    address                              Address A + 4  Address A + 8  Address A + 12   Address A
          bus
     Internal
    data bus                                            Address A + 4  Address A + 8  Address A + 12       Address A


EX: Instruction execution
MA: Memory access
WB: Write-back

                              Figure 8.4   Read Access in Case of a Cache Miss

































218
```

## PDF page 235

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=235)

```text
8.4.2        Write Access

This cache is of the write-through type, and writing to external memory is performed regardless of
whether or not there is a cache hit. The write address output to the cache address bus is used to
compare to the tag address of the cache’s address array. When they match, the write data output to
the cache data bus in the following cycle is written to the data array. When they do not match,
nothing is written to the cache data array. The write address is output to the internal address bus 1
cycle later than the cache address bus. The write data is similarly output to the internal data bus 1
cycle later than the cache data bus. The CPU waits until the writes onto the internal bus are
completed.






           CPU pipeline stage                EX             MA
                                                             EX                    MA

           Cache address bus                    Address A        Address B

                                                     Cache tag comparison
               Cache data bus                                    Address A                   Address B

                                                                 Data array write
         Internal address bus                                    Address A                   Address B


              Internal data bus                                                              Address A

EX: Instruction execution
MA: Memory access

                                            Figure 8.5   Write Access


















                                                                                                                     219
```

## PDF page 236

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=236)

```text
8.4.3        Cache-Through Access

When reading or writing a cache-through area, the cache is not accessed. Instead, the cache
address value is output to the internal address bus. For read operations, the read data output to the
internal address bus is fetched and output to the cache data bus. The read of the cache-through area
is only performed on the address in question. For write operations, the write data on the cache data
bus is output to the internal data bus. Writes on the cache through area are compared to the address
tag; except for the fact that nothing is written to the data array, the operation is the same as the
write shown in figure 8.5.





                                CPU           EX                   MA                   WB
                     pipeline stage                                EX                   MA

                              Cache              Address A      Address B
                       address bus

                              Cache                                          Address A
                            data bus

                             Internal                          Address A
                       address bus

                             Internal                                        Address A
                            data bus


EX: Instruction execution
MA: Memory access
WB: Write-back

                                Figure 8.6   Reading Cache-Through Areas





















220
```

## PDF page 237

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=237)

```text
8.4.4        The TAS Instruction

The TAS instruction reads data from memory, compares it to 0, and reflects the result in the T bit
of the status register while setting the most significant bit to 1. It is an address for writing to the
same address. Reads from memory become cache-through operations even when the cache area is
accessed. Address tags are not compared. The updated value is written to memory through the
internal data bus, but before that the address tag is compared and if there are any matching entries,
a write is performed to the corresponding data array.




        CPU
    pipeline         EX                    MA                    EX            MA
       stage
      Cache
    address              Address A                                  Address A'
         bus                                                        Cache tag comparison
      Cache                                           Address A                    Address A'
   data bus
     Internal                                                                     Data array write
    address                            Address A                                   Address A'
         bus
     Internal                                         Address A                                  Address A'
   data bus


EX: Instruction execution
MA: Memory access

                        Figure 8.7   TAS Instruction Execution and the Cache























                                                                                                                  221
```

## PDF page 238

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=238)

```text
8.4.5        Pseudo-LRU and Cache Replacement

When a cache miss occurs during a read, the data of the missed address is read from 1 line (16
bytes) of memory and replaced. This makes it important to decide which of the ways to replace. It
is likely that the way least recently used has the highest probability of being the next to be
accessed. This algorithm for replacing ways is called the least recently used replacement
algorithm, or LRU. The hardware to implement it, however, is complex. For that reason, this
cache uses a pseudo-LRU replacement algorithm that keeps track of the order of way access and
replaces the oldest way.

Six bits of data are used as the LRU information. The bits indicate the access order for 2 ways, as
shown in figure 8.8. When the value is 1, access occurred in the direction of the appropriate arrow
in the figure. The direction of the arrow can be determined by reading the bit. All the arrows show
the oldest access toward that way, which becomes the object of replacement. The access order is
recorded in the LRU information bits, so the LRU information is rewritten when a cache hit occurs
during a read, when a cache hit occurs during a write, and when replacement occurs after a cache
miss. Table 8.3 shows the rewrite values; table 8.4 shows how ways are selected for replacement.

After a cache purge by CCR’s CP bit, the LRU information is completely zeroized, so the initial
order used is way 3 ﬁ   way 2 ﬁ   way 1 ﬁ   way 0. Thereafter the way is selected according to the
order of access set by the program. Since the replacement will not be correct if the LRU gets an
inappropriate value, the address array write function can be used to rewrite. When this is done, be
sure not to write a value other than 0 as the LRU information.

When CCR’s OD bit or ID bit is 1, neither will replace the cache even if a cache miss occurs
during data read or instruction fetch. Instead of replacing, the missed address data is read and
directly transferred to the CPU.

The two-way mode of the cache set by CCR’s TW bit can only be implemented by replacing ways
2 and 3. Comparisons of tag addresses of address arrays are carried out on all four ways even in
two-way mode, so the valid bit of ways 1 and 0 must be zeroized prior to operation in the two-way
mode.

Writing for the tag address and valid bit for cache replacement does not wait for the read from
memory to be completed. When the memory access is aborted by a reset during replacement or the
like, the cache contents and memory contents may be out of sync, so always perform a purge.












222
```

## PDF page 239

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=239)

```text
                                                             Way 0

                                              Bit 5                              Bit 3
                                                                  Bit 4
                                    Way 1            Bit 1                              Way 3



                                              Bit 2                            Bit 0

                                                             Way 2

                             Figure 8.8   LRU Information and Access Sequence

Table 8.3      LRU Information after Update

                        Bit 5              Bit 4              Bit 3              Bit 2              Bit 1              Bit 0
Way 0 000———
Way 1                   1                    —                  —                  0                    0                    —
Way 2                   —                  1                    —                  1                    —                  0
Way 3                   —                  —                  1                    —                  1                    1
—: Holds the value before update.


Table 8.4      Selection Conditions for Replaced Way

                        Bit 5              Bit 4              Bit 3              Bit 2              Bit 1              Bit 0
Way 0 111———
Way 1                   0                    —                  —                  1                    1                    —
Way 2                   —                  0                    —                  0                    —                  1
Way 3                   —                  —                  0                    —                  0                    0
—: Don’t care.

















                                                                                                                                223
```

## PDF page 240

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=240)

```text
8.4.6        Cache Initialization

Purges of the entire cache area can only be carried out by writing 0 to the CP bit in CCR. Writing
1 to the CP bit initializes the valid bit of the address array and all bits of the LRU information to 0.
Cache purges are completed in 1 cycle, but additional time is required for writing to CCR. Always
initialize the valid bit and LRU before enabling the cache.

When the cache is enabled, instruction reads are performed from the cache even during writing to
CCR. This means that the prefetched instructions are read from the cache. To do a proper purge,
write 0 to CCR’s CE bit, then disable the cache and purge. Since CCR’s CE bit is cleared to 0 by a
power-on reset or manual reset, the cache can be purged immediately by a reset.


8.4.7        Associative Purges

Associative purges invalidate 1 line (16 bytes) corresponding to specific address contents when
the contents are in the cache. When the contents of shared addresses are rewritten by one CPU in a
multiprocessor configuration, the other CPU cache must be invalidated if it also contains the
address. When writing is performed to the address found by adding H'40000000 to the purged
address, the valid bit of the entry storing the address prior to the addition is initialized to 0. 16
bytes are purged in each write, so a purge of 256 bytes of consecutive areas can be accomplished
in 16 writes. Access sizes when associative purges are performed should be longword. A purge of
1 line requires 2 cycles.


    Associative purge:
                       31                                                                               9                 3     028
            Address                                                                                          Entry010Tag addressaddress      —
                        3419                                       6

                                   Figure 8.9   Associative Purge Access


8.4.8        Data Array Access

The cache data array can be read or written directly via the data array read/write area. The access
sizes for the data array may be byte, word or longword. Data array accesses are completed in 1
cycle for both reads and writes. Since only the cache bus is used, the operation can proceed in
parallel even when another master, such as the DMAC, is using the bus. The data array of way 0 is
mapped on H'C0000000 to H'C00003FF, way 1 on H'C0000400 to H'C00007FF, way 2 on
H'C0000800 to H'C0000BFF and way 3 on H'C0000C00 to H'C0000FFF. When the two-way
mode is being used, the area H'C0000000 to H'C00007FF is accessed as 2 kbytes of on-chip
RAM. When the cache is disabled, the area H'C0000000 to H'C0000FFF can be used as 4 kbytes
of on-chip RAM.




224
```

## PDF page 241

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=241)

```text
When the contents of the way being used as cache is rewritten using a data array access, the
contents of external memory and cache will not match, so this method should be avoided.


    Data array read/write:

                 31       28                                                                    9               3        0
    Address                                                                                       Entry110Tag addressWaddress      BA
                 3                                                                                                     4196

                                   31                                                                                                         0
                           Data                                                Data
                                                                                 32

    BA:  Byte address within line      W:  Way specification
                                            Figure 8.10   Data Array Access


8.4.9        Address Array Access

The address array of the cache can be accessed so that the contents fetched to the cache can be
checked for purposes of program debugging or the like. The address array is mapped on
H'60000000 to H'600003FF. Since all of the ways are mapped to the same addresses, ways are
selected by rewriting the W1 and W0 bits in CCR. The address array can only be accessed in
longwords.

When the address array is read, the tag address, LRU information, and valid bit are output as data.
When the address array is written to, the tag address and valid bit are written from the cache
address bus. This requires that the write address be calculated according to the value to be written,
then written. LRU information is written from data, but 0 should always be written to prevent
malfunctions.




















                                                                                                                                 225
```

## PDF page 242

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=242)

```text
   Address array read:

            31       28                                                                    9                 3     0
  Address                                                                                          Entry011—address      —
            3419                                       6
                         31                                                                               9             3  2      0281
                   Data                                                                                          LRU            ——Tag address—V
                         3119                                       6                    12

   Address array write:
            31       28                                                                    932 01
  Address                                                                                       Entry011Tag addressaddress           ——V
            3119                                       6                   12

                         31                                                                               9                 3     0
                   Data                              —LRU
                                                                            information    —
                                                     22                                       6              4

   V:  Valid bit
                              Figure 8.11   Address Array Access

8.5        Cache Use


8.5.1        Initialization

Cache memory is not initialized in a reset. Therefore, the cache must be initialized by software
before use. Cache initialization clears (to 0) the address array valid bit and all LRU information.
The address array write function can be used to initialize each line, but it is simpler to initialize it
once by writing 1 to the CP bit in CCR. Figure 8.12 shows how to initialize the cache.


     MOV.W  #H'FE92, R1
     MOV.B  @R1, R0     ;
     AND    #H'FE, R0   ;
     MOV.B  #R0, @R1    ; Cache disable
     OR     #H'10, R0
     MOV.B  R0, @R1     ; Cache purge
     OR     #H'01, R0
     MOV.B  R0, R1      ; Cache enable

                               Figure 8.12   Cache Initialization
226
```

## PDF page 243

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=243)

```text
8.5.2        Purge of Specific Lines

Since the SH7604 has no snoop function (for monitoring data rewrites), specific lines of cache
must be purged when the contents of cache memory and external memory differ as a result of an
operation. For instance, when a DMA transfer is performed to the cache area, cache lines
corresponding to the rewritten address area must be purged. All entries of the cache can be purged
by setting the CP bit in CCR to 1. However, it is efficient to purge only specific lines if only a
limited number of entries are to be purged.

An associative purge is used to purge specific lines. Since cache lines are 16 bytes long, purges are
performed in a 16-byte units. The four ways are checked simultaneously, and only lines holding
data corresponding to specified addresses are purged. When addresses do not match, the data at
the specified address is not fetched to the cache, so no purge occurs.


; Purging 32 bytes from address R3
MOV.L      #H'40000000, R0
XOR        R1, R1
MOV.L      R1, @(R0, R3)
ADD        #16, R3
MOV.L      R1, @(R0, R3)

                                  Figure 8.13   Purging Specific Addresses

When it is troublesome to purge the cache after every DMA transfer, it is recommended that the
OD bit in CCR be set to 1 in advance. When the OD bit is 1, the cache operates as cache memory
only for instructions. However, when data is already fetched into cache memory, specific lines of
cache memory must be purged for DMA transfers.


8.5.3        Cache Data Coherency

The SH7604’s cache memory does not have a snoop function. This means that when data is shared
with a bus master other than the CPU, software must be used to ensure the coherency of data. For
this purpose, the cache-through area can be used, the break function can be used in external bus
cycles, or a cache purge can be performed with program logic.

If the cache-through area is to be used, the data shared by the bus masters is placed in the cache-
through area. This makes it easy to maintain data coherency, since access of the cache-through
area does not fetch data into the cache. When the shared data is accessed repeatedly and the
frequency of data rewrites is low, a lower access speed can adversely affect performance.

To use the external bus cycle break function, the user break controller is used. Set the user break
controller to generate an interrupt when a write cycle is detected to any of the areas that have
shared data. The interrupt handling routine purges the cache. Since the cache is purged whenever a

                                                                                                                   227
```

## PDF page 244

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=244)

```text
rewrite is detected, data coherency can be maintained. When data that extends over multiple
words, such as a structure, is rewritten, however, interrupts are generated at the rewrites, which
can lower performance. This method is most appropriate for cases in which it is difficult to predict
and detect the timing of data updates and the update frequency is low.

To purge the cache using program logic, the data updates are detected by the program flow and the
cache is then purged. For example, if the program inputs data from a disk, whenever reading of a
unit (such as a sector) is completed, the buffer address used for reading or the entire cache is
purged, thereby maintaining coherency. When data is to be handled between two processors, only
flags to provide mutual notification of completion of data preparation or completion of a fetch are
placed in the cache-through area. The data actually transferred is placed in the cache area and the
cache is purged before the first data read to maintain the coherency of the data. When semaphores
are used as the means of communication, data coherency can be maintained even when the cache
is not purged by utilizing the TAS instruction. The TAS instruction is not read within the cache;
the external access is always direct. This means that data can be synchronized with other masters
when it is read.

When the update unit it is small, specific addresses can be purged, so only the relevant addresses
are purged. When the update unit is larger, it is faster to purge the entire cache rather than purging
all the addresses in order, and then read in the data previously existing in the cache again from
external memory.


8.5.4        Two-Way Cache Mode

The 4-kbyte cache can be used as 2-kbyte RAM and 2-kbyte mixed instruction/data cache memory
by setting the TW bit in CCR to 1. Ways 2 and 3 become cache, and ways 0 and 1 become RAM.

The cache and RAM are initialized by setting the CP bit in CCR to 1. The valid bit and LRU bits
are cleared to 0.

When the initial values of the LRU information are set to 0, ways 3 and 2 are initially used, in that
order. Ways 3 and 2 are subsequently selected for replacement as specified by the LRU
information. The conditions for updating the LRU information are the same as for four-way mode,
except that the number of ways is two.

When designated as 2-kbyte RAM, ways 0 and 1 are accessed by data array access. Figure 8.14
shows the address mapping.










228
```

## PDF page 245

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=245)

```text
                              H'00000000





                              H'C0000000
                                                              Way 0
                              H'C00003FF
                              H'C0000400
                                                              Way 1
                              H'C00007FF





                             H'FFFFFFFF

             Figure 8.14   Address Mapping of 2-kbyte RAM in the Two-Way Mode


8.5.5        Usage Notes

Standby: Disable the cache before entering the standby mode for power-down operation. After
returning from power-down, initialize the cache before use.

Cache Control Register: Changing the contents of CCR also changes cache operation. The
SH7604 makes full use of pipeline operations, so it is difficult to synchronize access. For this
reason, change the contents of the cache control register while disabling the cache or after the
cache is disabled.
























                                                                                                                229
```

## PDF page 246

[Original page](https://antime.kapsi.fi/sega/files/sh7604.pdf#page=246)

```text
230
```
