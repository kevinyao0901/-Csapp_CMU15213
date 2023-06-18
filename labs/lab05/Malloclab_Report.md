# MallocLab Report
### Name:KevinYao
### StudentId:10224507041

## 1.实验目的

在这个实验中，需要编写一个C程序的动态存储分配器，实现malloc，free和realloc函数。需要Explicit Free Lists（显示空闲链表）数据结构来实现动态存储分配器。

## 2.实验要求

仅需要修改mm.c文件并在其中完善这些函数。

动态存储分配器将由以下四个函数组成，这些函数在mm.h中声明并在mm.c中定义。
int mm_init(void);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
void *mm_realloc(void *ptr, size_t size);

具体函数功能：

• mm_init：在调用mm_malloc、mm_realloc或mm_free之前，应用程序（即用于评估您实现的追踪驱动程序）调用
mm_init执行任何必要的初始化，例如分配初始堆区域。如果在执行初始化时出现问题，则返回值应为-1，否则为0。

• mm_malloc: mm malloc例程返回一个指向至少size字节的分配块有效负载的指针。

• mm_free: mm free例程释放ptr指向的块。它不返回任何内容。此例程仅在传递的指针（ptr）是先前调用mm_malloc或mm_realloc返回且尚未释放时才能保证工作。

• mm_realloc: mm_realloc例程返回一个指向至少size字节的分配区域的指针

## 前期准备
在CsApp课本上599页给出了如下在构建隐式空闲链表时用到的宏定义，在我们构建显示空闲链表时可能会用到，所以先进行解读并定义到mm.c中

对于其中每一个函数以及一些为了构建显示空闲链表而添加的定义的具体作用和工作原理做如下解释：

### 1.参数

双字（8）对齐，假设在32位地址的情况下运行,可定义如下参数：

(1)`#define ALIGNMENT 8` 假设8字节对齐

(2)`#define WSIZE 4 `单字4字节

(3)`#define DSIZE 8` 双字8字节

(4)`#define INITSIZE 16` 在添加第一个空闲块之前，空闲列表的初始大小

(5)`#define MINBLOCKSIZE 16` 空闲块的最小大小，包括 4 个字节的头部/尾部和用于前一个和后一个
空闲块两个指针的有效负载内的空间 ，假设32位地址，故最小空闲块有16字节


### 2.宏

在正式开始修改要求的四个函数前，我们针对显示空闲链表的结构定义一些宏以方便后续参数的读写，首先附上显示空闲链表中块结构：

显示空闲链表中的块结构如下：

```
    已分配的块                空闲块
 Allocated Block          Free Block
   ---------               ---------
  | HEADER  |             | HEADER  |
   ---------               ---------
  |         |             |  NEXT   |
  |         |              ---------
  | PAYLOAD |             |  PREV   |
  |         |              ---------
  |         |             |         |
   ---------              |         |
  | FOOTER  |              ---------
   ---------              | FOOTER  |
                           ---------

```

(1)`#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)` 作用是向最接近的ALIGNMENT的倍数上取整
表示对于一个给定的size，按照ALIGNMENT对齐方式进行对齐，并返回对齐后的结果。
以ALIGNMENT为8的情况为例，假设size为12，则按照上述宏定义计算得到的对齐后的结果为16。具体地，先将size和(ALIGNMENT-1)进行相加，即12+7=19。19是8的倍数，
且大于等于12，因此最小的对齐后的值就是16。最后再用~0x7将16的最后3位清零，即得到了最终的对齐后的结果16。
这种对齐方式可以保证内存分配的块都是8字节对齐的，从而提高程序的性能表现。

(2)`#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))`

(3)`#define CHUNKSIZE (1<<12)`

(4)`#define MAX(x,y) ((x)>(y)?(x):(y))`

(5)`#define PACK(size,alloc) ((size)|(alloc))`
接受两个参数 size 和 alloc，将它们合并为一个双字（8字节）大小的值，并返回该值。size 所代表的整数值作为内存块的大小，而 alloc 代表是否分配（即是否被标记为已使用）。

为了对内存中二进制数据的读写操作,由于数据为4个字节（32位地址），可以定义如下宏：

(6)`#define GET(p) (*(size_t *)(p))` 读p指针所指位置

(7)`#define PUT(p,val) (*(size_t *)(p)=(val))` 写p指针所指位置

(8)`#define GET_SIZE(p) (GET(p) & ~0x1)` 返回指针 p 所指向的内存块的大小

(9)`#define GET_ALLOC(p) (GET(p) & 0x1)` 返回指针 p 所指向的内存块是否已经分配

