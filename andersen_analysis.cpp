#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"
#include <set>
#include <list>
#include "andersen.cpp" 

using namespace llvm;
using namespace std;

class AndersenPA : public ModulePass {
public:
    static char ID;
    AndersenPA() : ModulePass(ID) {}
    bool runOnModule(Module &M) {
       errs() << "Andersen Alias Analysis: \n\n";

        for(Module::global_iterator G = M.global_begin(), GE = M.global_end(); G != GE; G++){
            //for globle value
            if(!G->getValueType()->isStructTy()){ //struct need to be handled diffirently
                string gName = "_global_@";
                gName += G->getName();
                string objName = getObject();
                newNode(gName, nodes);
                newNode(objName, nodes);
                addrNode(gName, objName, nodes);
            }
        }
    	for (Module::iterator F = M.begin(), FE = M.end(); F != FE; F++) {
            createInitialConstraints(*F, false); //creat constraints from llvm
        }
        //printInfo(nodes, constraints, edges);
        solveCallBind(false);
        //printInfo(nodes, constraints, edges);

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
	    getPointToInfo(nodes, nodeToPtrs, true);
    	return false;
	}
	void createInitialConstraints(Function &F, bool isPrint);

private:
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

	string getTempPointer(){
		static string tempName = "_TEMP_";
		return tempName + to_string(tempIndx++);
	}

	string getObject(){
		static string objName = "_OBJ_";
		return objName + to_string(objIndx++);
	}

	void addNodeValue(string node, bool isPrint){
		if(find(nodeValue.begin(), nodeValue.end(), node) == nodeValue.end()){
            if(isPrint)
                errs() << "newNode: " << node << "\n";
			newNode(node, nodes);
			nodeValue.push_back(node);
		}
	}
    void solveCallBind(bool isPrint){

        map<string,string> constraintAppend;
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
                    else
                        constraintAppend[(*vi)] = (*i).rightOp.valueName;
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
                    else
                        constraintAppend[(*i).leftOp.valueName] = (*vi);
               }
            }   
        }

        for(map<string, string>::iterator mi = constraintAppend.begin(), me = constraintAppend.end(); mi != me; mi++){
            if(isPrint)
                errs() << "addConstraint: " << mi->first << " = " << mi->second << "\n";
            addConstraint(mi->first, mi->second, constraints);
        }

        if(isPrint)
            errs() << "solve call bind in nodes:\n";
        for(set<Pointer>::iterator i = nodes.begin(); i != nodes.end(); i++){
            if(CallValueBind.find((*i).sid) != CallValueBind.end() && !CallValueBind[(*i).sid].empty()){
                if(isPrint)
                    errs() << "delete node: " << (*i).sid << "\n";
                nodes.erase(i);
            }
        }
    }
};

