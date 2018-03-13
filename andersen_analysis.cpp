#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include <set>
#include <list>
#include <fstream>
#include "andersen.cpp" 

using namespace llvm;
using namespace std;

class AndersenPA : public ModulePass {
public:
    static char ID;
    AndersenPA() : ModulePass(ID) {}
    bool runOnModule(Module &M) {
        string filename = getFileName(M, false);
        errs() << "Andersen Alias Analysis1: " << filename << " \n\n";

        for(Module::global_iterator G = M.global_begin(), GE = M.global_end(); G != GE; G++){
            handleGlobalVariable(*G, false);
        }

        for (Module::iterator F = M.begin(), FE = M.end(); F != FE; F++) {
            createInitialConstraints(*F, false);
        }

        // printInfo(nodes, constraints, edges);

        solveCallBind(false);

        doRobust(false);

         // printInfo(nodes, constraints, edges);
        set<Pointer>::iterator b = nodes.begin();
        set<Pointer>::iterator e = nodes.end();
        for(; b != e; b++){
            Pointer node = *b;
            if (!node.pts.empty()) {
                //node.str();
                workQueue.push_back(node.sid);
            }
        }
        //init(nodes, constraints, edges, workQueue);
        calculateConstraints(nodes, constraints, edges, workQueue, false);
        // printInfo(nodes, constraints, edges);

        // for(map<string, string>::iterator i = PointerToType.begin(), e = PointerToType.end(); i != e; i++)
        //     errs() << "pointer: " << i->first << ", type: " << i->second << "\n";
        getPointToInfo(true);
        writeToFile(filename + "_pointer.txt");

        return false;
    }
private:
    /* data */
    set<Pointer> nodes;
    vector<constraint> constraints;
    vector<edge> edges;       
    list<string> workQueue;

    unsigned objIndx = 0;
    unsigned tempIndx = 0;

    map<Value *, string> TempValues;
    vector<string> nodeValue;
    vector<Value *> callValue;
    map<string, vector<string>> CallValueBind;
    map<string, vector<string>> nodeToPtrs;
    map<Value *, Value *> GEPValueMap;
    map<Value *, string> StructToElememt;
    vector<string> ElementToObject; 
    map<string, string> PointerToType;
    map<string, unsigned> mallocInFunc;
    map<Type *, vector<Function *>> FunctionList; 

    /* main fuction */
    string getFileName(Module &M, bool isPrint);
    void handleGlobalVariable(GlobalVariable &G, bool isPrint);
    void createInitialConstraints(Function &F, bool isPrint);

    /* help functions */
    string getTempPointer();
    string getObject();
    void addNodeValue(string node, bool isPrint);
    void solveArgcAndArgv(bool isPrint);
    string getFunctionNameFromInstruction(Instruction &I);

    void solveAllocaInst(AllocaInst *AI, bool isPrint);
    void solveLoadInst(LoadInst *LI, bool isPrint);
    void solveStoreInst(StoreInst *SI, bool isPrint);
    void solveCallInst(CallInst *CI, bool isPrint);
    void solveReturnInst(ReturnInst *RI, bool isPrint);
    void solveBitCastInst(BitCastInst *BCI, bool isPrint);
    void solveGetElementPtrInst(GetElementPtrInst *GEPI, bool isPrint);

    void writeToFile(string filename){
        ofstream ofile;
        ofile.open(filename);
        for(map<string, vector<string>>::iterator i = nodeToPtrs.begin(), e = nodeToPtrs.end(); i != e; i++){
            if(!i->second.empty()){
                 ofile << i->first;
                for(vector<string>::iterator vi = (i->second).begin(), ve = (i->second).end(); vi != ve; vi++){
                   ofile << " " << (*vi);
                }
                ofile << "\n";
            }
        }
        ofile.close();
    }

    void CallRetBind(string calledFuncName, Value *CalledValue, string funcName, bool isPrint){

        callValue.push_back(CalledValue);
        string calledValueName = CalledValue->getName();
        
        if(isPrint)
            errs() << "addEdge: " << calledValueName << " = #" << calledFuncName << "\n";
        addNodeValue(calledValueName, isPrint);
        addNodeValue("#" + calledFuncName, isPrint);
        addEdge("#" + calledFuncName, calledValueName, edges);

        if(calledFuncName == "malloc"){
            string objName = getObject();
            string malloc_ret = "_malloc_@" + getMallocInFunc(funcName);
            addNodeValue(objName, isPrint);
            addrNode(malloc_ret, objName, nodes);
            if(isPrint)
                errs() << "addEdge: #" << calledFuncName << " = " << malloc_ret << "\n";
            addEdge(malloc_ret ,"#" + calledFuncName, edges);
        }
    }