(10)`#define HDRP(bp) ((char *)(bp)-WSIZE)` 返回指针 bp 所指向的内存块的头部位置

(11)`#define FTRP(bp) ((char *)(bp)+GET_SIZE(HDRP(bp))-DSIZE)` 返回指针 bp 所指向的内存块的尾部位置

为了获取指针 bp 所指向的内存块的前一个和后一个内存块的位置，可以定义如下宏：

(12)`#define NEXT_BLKP(bp) ((char *)(bp)+GET_SIZE(((char *)(bp)-WSIZE)))`返回指向后面的块的块指针

(13)`#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))`返回指向前面的块的块指针

(14)`#define NEXT_FREE(bp)(*(void**)(bp))`获取当前空闲块的下一个空闲块的指针

(15)`#define PREV_FREE(bp)(*(void**)(bp+WSIZE))`获取当前空闲块的上一个空闲块的指针

### 3.函数原型

为了完成要求的四个函数，我们还需要一些辅助函数，其声明如下：

(1)`static void *extend_heap(size_t words)`  `extend_heap` -  将堆扩展给定数量的字（向上取整为最接近的偶数）

(2)`static void *find_fit(size_t size)` `find_fit` - 尝试在空闲列表中查找至少给定大小的空闲块

(3)`static void *coalesce(void *bp)` `coalesce` - 使用边界标记策略合并块 bp 周围的内存

(4)`static void place(void *bp,size_t asize)` `place` - 在给定的空闲块（由指针bp指向）中分配给定大小的内存块

(5)`static void remove_freeblock(void *bp)` `remove_freeblock` - 从空闲块链表中移除指定的空闲块。

### 4. 全局变量

 由于需要构建模拟malloc的函数，所以需要两个指针维持堆结构,并在定义时进行初始化：
(1)`static char* heap_listp=0`指向堆起始地址的指针
(2)`static char* free_listp=0`指向空闲列表第一个空闲块的指针

## 具体实现

完成了上述准备工作后，我们可以更快速的进行目标函数的构建了，首先我们构建`mm_init` 函数。

```
mm_init - 初始化堆，其示意图如下所示.
  ____________                                                    _____________
 |  PROLOGUE  |                8+ bytes or 2 ptrs                |   EPILOGUE  |
 |------------|------------|-----------|------------|------------|-------------|
 |   HEADER   |   HEADER   |        PAYLOAD         |   FOOTER   |    HEADER   |
 |------------|------------|-----------|------------|------------|-------------|
 ^            ^            ^       
 heap_listp   free_listp   bp 
 ```

### 1.mm_init - 初始化malloc.

mm_init函数初始化后，堆中应有如图上述结构，可以参考书中598页，图9-42的隐式空闲链表相关内容，但是需要根据显示空闲链表的特点进行修改，得到如图结构，
`heap_listp`始终指向模拟堆序言块的头部，而`free_listp`始终指向第一个空闲块头部，序言块和结尾块的大小均为一个字，而初始化时应至少有一个空闲块在其中
，因此确定最初需要的空间大小为`INITSIZE+MINBLOCKSIZE`。同时按照显示空闲链表的规则对空闲块的头部和脚部进行写入，就可以完成对堆的初始化。代码如下：

```
int mm_init(void)
{
    if((heap_listp=mem_sbrk(INITSIZE+MINBLOCKSIZE))==(void *)-1)
       {
           return -1;
       }
    PUT(heap_listp,PACK(MINBLOCKSIZE,1)); // 序言块
    PUT(heap_listp+WSIZE,PACK(MINBLOCKSIZE,0)); // 空闲块头部脚注 
    
    PUT(heap_listp+2*WSIZE,PACK(0,0));// 用于储存向后指的指针
    PUT(heap_listp+3*WSIZE,PACK(0,0));// 用于储存向前指的指针
    
    PUT(heap_listp+4*WSIZE,PACK(MINBLOCKSIZE,1));// 空闲块尾部脚注
    PUT(heap_listp+5*WSIZE,PACK(0,1));// 结尾块
        
    free_listp=heap_listp+WSIZE;
    
    return 0;
}
```

这里需要说明一下关于`mem_sbrk`函数的调用，由于实验要求模拟`malloc`所以无法通过`mallloc`申请内存，所以我们使用`mem_sbrk`函数，`mem_sbrk`是一种系统调用，
它使应用程序向操作系统请求在堆区分配指定数量的内存。当应用程序调用`mem_sbrk`时，操作系统会将当前的堆区域末尾地址增加相应的大小，并返回这段新增的内存的起始地址。
如果请求的内存大小不能满足，`mem_sbrk`将会返回-1或抛出异常，表示分配失败。