void AndersenPA::createInitialConstraints(Function &F, bool isPrint) {

    for (Function::iterator B = F.begin(), BE = F.end(); B != BE; B++) {
    	string funcName = F.getName().str();

        for (BasicBlock::iterator I = B->begin(), IE = B->end(); I != IE; I++) {

            switch(I->getOpcode()) {

                case Instruction::Alloca:
                {
                    AllocaInst *AI = dyn_cast<AllocaInst>(I);
                    if(!AI->getAllocatedType()->isStructTy()){
                    	string oName = getObject();
                    	string valueName = funcName + "@";
                    	valueName += I->getName();

                    	
                    	//errs() << "newNode: " << oName << "\n";
                        if(isPrint)
                    	   errs() << "addrNode: " <<  valueName  << " = &" << oName << "\n";
                        addNodeValue(oName, isPrint);
                        addNodeValue(valueName, isPrint);
                    	addrNode(valueName, oName, nodes);
                    }
                }
                break;
                case Instruction::GetElementPtr:
                {
                	//errs() << "GetElementPtr : " << I->getName() << "\n";
                    GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(I);
                    Value *v = GEPI;
                    Value *ptr = GEPI->getPointerOperand();
                    //errs() << v->getName() << " = " << ptr->getName() << "\n";
                    GEPValueMap[v] = ptr;

                    if(Instruction *I = dyn_cast<Instruction>(ptr)){
                        if(AllocaInst *AI = dyn_cast<AllocaInst>(I)){
                            if(AI->getAllocatedType()->isStructTy()){
                                string elementName = "";
                                if(isa<GlobalVariable>(ptr))
                                    elementName  += "_global_@struct#";
                                else
                                    elementName += funcName + "@struct#";
                                elementName += ptr->getName();
                                if(ConstantInt *CI = dyn_cast<ConstantInt>(GEPI->getOperand(2))){
                                    elementName += "." + to_string(CI->getZExtValue());
                                    //errs() << elementName << "\n";
                                    StructToElememt[ptr] = elementName;
                                }
                            }
                        }
                    }
                }
                break;
                case Instruction::Store:
                {
                	StoreInst *SI = dyn_cast<StoreInst>(I);
    
                	Value *v = SI->getValueOperand();
                    Value *ptr = SI->getPointerOperand();
                    //errs() << "store: " << ptr->getName() << " = " << v->getName() << "\n";
                    if(ConstantExpr *CE = dyn_cast<ConstantExpr>(v)){
                        if(CE->getOpcode() == Instruction::GetElementPtr){
                            //CE->dump();
                            string elementName = "";
                            v = CE->getOperand(0);
                            
                            if(GlobalVariable *GV = dyn_cast<GlobalVariable>(v)){
                                if(GV->getValueType()->isStructTy()){
                                    elementName  += "_global_@struct#";
                                    elementName += v->getName();
                                    if(ConstantInt *CI = dyn_cast<ConstantInt>(CE->getOperand(2))){
                                        elementName += "." + to_string(CI->getZExtValue());
                                        //errs() << "v: " <<elementName << "\n";
                                        StructToElememt[v] = elementName;
                                    }
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
                                    elementName  += "_global_@struct#";
                                    elementName += ptr->getName();
                                    if(ConstantInt *CI = dyn_cast<ConstantInt>(CE->getOperand(2))){
                                        elementName += "." + to_string(CI->getZExtValue());
                                        //errs() << "v: " <<elementName << "\n";
                                        StructToElememt[ptr] = elementName;
                                    }
                                }
                            }
                        }
                    }
;
                    if ((!isa<GlobalVariable>(v) && isa<Constant>(v) && !isa<Function>(v)) || 
                        (!isa<GlobalVariable>(ptr) && isa<Constant>(ptr)) && !isa<Function>(ptr)) {
                        break;
                    }

                    if(v->getType()->isPointerTy()) {
                        if(ptr->getType()->isPointerTy())
                        	;
                        else
                            break;
                    }
                    else {
                        break;
                    }
                    //errs() << "here\n";
                    string valueName = "";
                    string ptrName;

                    while(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(v)){

                        if(GEPValueMap.find(v) != GEPValueMap.end())
                            v = GEPValueMap[v];
                    }

                    while(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(ptr)){
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
                    addNodeValue(valueName, isPrint);
                    addNodeValue(ptrName, isPrint);
                    addConstraint("*" + ptrName, valueName, constraints);
                	//I->dump();
                }
                break;
                case Instruction::Load:
                {
                	LoadInst *LI = dyn_cast<LoadInst>(I);
                    Value *v = LI;
                    Value *ptr = LI->getPointerOperand();

                    if(ConstantExpr *CE = dyn_cast<ConstantExpr>(v)){
                        if(CE->getOpcode() == Instruction::GetElementPtr){
                            //CE->dump();
                            string elementName = "";
                            v = CE->getOperand(0);
                            
                            if(GlobalVariable *GV = dyn_cast<GlobalVariable>(v)){
                                if(GV->getValueType()->isStructTy()){
                                    elementName  += "_global_@struct#";
                                    elementName += v->getName();
                                    if(ConstantInt *CI = dyn_cast<ConstantInt>(CE->getOperand(2))){
                                        elementName += "." + to_string(CI->getZExtValue());
                                        //errs() << "v: " <<elementName << "\n";
                                        StructToElememt[v] = elementName;
                                    }
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
                                    elementName  += "_global_@struct#";
                                    elementName += ptr->getName();
                                    if(ConstantInt *CI = dyn_cast<ConstantInt>(CE->getOperand(2))){
                                        elementName += "." + to_string(CI->getZExtValue());
                                        //errs() << "v: " <<elementName << "\n";
                                        StructToElememt[ptr] = elementName;
                                    }
                                }
                            }
                        }
                    }


                    if ((!isa<GlobalVariable>(v) && isa<Constant>(v)) || 
                        (!isa<GlobalVariable>(ptr) && isa<Constant>(ptr))) {
                        break;
                    }

                    if(ptr->getType()->isPointerTy())
                        ;
                    else {
                        break;
                    }

                    string valueName = "";
                    string ptrName;

                    while(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(ptr)){
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

                	//errs() << "Load : " << I->getName() << "\n";
                }
                break;
                case Instruction::Call:
                {
                    

                	CallInst *CI = dyn_cast<CallInst>(I);

                    Function *FuncCalled = CI->getCalledFunction();

                    string calledFuncName = FuncCalled->getName().str();

                    Value *CalledValue = CI;
                    
                    if (CalledValue->hasName()) {
                        //errs() << "Call:" << CalledValue->getName() << "\n";
                        callValue.push_back(CalledValue);
                    	string calledValueName = CalledValue->getName();
                    	
                        if(isPrint)
                            errs() << "addEdge: " << calledValueName << " = " << calledFuncName << "\n";
                        addNodeValue(calledValueName, isPrint);
                        addNodeValue("#" + calledFuncName, isPrint);
                        addEdge("#" + calledFuncName, calledValueName, edges);
                    }

                    int i = 0;
                    for (Function::arg_iterator FA = FuncCalled->arg_begin(),
                         FAE = FuncCalled->arg_end(); FA != FAE; FA++, i++) {
                        
                        Value *actualArg = CI->getArgOperand(i);
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
                            errs() << "not Pointer:" << actualArgName << "\n";
                        }
                        CallValueBind[formalArgName].push_back(actualArgName);
                    }
                }
                break;
                case Instruction::Ret:
                {
                	ReturnInst *RI = dyn_cast<ReturnInst>(I);
                	if (RI->getNumOperands() != 0) {
                        Value *retVal = RI->getOperand(0);
                        string returnName;
                        if (retVal->hasName()) {
                        	returnName = funcName + "@";
                            returnName += retVal->getName();
                        }else
                        	returnName = TempValues[retVal];

                        if(!returnName.empty()){
                        	
                            if(isPrint)
                                errs() << "addEdge : " << funcName << " = " << returnName << "\n";
                            addNodeValue("#" + funcName, isPrint);
                            addNodeValue(returnName, isPrint);
                            addEdge(returnName, "#" + funcName, edges);
                        }
                    }
                	
                }
                break;

            }
        }
    }
}


char AndersenPA::ID = 0;
static RegisterPass<AndersenPA> X("ander", "Andersen Points To Alias Analysis"
                            " pass",
                            false /* Only looks at CFG */,
                            false /* Analysis Pass */);