    void ArgBind(Function *FuncCalled, CallInst *CI, bool isPrint){
        string funcName = getFunctionNameFromInstruction(*CI);
        string calledFuncName = FuncCalled->getName().str();
        int i = 0;
        for (Function::arg_iterator FA = FuncCalled->arg_begin(),
             FAE = FuncCalled->arg_end(); FA != FAE; FA++, i++) {
            
            Value *actualArg = CI->getArgOperand(i);
            if(ConstantExpr *CE = dyn_cast<ConstantExpr>(actualArg)){
                if(CE->getOpcode() == Instruction::GetElementPtr){
                    actualArg = CE->getOperand(0);
                }
            }
            while(isa<GetElementPtrInst>(actualArg)){
                if(GEPValueMap.find(actualArg) != GEPValueMap.end())
                    actualArg = GEPValueMap[actualArg];
            }
            string formalArgName = calledFuncName + "@";
            formalArgName += FA->getName();
            addNodeValue(formalArgName, isPrint);

            string actualArgName;

            if (actualArg->hasName()) {
                actualArgName = funcName + "@";
                actualArgName += actualArg->getName();
                
            }else{
                actualArgName = TempValues[actualArg];

            }
            if(actualArg->getType()->isPointerTy())
                ;
            else {
                // if(isPrint)
                //     errs() << "not Pointer:" << actualArgName << "\n";
                continue;
            }
            if(isa<GlobalVariable>(actualArg)){
                actualArgName = "_global_@";
                actualArgName += actualArg->getName();
            }
            if(isPrint)
                errs() << "bind args:" << formalArgName << " = " << actualArgName << "\n";
            CallValueBind[formalArgName].push_back(actualArgName);
        }
    }

    void solveCallBind(bool isPrint){
        // for(map<string, vector<string>>::iterator i = CallValueBind.begin(), e = CallValueBind.end(); i != e; i++){
        //     errs() << "CallValueBind: " << i->first << "\n";
        // }
        map<string,vector<string>> constraintAppend;
        if(isPrint)
            errs() << "solve call bind in constraints:\n";
        for(vector<constraint>::iterator i = constraints.begin(), e = constraints.end(); i != e; i++){
            if(CallValueBind.find((*i).leftOp.valueName) != CallValueBind.end()){
                for(vector<string>::iterator vi = CallValueBind[(*i).leftOp.valueName].begin(), ve = CallValueBind[(*i).leftOp.valueName].end();
                    vi != ve; vi++){
                    if(isPrint){
                        errs() << "solve left: " << (*i).leftOp.valueName << "=" << (*i).rightOp.valueName << \
                            " --> " << (*vi) << "=" << (*i).rightOp.valueName << "\n";                    
                    }
                    if(vi == CallValueBind[(*i).leftOp.valueName].begin())
                        (*i).leftOp.valueName = (*vi);
                    
                    constraintAppend[(*vi)].push_back((*i).rightOp.valueName);
               }
               
            }
            if(CallValueBind.find((*i).rightOp.valueName) != CallValueBind.end()){
                for(vector<string>::iterator vi = CallValueBind[(*i).rightOp.valueName].begin(), ve = CallValueBind[(*i).rightOp.valueName].end();
                    vi != ve; vi++){
                    if(isPrint){
                        errs() << "solve right: " << (*i).leftOp.valueName << "=" << (*i).rightOp.valueName << \
                            " --> " << (*i).leftOp.valueName << "=" << (*vi) << "\n";                    
                    }
                    if(vi == CallValueBind[(*i).rightOp.valueName].begin())
                        (*i).rightOp.valueName = (*vi);
                    
                    constraintAppend[(*i).leftOp.valueName].push_back(*vi);
               }
            }   
        }

        for(map<string, vector<string>>::iterator mi = constraintAppend.begin(), me = constraintAppend.end(); mi != me; mi++){
            for(vector<string>::iterator i = mi->second.begin(), e = mi->second.end(); i != e; i++){
                if(isPrint)
                    errs() << "addConstraint: " << mi->first << " = " << *i << "\n";
                addConstraint(mi->first, *i, constraints);
            }
        }
        // for(map<string, vector<string>>::iterator i = CallValueBind.begin(), e = CallValueBind.end(); i != e; i++){
        //     errs() << "CallValueBind: " << i->first << "\n";
        // }
        if(isPrint)
            errs() << "solve call bind in nodes:\n";
        for(set<Pointer>::iterator i = nodes.begin(); i != nodes.end();){
            if(CallValueBind.find((*i).sid) != CallValueBind.end() && !CallValueBind[(*i).sid].empty()){
                if(isPrint)
                    errs() << "delete node: " << (*i).sid << "\n";
                set<Pointer>::iterator e = i;
                i++;
                nodes.erase(e);
                continue;
            }
            i++;
        }
        //errs() << "solve bind finished.\n";
    }
    void doRobust(bool isPrint){
        for(set<Pointer>::iterator i = nodes.begin(), e = nodes.end(); i != e; i++){
            if(isContainAt((*i).sid)){
                if((*i).pts.empty()){
                    
                    string objName = getObject();
                    addNodeValue(objName, isPrint);
                    addrNode((*i).sid, objName, nodes);
                    if(isPrint){
                        errs() << "need doRobust: " << (*i).sid << "\n";
                        errs() << "addrNode: " <<  (*i).sid  << " = &" << objName << "\n";
                    }
                }
            }
        }
    }