这样我们就完成了堆的初始化，目前为止堆中结构如上图所示，有序言块，结尾块，以及一个空闲块。

### 2.mm_malloc - 分配大小为给定值的内存块，该内存块按8字节边界对齐。。

接下来我们将要实现`mm_malloc`函数，对于这个函数，我们需要分配给定大小的内存块。

我们可以使用以下策略来分配内存块：

（1）如果找到一个足够大小的空闲块，则分配该空闲块并返回该块有效载荷的指针。让我们先假设`find_fit()`可以正常工作 ，
如果模拟堆中存在符合要求大小的空间，它将返回指向这个位置的指针否则返回NULL。若找到了符合要求大小的内存块，`place()`函数
将会在给定的空闲块（由指针bp指向）中分配出给定大小的内存块。最终返回指针即可。

（2）否则，无法找到空闲块，需要扩展堆。简单地扩展堆并将分配的块放在新的空闲块中。使用`extend_heap()`函数扩展堆的大小，再对新分配的内存进行写入即可。

```
void *mm_malloc(size_t size)
{
    if(size==0)
    {
        return NULL;
    }
    size_t asize;
    size_t extendsize;
    char* bp;
    /* 新块的大小等于头部和尾部的大小加上有效载荷的大小
   如果请求的大小更小，则为 MINBLOCKSIZE。*/
    asize=MAX(ALIGN(size)+DSIZE,MINBLOCKSIZE);
    
    // 在空闲块链表中查找合适的块
    if((bp=find_fit(asize)))
    {
        place(bp,asize);
        return bp;
    }
    
    // 否则，若没有找到合适的空闲块。需要增大堆的大小。
    extendsize=MAX(asize,MINBLOCKSIZE);
    if((bp=extend_heap(extendsize/WSIZE))==NULL)
        return NULL;
    
    // 放置新分配的内存块。
    place(bp,asize);
    
    return bp;
}
```

### 3.mm_free - 释放块

释放块相对容易，我们只需要将已分配块的首尾标记位均置零后与前后空闲块合并即可，在这里我们先假设coalesce()函数可以正常工作，
能够合并与要释放的块前后相邻的空闲块。

```
void mm_free(void *bp)
{
    if(!bp)
        return;
    size_t size=GET_SIZE(HDRP(bp));
    /*  将头部和尾部的已分配位设置为0，从而释放该块内存 */
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    
    //合并相邻的空闲块，并将它们添加到空闲链表中
    coalesce(bp);
}
```

### 4.mm_realloc - 通过 mm_malloc 和 mm_free 实现

`mm_realloc`函数能够根据所给出的大小重新分配指定已分配块内存空间。若是空指针则相当于`mm_malloc`，若重新分配大小为0则相当于`free`
这里我们首先找出按照对其要求调整后，所需要的块大小，再对情况分类讨论，根据所需大小和当前已分配块的负载大小的关系将情况分为三类：

(1)大小等于当前有效负载大小时直接返回，相当于没有修改直接返回即可。

(2)大小小于当前有效负载大小时将当前块分割成两个已分配块，其中第一个块大小为要求大小，此时释放第二个块并返回指向第一个块的指针即可。

(3)请求的大小大于当前有效负载大小，根据下一个块是否为空讨论，若下一个块为空且当前块大小加上下一个块的大小大于要求大小则将两个块当成一个，
设置为已分配并且释放掉多余内存，若下一个块为空且当前块大小加上下一个块的大小小于要求大小，则分配一个新的所请求大小的内存块，并释放当前内存块。

