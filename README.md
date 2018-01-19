# llvm_andersen_pointer_analysis_pass
andersen's pointer analysis base on llvm's ModulePass

## 这个工程的andersen分析已经做到什么程度了？
不得不承认，普通的指针分析还是很容易实现的，但是，更细致的指针问题还是比较麻烦的，原因是指针这个东西太灵活了。目前我仍在完善各种指针的分析，后续仍会不断更新。目前该工程只针对.c文件，c++后续再说。

    *简单指针分析（solved）<br>
    *函数调用（solved）<br>
    *数组指针（solved）<br>
    *指针操作数组（solved）<br>
    *结构体指针（solved）<br>
    *指针操作结构体（solved）<br>
    *全局指针（solved）<br>
    *全局数组（solved）<br>
    *全局结构体（solved）<br>
    *结构体数组中的指针情况（unsolve）<br>
    *函数指针（unsolve）<br>
    *库函数，如malloc （unsolve）<br>

## 文件关系？
andersen.cpp: 实现了andersen算法和算法所需的数据结构，可以独立运行。只需要在init()函数中设定好所需的andersen约束关系<br>
andersen_analysis.cpp: 从llvm的.bc文件中得到指针约束关系，再调用andersen.cpp 中的接口实现andersen指针分析，并输出结果。<br>
makefile: 生成可load的pass库，需要修改其中的LLVM_INCLUDE和LLVM_LIB，将其修改为你自己的llvm源码和编译后的文件夹，这个只要之前写过llvm pass的应该知道我想表达什么。

## 如何使用？
这里使用一个小例子来说明使用方法。
###  example.c

    char* swap(char **p, char **q){
      char* t = *p;
      *p = *q;
      *q = t;
      return t;
    }
    int main(){
      char a1, b1;
      char *a = &a1;
      char *b = &b1;
      char *c = swap(&a,&b);
    }
    
### clang -c -emit-llvm example.c -o example.bc
### llvm-dis example.bc -o example.ll

    ; Function Attrs: noinline nounwind uwtable
    define i8* @swap(i8** %p, i8** %q) #0 {
    entry:
      %p.addr = alloca i8**, align 8
      %q.addr = alloca i8**, align 8
      %t = alloca i8*, align 8
      store i8** %p, i8*** %p.addr, align 8
      store i8** %q, i8*** %q.addr, align 8
      %0 = load i8**, i8*** %p.addr, align 8
      %1 = load i8*, i8** %0, align 8
      store i8* %1, i8** %t, align 8
      %2 = load i8**, i8*** %q.addr, align 8
      %3 = load i8*, i8** %2, align 8
      %4 = load i8**, i8*** %p.addr, align 8
      store i8* %3, i8** %4, align 8
      %5 = load i8*, i8** %t, align 8
      %6 = load i8**, i8*** %q.addr, align 8
      store i8* %5, i8** %6, align 8
      %7 = load i8*, i8** %t, align 8
      ret i8* %7
    }

    ; Function Attrs: noinline nounwind uwtable
    define i32 @main() #0 {
    entry:
      %a1 = alloca i8, align 1
      %b1 = alloca i8, align 1
      %a = alloca i8*, align 8
      %b = alloca i8*, align 8
      %c = alloca i8*, align 8
      store i8* %a1, i8** %a, align 8
      store i8* %b1, i8** %b, align 8
      %call = call i8* @swap(i8** %a, i8** %b)
      store i8* %call, i8** %c, align 8
      ret i32 0
    }

### opt -load ./andersen_analysis.so -ander example.bc

    main@a -> {main@a1, main@b1, }
    main@a1 -> {}
    main@b -> {main@a1, main@b1, }
    main@b1 -> {}
    main@c -> {main@a1, main@b1, }
    swap@p -> {main@a, }
    swap@q -> {main@b, }
    swap@t -> {main@a1, main@b1, }
    
## 其他
在ModelePass的runOnModule函数中很多函数都有一个bool参数，这个是指示是否输出相应运行时信息的选项，你可以设置true来得到相应运行时信息，或者false屏蔽。<br>
本工程只与llvm的中间代码表示有关，与llvm版本无关。目前测试4.0.0和5.0.0均正常运行。