    void getPointToInfo(bool isPrint){
        /*it's a translate function only for llvm pass.
        because llvm's pointer and variable are all connected to it's memory object,
        so we need to trainslate the memory relationship to pointer-to relationship.

        nodes: all pointer and variable nodes
        nodeToPtrs: the result pointer-to relations,pointer and it's pts
        isPrint:whether to print running imformation
        */
        set<Pointer>::iterator i,e;
        map<string, vector<string> > ObjectToValue; //object to it's address name, that is, variable or pointer

        for(i = nodes.begin(), e = nodes.end(); i != e; i++){
            //construct ObjectToValue from nodes
            if(isContainAt((*i).sid)){
                set<Pointer> pts = (*i).pts;
                string objname = (*(pts.begin())).sid;
                ObjectToValue[objname].push_back((*i).sid);

            }
        }
        for(i = nodes.begin(), e = nodes.end(); i != e; i++){
            if(isContainAt((*i).sid)){
                set<Pointer> pts = (*i).pts;
                if(!pts.empty()){
                    string objname = (*(pts.begin())).sid;
                    set<Pointer> objpts = getPts(nodes, objname); // get object's pts
                    set<Pointer>::iterator si,se;
                    string vname = (*i).sid.substr(0, (*i).sid.find(".addr")); //convert p.addr to p
                    vector<string> toPtrs; //contains point-to variables

                    for(si = objpts.begin(), se = objpts.end(); si != se; si++){
                        string toobjname = (*si).sid;
                        vector<string> toValue = ObjectToValue[toobjname];
                        vector<string>::iterator vi,ve;
                        for(vi = toValue.begin(), ve = toValue.end(); vi != ve; vi++){
                            toPtrs.push_back(*vi);
                        }
                    }
                    nodeToPtrs[vname] = toPtrs; //pointer to variables map
                }

            }
        }
        if(isPrint){
            //print pointer and it's pts
            //variable's pts in empty.
            for(map<string, vector<string>>::iterator mi = nodeToPtrs.begin(), me = nodeToPtrs.end(); mi != me; mi++){
                errs() << mi->first << " -> {";
                for(vector<string>::iterator vi = mi->second.begin(), ve = mi->second.end(); vi != ve; vi++)
                    errs() << (*vi) << ", ";
                errs() << "}\n";
            }
        }  
    }

    string getMallocInFunc(string FuncName){
        unsigned idx = 0;
        if(mallocInFunc.find(FuncName) != mallocInFunc.end()){           
            mallocInFunc[FuncName] = mallocInFunc[FuncName] + 1;
            idx = mallocInFunc[FuncName];
        }else{
            mallocInFunc[FuncName] = 0;
        }
        return FuncName + "%" + to_string(idx);
    }

