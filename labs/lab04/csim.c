//StudentId:10224507041
#include "cachelab.h"
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <limits.h>

char t[1000];//存getopt中s的内容

//参数
int h,v,s,E,b,S;
int hit_cnt,miss_cnt,eviction_cnt;

//结构体

//模拟高速缓存
typedef struct 
{
    int is_valid;//有效位
    int tag;//标记
    int time_flag;//为LSU实现准备，记录上一次访问这条缓存的时间
}cache_line,*cache_asso,**cache;
//cache_line表示一个缓存行，而cache_asso表示指向缓存行的指针，cache则表示指向缓存的二级指针，可以认为是一个指向缓存行数组的指针

cache my_cache=NULL;

//相关函数

//初始化cache
void init_cache()
{
    my_cache=(cache)malloc(sizeof(cache_asso)*S);
    for(int i=0;i<S;i++)
    {
        my_cache[i]=(cache_asso)malloc(sizeof(cache_line)*E);
        for(int j=0;j<E;j++)
        {
            my_cache[i][j].is_valid=0;
            my_cache[i][j].tag=-1;
            my_cache[i][j].time_flag=-1;
        }
    }
}

//根据索引地址更新cache
void update(unsigned int address)
{
    int set_index=(address >> b) & ((-1U) >> (64 - s));
    int input_tag=address>>(b+s);
    
    int max_flag=INT_MIN;
    int max_flag_index=-1;
    
    //tag相同并且valid_bit为1，则命中               
    for(int i=0;i<E;i++)
    {
        if(my_cache[set_index][i].tag==input_tag && my_cache[set_index][i].is_valid==1)
        {
            my_cache[set_index][i].time_flag=0;
            hit_cnt++;
            return;
        }
    }
    
    //若不命中
    //若有空行，则将从下一级储存中取出数据后直接填入空行
    for(int i=0;i<E;i++)
    {
        if(my_cache[set_index][i].is_valid==0)
        {
            my_cache[set_index][i].is_valid=1;
            my_cache[set_index][i].tag=input_tag;
            my_cache[set_index][i].time_flag=0;
            miss_cnt++;
            return;
        }
    }
   
    //没有命中也没有空行就说明要从下一层缓存中取出后再根据LRU驱逐某一行
    eviction_cnt++;
    miss_cnt++;
                   
    for(int i=0;i<E;i++)
    {
        if(my_cache[set_index][i].time_flag>max_flag)
        {
            max_flag=my_cache[set_index][i].time_flag;
            max_flag_index=i;
        }
    }
    my_cache[set_index][max_flag_index].tag=input_tag;
    my_cache[set_index][max_flag_index].time_flag=0;
    return;
}

//更新LRU标记
void update_time_flag()
{
    for(int i=0;i<S;i++)
    {
        for(int j=0;j<E;j++)
        {
            if(my_cache[i][j].is_valid==1)
            {
                my_cache[i][j].time_flag++;
            }
        }
    }
}
                   
void read_input()
{
    FILE* fp = fopen(t, "r"); // 读取文件名
    if(fp == NULL)
    {
        printf("open error");
        exit(-1);
    }

    char operation;         // 命令开头的 I L M S
    unsigned int address;   // 地址参数
    int size;               // 大小
    while(fscanf(fp, " %c %xu,%d\n", &operation, &address, &size) > 0)
    {

        switch(operation)
        {
            //case 'I': continue;   // 不用写关于 I 的判断也可以
            case 'L':
                update(address);
                break;
            case 'M':
                update(address);  // miss的话还要进行一次storage
            case 'S':
                update(address);
        }
        update_time_flag();//更新时间戳
    }

    fclose(fp);
    for(int i = 0; i < S; ++i)
        free(my_cache[i]);
    free(my_cache);            // malloc 完要记得 free 并且关文件

}                             

int main(int argc ,char* argv[])
{
    hit_cnt=0;
    miss_cnt=0;
    eviction_cnt=0;
    int opt;
    
    while(-1!=(opt=getopt(argc,argv,"hvs:E:b:t:")))
    {
        switch(opt)
        {
            case 's':
                s=atoi(optarg);
                break;
            case 'E':
                E=atoi(optarg);
                break;
            case 'b':
                b=atoi(optarg);
                break;
            case 't':
                strcpy(t,optarg);
                break;
            default:
                break;
        }
    }
    
    if(s<=0 || E<=0 || b<=0 || t==NULL)
    {
        return -1;
    }
    S=1<<s;
    
    FILE* fp=fopen(t,"r");
    if(fp==NULL)
    {
        printf("File can't been open!");
        exit(-1);
    }
    
    init_cache();
    read_input(); // 更新最终的三个参数
    
    printSummary(hit_cnt, miss_cnt, eviction_cnt);
    return 0;
}