```
void *mm_realloc(void *ptr, size_t size)
{
    if(ptr==NULL)
        return mm_malloc(size);
    if(size==0)
    {
        mm_free(ptr);
        return NULL;
    }
    size_t asize=MAX(ALIGN(size)+DSIZE,MINBLOCKSIZE);
    size_t current_size=GET_SIZE(HDRP(ptr));
    
    void *bp;
    char* next=HDRP(NEXT_BLKP(ptr));
    size_t newsize=current_size+GET_SIZE(next);
    
    // 情况1：大小等于当前有效负载大小 
    if(asize==current_size)
        return ptr;
    
    //情况2：大小小于当前有效负载大小
    if(asize<=current_size)
    {
        if(asize>MINBLOCKSIZE && (current_size-asize)>MINBLOCKSIZE)
        {
        PUT(HDRP(ptr),PACK(asize,1));
        PUT(FTRP(ptr),PACK(asize,1));
        bp=NEXT_BLKP(ptr);
        PUT(HDRP(bp),PACK(current_size-asize,1));
        PUT(FTRP(bp),PACK(current_size-asize,1));
        mm_free(bp);
        return ptr;
        }
        
        bp=mm_malloc(asize);
        memcpy(bp,ptr,asize);
        mm_free(ptr);
        return bp;
    }
    
    // 情况3：请求的大小大于当前有效负载大小
    else
    {
        if(!GET_ALLOC(next) && newsize>=asize)
        {
            remove_freeblock(NEXT_BLKP(ptr));
            PUT(HDRP(ptr),PACK(asize,1));
            PUT(FTRP(ptr),PACK(asize,1));
            bp=NEXT_BLKP(ptr);
            PUT(HDRP(bp),PACK(newsize-asize,1));
            PUT(FTRP(bp),PACK(newsize-asize,1));
            mm_free(bp);
            return ptr;
        }
        
        // 否则分配一个新的所请求大小的内存块，并释放当前内存块
        bp=mm_malloc(asize);
        memcpy(bp,ptr,current_size);
        mm_free(ptr);
        return bp;
    }
}
```

### 到目前为止我们已经依靠一些声明了原型的辅助函数基本实现了模拟`malloc` 的功能，接下来我们将具体讨论这些辅助函数如何实现

## 辅助函数

### 1.extend_heap -  将堆扩展给定数量的字（向上取整为最接近的偶数）

`extend_heap`函数的作用是在需要时扩展堆的大小，首先根据对齐要求调整输入的要求大小必须为双字（8）的倍数，且不得小于最小块大小，在使用mem_sbrk()函数扩展模拟堆的大小，
此时bp指向新申请的空间的头部，我们需要设置新创建的空闲块的头脚注和尾脚注，并将结尾块推到后面，最后合并最新申请块的前后的空闲块即可。

```
static void *extend_heap(size_t words)
        {
            char * bp;
            size_t asize;
            //调整大小以满足对齐和最小块大小要求 
            asize=(words%2)?(words+1)*WSIZE:words*WSIZE;
            if(asize<MINBLOCKSIZE)
                asize=MINBLOCKSIZE;
            
            // 尝试通过调整后的大小扩展堆
            if((bp=mem_sbrk(asize))==(void*)-1)
                return NULL;
            
            /* 设置新创建的空闲块的头脚注和尾脚注，并将结尾块推到后面*/
            PUT(HDRP(bp),PACK(asize,0));
            PUT(FTRP(bp),PACK(asize,0));
            PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));/* 将结尾块（epilogue）移动到最后 */
                
             // 合并空闲块
            return coalesce(bp);
        }
```

### 2.find_fit - 尝试在空闲列表中查找至少给定大小的空闲块

`find_fit`函数会再模拟堆中进行搜索，遍历空闲块链表并尝试找到足够大的空闲块，如果没有足够大的空闲块则返回`NULL`。

```
static void* find_fit(size_t size)    
        {
            // 搜索策略：First-fit search
            void* bp;
            
            //遍历空闲块链表并尝试找到足够大的空闲块
            for(bp=free_listp;GET_ALLOC(HDRP(bp))==0;bp=NEXT_FREE(bp))
            {
                if(size<=GET_SIZE(HDRP(bp)))
                    return bp;
            }
            //否则说明没有足够大的空闲块
            return NULL;
        }
```

### 3.remove_freeblock - 从空闲块链表中移除指定的空闲块。

当空闲块被分配时，该空闲块变为已分配块，不应存在与显式空闲链表结构的链表中，所以需要使用`remove_freeblock`从链表中移除。具体步骤如下：

(1)首先检查传入的参数 bp 是否为非空指针。

(2)若 bp 所指向的空闲块有前驱（即 PREV_FREE(bp) 不为 NULL），则将该块在链表中的前驱指向它的后继（也就是 NEXT_FREE(bp)），
以完成从链表中移除该块的操作。

(3)否则，如果该块没有前驱，说明它是该链表的首块，因此需要将 free_listp（即链表的头指针）指向它的后继，
以完成从链表中移除该块的操作。

(4)接下来，判断该块是否有后继（即 NEXT_FREE(bp) 不为 NULL）。如果有，将该块在链表中的后继指向它的前驱（也就是 PREV_FREE(bp)），
以完成从链表中移除该块的操作。

```
static void remove_freeblock(void* bp)
        {
            if(bp)
            {
                if(PREV_FREE(bp))
                    NEXT_FREE(PREV_FREE(bp))=NEXT_FREE(bp);
                else
                    free_listp=NEXT_FREE(bp);
                if(NEXT_FREE(bp)!=NULL)
                    PREV_FREE(NEXT_FREE(bp))=PREV_FREE(bp);
            }
        }
```