    void updateFunctionList(Type *t, Function *F){
        if(FunctionList.find(t) == FunctionList.end()){ //更新FunctionList
            vector<Function *> v;
            v.push_back(F);
            FunctionList.insert(map<Type *, vector<Function *>>::value_type(t,v));
        }
        else{
            vector<Function *> v = FunctionList.find(t)->second;
            if(find(v.begin(),v.end(),F) == v.end())
                (FunctionList.find(t)->second).push_back(F);
        }
    }
    vector<Function *> getFuncList(Type *t){
        Type *ft = t->getPointerElementType();
        vector<Function *> funclist;
        funclist = FunctionList[ft];

        // for(vector<Function *>::iterator i = funclist.begin(), e = funclist.end(); i != e; i++){
        //     errs() << "function: " << (*i)->getName().str() << "\n";
        // }
        return funclist;
    }
};

/////////////////////////////////////////////////////////////////////////
//                            main functions                           //
/////////////////////////////////////////////////////////////////////////

string AndersenPA::getFileName(Module &M, bool isPrint){
    string filename = M.getName().str();
    filename = filename.substr(0,filename.find(".bc"));
    if(isPrint)
        errs() << "filename: " << filename << "\n";

    return filename;
}

void AndersenPA::handleGlobalVariable(GlobalVariable &G, bool isPrint){    
    //if(!G.getValueType()->isStructTy()){
        string gName = "_global_@";
        gName += G.getName();
        string objName = getObject();
        addNodeValue(gName, isPrint);
        addNodeValue(objName, isPrint);
        addrNode(gName, objName, nodes);
        if(isPrint)
            errs() << "addrNode: " << gName << " = &" << objName << "\n";

        if(G.getValueType()->isPointerTy() && G.hasInitializer()){

            Constant *C = G.getInitializer();
            if(C->hasName()){
                string vName = "_global_@" + C->getName().str();
            
                addConstraint("*" + gName, vName, constraints);
                if(isPrint)
                    errs() << "addConstraint: *" <<gName << " = " << vName << "\n";
            }
        }
    //}
}

void AndersenPA::createInitialConstraints(Function &F, bool isPrint) {
    Type *t = F.getFunctionType();
    Function *Func = &F;
    updateFunctionList(t, Func);

    if(F.getName().str() == "main"){
        solveArgcAndArgv(isPrint);
    }
    for (Function::iterator B = F.begin(), BE = F.end(); B != BE; B++) {
        for (BasicBlock::iterator I = B->begin(), IE = B->end(); I != IE; I++) {
            if(isPrint)
                I->dump();
            
            switch(I->getOpcode()) {
                case Instruction::Alloca:
                {
                    AllocaInst *AI = dyn_cast<AllocaInst>(I);
                    solveAllocaInst(AI, isPrint);
                }
                break;
                case Instruction::GetElementPtr:
                {
                    GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(I);
                    solveGetElementPtrInst(GEPI, isPrint);
                }
                break;
                case Instruction::Store:
                {
                    StoreInst *SI = dyn_cast<StoreInst>(I);
                    solveStoreInst(SI, isPrint);
                }
                break;
                case Instruction::Load:
                {
                    LoadInst *LI = dyn_cast<LoadInst>(I);
                    solveLoadInst(LI, isPrint);
                }
                break;
                case Instruction::Call:
                {
                    CallInst *CI = dyn_cast<CallInst>(I);
                    solveCallInst(CI, isPrint);
                }
                break;
                case Instruction::Ret:
                {
                    ReturnInst *RI = dyn_cast<ReturnInst>(I);
                    solveReturnInst(RI, isPrint);
                }
                break;
                case Instruction::BitCast:
                {
                    BitCastInst *BCI = dyn_cast<BitCastInst>(I);
                    solveBitCastInst(BCI, isPrint);
                }
                break;
            }
            if(isPrint)
                errs() << "\n";
        }
    }
}

/////////////////////////////////////////////////////////////////////////
//                            help functions                           //
/////////////////////////////////////////////////////////////////////////

string AndersenPA::getTempPointer(){
    //
    static string tempName = "_TEMP_";
    return tempName + to_string(tempIndx++);
}

string AndersenPA::getObject(){
    static string objName = "_OBJ_";
    return objName + to_string(objIndx++);
}

void AndersenPA::addNodeValue(string node, bool isPrint){
    if(find(nodeValue.begin(), nodeValue.end(), node) == nodeValue.end()){
        if(isPrint)
            errs() << "newNode: " << node << "\n";
        newNode(node, nodes);
        nodeValue.push_back(node);
    }
}

