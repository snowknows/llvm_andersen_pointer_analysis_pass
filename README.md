# llvm_andersen_pointer_analysis_pass
andersen's pointer analysis base on llvm's ModulePass

## 在已经有那么多人上传了他们多andersen指针分析，并且很多都在llvm上实现了（特别是SVF），那么我为什么还要自己重现实现一遍呢？
原因很简单，他们的不好用。要么是因为llvm的版本问题，或者是本身的结果不好用，就是这么简单。我试过好几个已有的andersen分析工程，比如SVF，比如     LLVMAndersenAnalysisPass，但是他们不好用。SVF的结果根本不直观，而且还需要mem2reg进行优化后产生结果，这会导致一些变量直接被优化掉了，使得指针    分析其实失去了很多信息。而且就算不优化，他的结果也不友好。LLVMAndersenAnalysisPass则更离谱，虽然我的llvm中间代码生成Andersen约束参考了他的很   多代码，但是我不得不说，这位老兄的实现是有相当大的问题的，他的分析结果有很多都是错的。

以上，就是我自己动手来写andersen指针分析的原因。

## 这个工程的andersen分析已经做到什么程度了？
不得不承认，普通的指针分析还是很容易实现的，但是，更细致的指针问题还是比较麻烦的，原因是指针这个东西太灵活了。目前我仍在完善各种指针的分析，后续仍会不断更新。

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