### 4.coalesce - 利用边界标记策略合并相邻内存块的内存。

 在教材中 (596页,9.9.11节)有相关理论性内容,图示和描述都十分详细，在此不多赘述。
 该函数会将相邻的空闲块合并在一起，并将合并后的大块添加到空闲块链表中。同时，还会从空闲块链表中移除被合并的块。

 ```
static void* coalesce(void* bp)        
        {
            // 确定前一个和后一个块的当前分配状态
            size_t prev_alloc=GET_ALLOC(FTRP(PREV_BLKP(bp)))|| PREV_BLKP(bp)==bp;
            size_t next_alloc=GET_ALLOC(HDRP(NEXT_BLKP(bp)));
            
            // 获取当前空闲块的大小
            size_t size=GET_SIZE(HDRP(bp));
            
            //如果下一个块是空闲的，则合并当前块（bp）和下一个块
            // 情况 2 (教材中)
            if(prev_alloc && !next_alloc)
            {
                size+=GET_SIZE(HDRP(NEXT_BLKP(bp)));
                remove_freeblock(NEXT_BLKP(bp));
                PUT(HDRP(bp),PACK(size,0));
                PUT(FTRP(bp),PACK(size,0));
            }
            
            //如果前一个块是空闲的，则合并当前块（bp）和前一个块
            // 情况 3 (教材中)
            else if(!prev_alloc && next_alloc)
            {
                size+=GET_SIZE(HDRP(PREV_BLKP(bp)));
                bp=PREV_BLKP(bp);
                remove_freeblock(bp);
                PUT(HDRP(bp),PACK(size,0));
                PUT(FTRP(bp),PACK(size,0));
            }
            
            //如果前一个块和下一个块都是空闲的，则合并它们两个
            // 情况 4 (教材中) 
            else if(!prev_alloc && !next_alloc)
            {
                size+=GET_SIZE(HDRP(PREV_BLKP(bp)))+GET_SIZE(HDRP(NEXT_BLKP(bp)));
                remove_freeblock(PREV_BLKP(bp));
                remove_freeblock(NEXT_BLKP(bp));
                bp=PREV_BLKP(bp);
                PUT(HDRP(bp),PACK(size,0));
                PUT(FTRP(bp),PACK(size,0));
            }
            
            // 将合并后的空闲块插入到空闲块链表的前面
            NEXT_FREE(bp)=free_listp;
            PREV_FREE(free_listp)=bp;
            PREV_FREE(bp)=NULL;
            free_listp=bp;
            
            return bp;
        }
```

### 5.place - 在给定的空闲块（由指针bp指向）中分配给定大小的内存块

将给定大小的内存块放置在指向的空闲块中。使用了分裂策略。

具体步骤如下：

(1)获取空闲块的总大小 `fsize`。

(2)如果 `(fsize - asize)` 大于或等于最小块大小，表示当前空闲块可以分裂成两个块，一个已分配块和一个剩余的空闲块。

(3)将已分配块的头部和尾部设置为已分配状态，并从空闲块列表中移除。

(4)计算剩余空闲块的大小，并设置其头部和尾部为未分配状态。

(5)对剩余的空闲块进行合并，即检查其前后相邻的空闲块是否也为未分配状态，如果是，则将它们合并为一个大的空闲块。

(6)如果 `(fsize - asize)` 小于最小块大小，则表示当前空闲块无法分裂。直接将整个空闲块设置为已分配状态，并从空闲块列表中移除。

```
static void place(void* bp,size_t asize)
        {
            // 获取空闲块的总大小
            size_t fsize=GET_SIZE(HDRP(bp));
            
            //获取空闲块的总大小
            if(fsize-asize>=MINBLOCKSIZE)
            {
                PUT(HDRP(bp),PACK(asize,1));
                PUT(FTRP(bp),PACK(asize,1));
                remove_freeblock(bp);
                bp=NEXT_BLKP(bp);
                PUT(HDRP(bp),PACK(fsize-asize,0));
                PUT(FTRP(bp),PACK(fsize-asize,0));
                coalesce(bp);
            }
            
            else
            {
            // 情况2: 无法进行拆分操作，使用完整的空闲块
            PUT(HDRP(bp),PACK(fsize,1));
            PUT(FTRP(bp),PACK(fsize,1));
            remove_freeblock(bp);
            }
        }
```

## 测试结果

测试命令：依次输入`make`,`./mdriver -t traces -V`，并且请确保traces文件夹在malloclab-hanout文件夹目录下。

结果如下：