void AndersenPA::solveArgcAndArgv(bool isPrint){
    string argc = "main@argc";  
    string argc_obj = getObject();
    addNodeValue(argc, isPrint);
    addNodeValue(argc_obj, isPrint);
    addrNode(argc, argc_obj, nodes);
    if(isPrint)
        errs() << "addrNode: " <<  argc  << " = &" << argc_obj << "\n";

    string argv = "main@argv";
    string argv_obj = getObject();
    addNodeValue(argv, isPrint);
    addNodeValue(argv_obj, isPrint);
    addrNode(argv, argv_obj, nodes);
    if(isPrint)
        errs() << "addrNode: " <<  argv  << " = &" << argv_obj << "\n";
}

string AndersenPA::getFunctionNameFromInstruction(Instruction &I){
    return I.getFunction()->getName();
}

void AndersenPA::solveAllocaInst(AllocaInst *AI, bool isPrint){
    
    // if(!AI->getAllocatedType()->isStructTy()){
        string oName = getObject();
        string valueName = getFunctionNameFromInstruction(*AI) 
                            + "@" + AI->getName().str();       
        addNodeValue(oName, isPrint);
        addNodeValue(valueName, isPrint);
        if(isPrint)
           errs() << "addrNode: " <<  valueName  << " = &" << oName << "\n";
        addrNode(valueName, oName, nodes);
    // }
    // else{
    //     //init pointer
    //     string oNamePointer = getObject();
    //     string valueNamePointer = getFunctionNameFromInstruction(*AI) 
    //                                 + "@" + AI->getName().str() + ".pointer";        
    //     addNodeValue(oNamePointer, isPrint);
    //     addNodeValue(valueNamePointer, isPrint);
    //     if(isPrint)
    //        errs() << "addrNode: " <<  valueNamePointer  << " = &" << oNamePointer << "\n";
    //     addrNode(valueNamePointer, oNamePointer, nodes);

    //     //init variable
    //     string oNameVariable = getObject();
    //     string valueNameVariable = getFunctionNameFromInstruction(*AI) 
    //                                 + "@" + AI->getName().str() + ".Variable";
    //     addNodeValue(oNameVariable, isPrint);
    //     addNodeValue(valueNameVariable, isPrint);
    //     if(isPrint)
    //        errs() << "addrNode: " <<  valueNameVariable  << " = &" << oNameVariable << "\n";
    //     addrNode(valueNameVariable, oNameVariable, nodes);
    // }
}

void AndersenPA::solveLoadInst(LoadInst *LI, bool isPrint){
    string funcName = getFunctionNameFromInstruction(*LI);
    Value *v = LI;
    Value *ptr = LI->getPointerOperand();

    if(ConstantExpr *CE = dyn_cast<ConstantExpr>(v)){
        if(CE->getOpcode() == Instruction::GetElementPtr){
            //CE->dump();
            string elementName = "";
            v = CE->getOperand(0);
            
            if(GlobalVariable *GV = dyn_cast<GlobalVariable>(v)){
                if(GV->getValueType()->isStructTy()){
                    elementName  += "_global_@";
                    elementName += v->getName();
                    // if(ConstantInt *CI = dyn_cast<ConstantInt>(CE->getOperand(2))){
                        //elementName += "." + to_string(CI->getZExtValue());
                        //errs() << "v: " <<elementName << "\n";
                        StructToElememt[v] = elementName;
                    // }
                }
            }
        }
    }

    if(ConstantExpr *CE = dyn_cast<ConstantExpr>(ptr)){
        if(CE->getOpcode() == Instruction::GetElementPtr){
            string elementName = "";
            ptr = CE->getOperand(0);
            if(GlobalVariable *GV = dyn_cast<GlobalVariable>(ptr)){
                if(GV->getValueType()->isStructTy()){
                    elementName  += "_global_@";
                    elementName += ptr->getName();
                    // if(ConstantInt *CI = dyn_cast<ConstantInt>(CE->getOperand(2))){
                        //elementName += "." + to_string(CI->getZExtValue());
                        //errs() << "v: " <<elementName << "\n";
                        StructToElememt[ptr] = elementName;
                    // }
                }
            }
        }
    }

    

    if ((!isa<GlobalVariable>(v) && isa<Constant>(v)) || 
        (!isa<GlobalVariable>(ptr) && isa<Constant>(ptr))) {
        return;
    }

    if(ptr->getType()->isPointerTy())
        ;
    else {
        return;
    }

    string valueName = "";
    string ptrName;

    while(isa<GetElementPtrInst>(ptr)){
        if(GEPValueMap.find(ptr) != GEPValueMap.end())
            ptr = GEPValueMap[ptr];
    }
    if (!v->hasName()) {
        if (TempValues[v].empty()) {

            string temp = getTempPointer();
            TempValues[v] = temp;
            valueName = temp;
            //errs() << "newNode: " << temp << "\n";
        }
        else{
            valueName = TempValues[v];
        }
    }else {
        
        valueName += v->getName();
    }
    if (!ptr->hasName()) {
        if (TempValues[ptr].empty()) {

            string temp = getTempPointer();
            TempValues[ptr] = temp;
            ptrName = temp;
        }
        else{
            ptrName = TempValues[ptr];
        }
    }else {
        ptrName = funcName + "@";
        ptrName += ptr->getName();
    }
    if(isa<GlobalVariable>(v)){
        valueName = "_global_@";
        valueName += v->getName();
    }
    if(isa<GlobalVariable>(ptr)){
        ptrName = "_global_@";
        ptrName += ptr->getName();
    }
    if(StructToElememt.find(v) != StructToElememt.end()){
        valueName = StructToElememt[v];
        if(find(ElementToObject.begin(), ElementToObject.end(), valueName) == ElementToObject.end()){
            string _objName = getObject();
            addNodeValue(_objName, isPrint);
            addNodeValue(valueName, isPrint);
            addrNode(valueName, _objName, nodes);
            ElementToObject.push_back(valueName);
        }
    }
    if(StructToElememt.find(ptr) != StructToElememt.end()){
        ptrName = StructToElememt[ptr];
        if(find(ElementToObject.begin(), ElementToObject.end(), ptrName) == ElementToObject.end()){
            string _objName = getObject();
            addNodeValue(_objName, isPrint);
            addNodeValue(ptrName, isPrint);
            addrNode(ptrName, _objName, nodes);
            ElementToObject.push_back(ptrName);
        }
    }
    
    if(isPrint)
        errs() << "addConstraint : " << valueName << " = *" << ptrName << "\n";
    addNodeValue(valueName, isPrint);
    addNodeValue(ptrName, isPrint);
    addConstraint(valueName, "*" + ptrName, constraints);

}

void AndersenPA::solveStoreInst(StoreInst *SI, bool isPrint){
    string valueName = "";
    string ptrName;
    string funcName = getFunctionNameFromInstruction(*SI);

    Value *v = SI->getValueOperand();
    Value *ptr = SI->getPointerOperand();

    string type_str;
    llvm::raw_string_ostream rso(type_str);
    v->getType()->print(rso);
    string pointerType = rso.str();
    // errs() << "pointer type:" << pointerType << "\n";

    /*handle GetElementPtr in ConstantExpr*/
    if(ConstantExpr *CE = dyn_cast<ConstantExpr>(v)){
        
        if(CE->getOpcode() == Instruction::GetElementPtr){
            //CE->dump();
            string elementName = "";
            v = CE->getOperand(0);
            
            if(GlobalVariable *GV = dyn_cast<GlobalVariable>(v)){
                if(GV->getValueType()->isStructTy()){
                    elementName  += "_global_@";
                    elementName += v->getName();
                    // if(ConstantInt *CI = dyn_cast<ConstantInt>(CE->getOperand(2))){
                        //elementName += "." + to_string(CI->getZExtValue());
                        //errs() << "v: " <<elementName << "\n";
                        StructToElememt[v] = elementName;
                    // }
                }
            }
        }else if(CE->getOpcode() == Instruction::BitCast){
            Type *t = CE->getType();
            v = CE->getOperand(0);
            if(Function *F = dyn_cast<Function>(v)){
                updateFunctionList(t->getPointerElementType(),F);
            }
        }
    }

    if(ConstantExpr *CE = dyn_cast<ConstantExpr>(ptr)){
        if(CE->getOpcode() == Instruction::GetElementPtr){
            string elementName = "";
            ptr = CE->getOperand(0);
            if(GlobalVariable *GV = dyn_cast<GlobalVariable>(ptr)){
                if(GV->getValueType()->isStructTy()){
                    elementName  += "_global_@";
                    elementName += ptr->getName();
                    // if(ConstantInt *CI = dyn_cast<ConstantInt>(CE->getOperand(2))){
                        //elementName += "." + to_string(CI->getZExtValue());
                        //errs() << "v: " <<elementName << "\n";
                        StructToElememt[ptr] = elementName;
                    // }
                }
            }
        }
    }

    /* avoid constant assignment*/
    if ((!isa<GlobalVariable>(v) && isa<Constant>(v) && !isa<Function>(v)) || 
        (!isa<GlobalVariable>(ptr) && isa<Constant>(ptr)) && !isa<Function>(ptr)) {
        return;
    }

    if(v->getType()->isPointerTy()) {
        if(ptr->getType()->isPointerTy())
            ;
        else
            return;
    }
    else {
        return;
    }

    while(isa<GetElementPtrInst>(v)){
        if(GEPValueMap.find(v) != GEPValueMap.end())
            v = GEPValueMap[v];
    }
    while(isa<GetElementPtrInst>(ptr)){
        if(GEPValueMap.find(ptr) != GEPValueMap.end())
            ptr = GEPValueMap[ptr];
    }

    if (!v->hasName()) {
        if (TempValues[v].empty()) {

            string temp = getTempPointer();
            TempValues[v] = temp;
            valueName = temp;
        }
        else{
            valueName = TempValues[v];
        }
    }else {
        if(find(callValue.begin(), callValue.end(), v) == callValue.end())
            valueName += funcName + "@";
        valueName += v->getName();
    }
    if (!ptr->hasName()) {
        if (TempValues[ptr].empty()) {

            string temp = getTempPointer();
            TempValues[ptr] = temp;
            ptrName = temp;
            //errs() << "newNode: " << temp << "\n";
        }
        else{
            ptrName = TempValues[ptr];
        }
    }else {
        ptrName = funcName + "@";
        ptrName += ptr->getName();
    }

    if(isa<GlobalVariable>(v)){
        valueName = "_global_@";
        valueName += v->getName();
    }
    if(isa<GlobalVariable>(ptr)){
        ptrName = "_global_@";
        ptrName += ptr->getName();
    }
    if(StructToElememt.find(v) != StructToElememt.end()){
        valueName = StructToElememt[v];
        if(find(ElementToObject.begin(), ElementToObject.end(), valueName) == ElementToObject.end()){
            string _objName = getObject();
            addNodeValue(_objName, isPrint);
            addNodeValue(valueName, isPrint);
            addrNode(valueName, _objName, nodes);
            ElementToObject.push_back(valueName);
        }
    }
    if(StructToElememt.find(ptr) != StructToElememt.end()){
        ptrName = StructToElememt[ptr];
        if(find(ElementToObject.begin(), ElementToObject.end(), ptrName) == ElementToObject.end()){
            string _objName = getObject();
            addNodeValue(_objName, isPrint);
            addNodeValue(ptrName, isPrint);
            addrNode(ptrName, _objName, nodes);
            ElementToObject.push_back(ptrName);
        }
    }
    if(isa<Function>(v)){
        string oName = getObject();
        valueName = "_FUNC_@";
        valueName += v->getName();
        addNodeValue(oName, isPrint);
        addNodeValue(valueName, isPrint);
        addrNode(valueName, oName, nodes);
    }
    if(isPrint)
        errs() << "addConstraint : *" << ptrName << " = " << valueName << "\n";
    PointerToType[ptrName] = pointerType;
    addNodeValue(valueName, isPrint);
    addNodeValue(ptrName, isPrint);
    addConstraint("*" + ptrName, valueName, constraints);
}

void AndersenPA::solveCallInst(CallInst *CI, bool isPrint){
    string calledFuncName;
    string funcName = getFunctionNameFromInstruction(*CI);

    Function *FuncCalled = CI->getCalledFunction();
    Value *CalledValue = CI;
    
    if(FuncCalled){        
        calledFuncName = FuncCalled->getName().str();
    }else if(CI->getNumOperands() > 1){
        if(ConstantExpr *CstExpr = dyn_cast<ConstantExpr>(CI->getOperand(1))){
            if (CstExpr->isCast()) {
                calledFuncName = CstExpr->getOperand(0)->getName().str();
            }else
                return;
        }else{
            Type *t = CI->getCalledValue()->getType();
            vector<Function *> funclist = getFuncList(t);
            for(vector<Function *>::iterator i = funclist.begin(), e = funclist.end(); i != e; i++){
                calledFuncName = (*i)->getName().str();
                if (CalledValue->hasName()) {
                    CallRetBind(calledFuncName, CI, funcName, isPrint);
                }
                ArgBind(*i, CI, isPrint);
            }
            return;
        }
    }else{
        Type *t = CI->getCalledValue()->getType();
        vector<Function *> funclist = getFuncList(t);
        for(vector<Function *>::iterator i = funclist.begin(), e = funclist.end(); i != e; i++){
            calledFuncName = (*i)->getName().str();
            if (CalledValue->hasName()) {
                CallRetBind(calledFuncName, CI, funcName, isPrint);
            }
            ArgBind(*i, CI, isPrint);
        }
        return;
    }
       
    if (CalledValue->hasName()) {
        CallRetBind(calledFuncName, CI, funcName, isPrint);
    }
    if(FuncCalled){
        ArgBind(FuncCalled, CI, isPrint);
    }

}

void AndersenPA::solveReturnInst(ReturnInst *RI, bool isPrint){
    string funcName = getFunctionNameFromInstruction(*RI);

    if (RI->getNumOperands() != 0) {
        Value *retVal = RI->getOperand(0);
        string returnName = "";
        if (retVal->hasName()) {
            if(find(callValue.begin(), callValue.end(), retVal) == callValue.end())
               returnName = funcName + "@";
            returnName += retVal->getName();
        }else
            returnName = TempValues[retVal];

        if(!returnName.empty()){
            
            if(isPrint)
                errs() << "addEdge : #" << funcName << " = " << returnName << "\n";
            addNodeValue("#" + funcName, isPrint);
            addNodeValue(returnName, isPrint);
            addEdge(returnName, "#" + funcName, edges);
        }
    }
}

void AndersenPA::solveBitCastInst(BitCastInst *BCI, bool isPrint){
    string dstName, srcName;
    string funcName = getFunctionNameFromInstruction(*BCI);

    Value *dst = BCI;
    Value *src = BCI->getOperand(0);
    //Value *src = BCI->getOperand(1);

    if (!dst->hasName()) {
        if (TempValues[dst].empty()) {

            string temp = getTempPointer();
            TempValues[dst] = temp;
            dstName = temp;
            //errs() << "newNode: " << temp << "\n";
        }
        else{
            dstName = TempValues[dst];
        }
    }else{
        dstName = funcName + "@";
        dstName += dst->getName();
    }

    if (!src->hasName()) {
        if (TempValues[src].empty()) {

            string temp = getTempPointer();
            TempValues[src] = temp;
            srcName = temp;
            //errs() << "newNode: " << temp << "\n";
        }
        else{
            srcName = TempValues[src];
        }
    }else if(isa<CallInst>(src)){
        srcName = src->getName().str();
    }
    else{
        srcName = funcName + "@";
        srcName += src->getName();
    }

    if(isPrint)
        errs() << "addEdge: "<< dstName << " = " << srcName << "\n";
    addNodeValue(dstName, isPrint);
    addNodeValue(srcName, isPrint);
    addEdge(srcName, dstName, edges);
    //src->dump();
}

void AndersenPA::solveGetElementPtrInst(GetElementPtrInst *GEPI, bool isPrint){
    Value *v = GEPI;
    Value *ptr = GEPI->getPointerOperand();
    GEPValueMap[v] = ptr;
    // v->dump();
    // GEPI->getResultElementType()->dump();
    

    if(Instruction *I = dyn_cast<Instruction>(ptr)){
        if(AllocaInst *AI = dyn_cast<AllocaInst>(I)){ //getElement in a store or other inst
            if(AI->getAllocatedType()->isStructTy()){
                string elementName = "";
                if(isa<GlobalVariable>(ptr))
                    elementName  += "_global_@";
                else
                    elementName += getFunctionNameFromInstruction(*GEPI) + "@";
                elementName += ptr->getName();
                //errs() << "elementName: " << elementName << "\n";
                // if(ConstantInt *CI = dyn_cast<ConstantInt>(GEPI->getOperand(2))){
                    //elementName += "." + to_string(CI->getZExtValue());
                    //errs() << "elementName: " << elementName << "\n";
                    StructToElememt[ptr] = elementName;
                // }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////
//                             install pass                            //
/////////////////////////////////////////////////////////////////////////
char AndersenPA::ID = 0;
static RegisterPass<AndersenPA> X("ander", "Andersen Points To Alias Analysis"
                            " pass",
                            false /* Only looks at CFG */,
                            false /* Analysis Pass */);