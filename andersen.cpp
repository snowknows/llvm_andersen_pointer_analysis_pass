/*
andersen's algorithm and it's data structure
*/
#include <string>
#include <set>
#include <iostream>
#include <list> 
#include <vector>
#include <algorithm>
#include <map>
using namespace std;

class Pointer {
// structure to save pointer p and it's ptrs
public:
    Pointer(string sid, set<Pointer> pts):sid(sid), pts(pts){}
    Pointer(string sid):sid(sid){}
    void str() const{
        cout << "sid: " << sid << ", ";
        set<Pointer>::iterator b = pts.begin();
        set<Pointer>::iterator e = pts.end();
        cout << "pts : { ";
        for(; b != e; b++){
            cout << (*b).sid << ", ";
        }
        cout << "}\n";
    }
    bool operator< (const Pointer& p) const
    {
        if (sid.compare(p.sid) < 0)
            return true;
        else
            return false;
    }
    string sid;
    set<Pointer> pts;

};

struct op{
    string valueName;
    op(string valueName):valueName(valueName){}
};

struct constraint
{
    //structure to save constraint: *a = b or a = *b
    op leftOp;
    op rightOp;
    constraint(op leftOp, op rightOp):leftOp(leftOp),rightOp(rightOp){}   
};

struct edge{
    // structure to save edge: b -> a(a = b)
    string from;
    string to;
    edge(string from, string to):from(from), to(to){}
};


void printInfo(set<Pointer> &nodes, vector<constraint> &constraints, vector<edge> &edges)
{
    //print pointer and it's ptr,constraints and edges
    cout << "nodes:\n";
    set<Pointer>::iterator b = nodes.begin();
    set<Pointer>::iterator e = nodes.end();
    for(; b != e; b++){
        (*b).str();
    }
    cout << "\n";

    cout << "edges:\n";
    vector<edge>::iterator eb = edges.begin();
    vector<edge>::iterator ee = edges.end();
    for(; eb != ee; eb++){
        cout << (*eb).from << " -> " << (*eb).to << "\n";
    }
    cout << "\n";

    cout << "constraints:\n";
    vector<constraint>::iterator cb = constraints.begin();
    vector<constraint>::iterator ce = constraints.end();
    for(; cb != ce; cb++){
        cout << (*cb).leftOp.valueName << " = " << (*cb).rightOp.valueName << "\n";
    }
    cout << "\n";
}

void printWorkQueue(list<string> &workQueue){
    cout << "workQueue:{";
    list<string>::iterator b = workQueue.begin();
    list<string>::iterator e = workQueue.end();
    for(; b != e; b++){
        cout << (*b) << ", ";
    }
    cout << "}\n";
}

bool addEdge(const string &a, const string &p, vector<edge> &edges){
    //convert a = b to b -> a
    vector<edge>::iterator eb = edges.begin();
    vector<edge>::iterator ee = edges.end();
    for(; eb != ee; eb++){
        if( ((*eb).from == a) && ((*eb).to == p) ){
            return false;
        }
    }
    edge a_p(a, p);
    edges.push_back(a_p);
    return true;
}

set<Pointer> getPts(set<Pointer> &nodes, string sid){
    //give a pointer id, get it's ptrs
    set<Pointer>::iterator b = nodes.begin();
    set<Pointer>::iterator e = nodes.end();
    for(; b != e; b++){
        if((*b).sid == sid)
            return (*b).pts;
    }
}

Pointer getPointer(set<Pointer> &nodes, string sid){
    //give a pointer id, get the pointer node
    set<Pointer>::iterator b = nodes.begin();
    set<Pointer>::iterator e = nodes.end();
    for(; b != e; b++){
        if((*b).sid == sid)
            return (*b);
    }
}

void update_nodes(set<Pointer> &nodes, Pointer q){
    //remove the old pointer from nodes, and replace with a new one
    set<Pointer>::iterator b = nodes.begin();
    for(; b != nodes.end(); b++){
        if((*b).sid == q.sid){
            nodes.erase(b);
            nodes.insert(q);
            return;
        }
    }
}

void newNode(string name, set<Pointer> &nodes){
    //initial a node, it's ptr is empty, and insert into pointer nodes
    Pointer node(name);
    nodes.insert(node);
}

void addrNode(string p, string q,  set<Pointer> &nodes){
    //for p = &q, add pointer q in p's ptrs
    Pointer node = getPointer(nodes, q);
    set<Pointer>::iterator b = nodes.begin();
    set<Pointer>::iterator e = nodes.end();
    for(; b != e; b++){
        if( (*b).sid == p ){
            //(*b).pts.insert(node);
            set<Pointer> pts = (*b).pts;
            pts.insert(node);
            Pointer node_p(p, pts);
            update_nodes(nodes, node_p);

            return;
        }
    }
    set<Pointer> pts;
    pts.insert(node);
    Pointer node_p(p, pts);
    nodes.insert(node_p);
}

void addConstraint(string left, string right, vector<constraint> &constraints){
    //for *a = b or a = *b, translate into a constraint
    op op_left(left);
    op op_right(right);
    constraint l_r(op_left, op_right);
    constraints.push_back(l_r);
}

void init(set<Pointer> &nodes, vector<constraint> &constraints, vector<edge> &edges, list<string> &workQueue){
    /*you can write your constraints here, so andersen's algorithm can analyse and give the result*/

    /*example 1*/
    // newNode("a1", nodes);
    // newNode("b1", nodes);
    // newNode("t", nodes);
    // newNode("r", nodes);
    // newNode("a", nodes);
    // newNode("b", nodes);
    // newNode("p", nodes);
    // newNode("q", nodes);
    // addrNode("a", "a1", nodes);
    // addrNode("b", "b1", nodes);
    // addrNode("p", "a", nodes);
    // addrNode("q", "b", nodes);

    // addConstraint("t", "*p", constraints);
    // addConstraint("r", "*q", constraints);
    // addConstraint("*p", "r", constraints);
    // addConstraint("*q", "t", constraints);


    /*example 2*/
    // newNode("main@a1", nodes);
    // newNode("main@b1", nodes);
    // newNode("main@a", nodes);
    // newNode("main@b", nodes);
    // newNode("swap@p", nodes);
    // newNode("swap@q", nodes);
    // newNode("_TEMP_0", nodes);
    // newNode("_TEMP_1", nodes);

    // newNode("_OBJ_0", nodes);
    // newNode("_OBJ_2", nodes);
    // newNode("_OBJ_3", nodes);
    // newNode("_OBJ_1", nodes);

    // addrNode("main@a1", "_OBJ_0", nodes);
    // addrNode("main@b1", "_OBJ_1", nodes);
    // addrNode("main@a", "_OBJ_2", nodes);
    // addrNode("main@b", "_OBJ_3", nodes);

    // addEdge("main@a", "swap@p", edges);
    // addEdge("main@b", "swap@q", edges);

    // addConstraint("*main@a", "main@a1", constraints);
    // addConstraint("*main@b", "main@b1", constraints);
    // addConstraint("_TEMP_0", "*swap@p", constraints);
    // addConstraint("_TEMP_1", "*swap@q", constraints);
    // addConstraint("*swap@p", "_TEMP_1", constraints);
    // addConstraint("*swap@q", "_TEMP_0", constraints);

    // newNode("#swap", nodes);
    // newNode("call", nodes);
    // addEdge("_TEMP_0", "#swap", edges);
    // addEdge("#swap", "call", edges);

    /*example 3*/
    newNode("_OBJ_0", nodes);
    newNode("_OBJ_1", nodes);
    newNode("_OBJ_2", nodes);
    newNode("_OBJ_3", nodes);
    newNode("_OBJ_4", nodes);
    newNode("_OBJ_5", nodes);
    newNode("_OBJ_6", nodes);
    newNode("_OBJ_7", nodes);
    newNode("_OBJ_8", nodes);

    newNode("main@a1", nodes);
    newNode("main@b1", nodes);
    newNode("main@a", nodes);
    newNode("main@b", nodes);
    newNode("main@paddr", nodes);
    newNode("main@qaddr", nodes);
    newNode("main@t", nodes);
    newNode("main@x", nodes);
    newNode("main@y", nodes);

    addrNode("main@a1", "_OBJ_0", nodes);
    addrNode("main@b1", "_OBJ_1", nodes);
    addrNode("main@a", "_OBJ_2", nodes);
    addrNode("main@b", "_OBJ_3", nodes);
    addrNode("main@paddr", "_OBJ_4", nodes);
    addrNode("main@qaddr", "_OBJ_5", nodes);
    addrNode("main@t", "_OBJ_6", nodes);
    addrNode("main@x", "_OBJ_7", nodes);
    addrNode("main@y", "_OBJ_8", nodes);

    newNode("_TEMP_0", nodes);
    newNode("_TEMP_1", nodes);
    newNode("_TEMP_2", nodes);
    newNode("_TEMP_3", nodes);
    newNode("_TEMP_4", nodes);
    newNode("_TEMP_5", nodes);
    newNode("_TEMP_6", nodes);

    addConstraint("*main@a", "main@a1", constraints);
    addConstraint("*main@b", "main@b1", constraints);
    addConstraint("*main@paddr", "main@x", constraints);
    addConstraint("*main@qaddr", "main@y", constraints);
    addConstraint("_TEMP_0", "*main@paddr", constraints);
    addConstraint("_TEMP_1", "*_TEMP_0", constraints);
    addConstraint("*main@t", "_TEMP_1", constraints);
    addConstraint("_TEMP_2", "*main@qaddr", constraints);
    addConstraint("_TEMP_3", "*_TEMP_2", constraints);
    addConstraint("_TEMP_4", "*main@paddr", constraints);
    addConstraint("*_TEMP_4", "_TEMP_3", constraints);
    addConstraint("_TEMP_5", "*main@t", constraints);
    addConstraint("_TEMP_6", "*main@qaddr", constraints);
    addConstraint("*_TEMP_6", "_TEMP_5", constraints);

    addEdge("main@a", "main@x", edges);
    addEdge("main@b", "main@y", edges);


    //init workQueue according to the constraints above
    set<Pointer>::iterator b = nodes.begin();
    set<Pointer>::iterator e = nodes.end();
    for(; b != e; b++){
        Pointer node = *b;
        if (!node.pts.empty()) {
            //node.str();
            workQueue.push_back(node.sid);
        }
    }
}

void calculateConstraints(set<Pointer> &nodes, vector<constraint> &constraints, vector<edge> &edges, list<string> &workQueue, bool isPrint){
    /*the key algorithm of andersen's pointer analysis
    nodes: all pointers and it's ptrs. the variable's ptr is empty
    constraints: contains a* = b, a = *b
    edges: contains a = b
    workQueue: contains all pointer needed to analysed
    isPrint: whether to print running information
    */

    int step = 0; //record the run times
    if(isPrint){ // print nodes, constraints, edges and workQueue
        cout << "step" << step << ":\n";
        printInfo(nodes, constraints, edges);
        printWorkQueue(workQueue);
        cout << "\n\n";
    }
    int newturn = 2; //times to analysis. 2 is a experiential number, it's the minimal times to run

    while(!workQueue.empty()){ 
        //get a pointer to analyse from workQueue
        string v = workQueue.front();
        string star_v = "*" + v;
        if(isPrint)
            cout << "v: " << v << "\n";
        workQueue.pop_front();

        set<Pointer> pts = getPts(nodes,v); //pts is v's pointer to pts

        for (set<Pointer>::iterator pts_b = pts.begin(), pts_e = pts.end(); pts_b != pts_e; pts_b++) {
            // for all v's ptrs
            string a = (*pts_b).sid; // a is a variable name which is pointered by v

            for (vector<constraint>::iterator cons_b = constraints.begin(), cons_e = constraints.end(); cons_b != cons_e; cons_b++) {
                // for all constraints containts *v
                if ((*cons_b).rightOp.valueName == star_v) {
                    // constraint: p = *v
                    Pointer p = getPointer(nodes,(*cons_b).leftOp.valueName);
                    if (addEdge(a, p.sid, edges)) { 
                        if(find(workQueue.begin(), workQueue.end(), v) == workQueue.end())
                            workQueue.push_back(v);
                    } 
                } 
                else if ((*cons_b).leftOp.valueName == star_v){
                    //constraint: *v = q
                    Pointer q = getPointer(nodes,(*cons_b).rightOp.valueName);
                    if (addEdge(q.sid, a, edges) ){ // if we add a new edge, put q in workQueue
                        if(find(workQueue.begin(), workQueue.end(), v) == workQueue.end())
                            workQueue.push_back(q.sid);
                    }
                }
            }
        }
        for (vector<edge>::iterator e_b = edges.begin(); e_b != edges.end(); e_b++) {
            // for all edges, update pointer's ptrs
            if ((*e_b).from == v) { // if edge is v -> q, put all v's ptrs to q's ptrs
                Pointer q = getPointer(nodes, (*e_b).to);
                Pointer v_pts = getPointer(nodes , v);
                int size = q.pts.size();

                set_union(q.pts.begin(), q.pts.end(), v_pts.pts.begin(), v_pts.pts.end(), inserter(q.pts, q.pts.begin())); //q.pts += v.pts

                if (q.pts.size() != size) { //if add new variable to q.pts, update q,and put q in workQueue
                    if(isPrint)
                        cout << "update nodes\n";
                    update_nodes(nodes, q);

                    if(find(workQueue.begin(), workQueue.end(), v) == workQueue.end())
                            workQueue.push_back(q.sid);

                }
            }

        } 

        //print nodes, constraints, edges and workQueue after operate on pointer v 
        step++;
        if(isPrint){
            cout << "step" << step << ":\n";
            printInfo(nodes, constraints, edges);
            printWorkQueue(workQueue);
            cout << "\n\n";
        }

        if(workQueue.empty() && newturn){
            //for a new run, reinitial workQueue
            newturn--;

            for(set<Pointer>::iterator b = nodes.begin(), e = nodes.end(); b != e; b++){
                Pointer node = *b;
                if (!node.pts.empty()) {
                    if(isPrint)
                        cout << "update nodes for workQueue\n";
                    workQueue.push_back(node.sid);
                }
            }
        }
    }
}
bool isContainAt(const string s){
    // all important variable and pointers' sid are labled @, 
    //so it's needed to filter them
    for(string::size_type i = 0, e = s.size();i != e; i++){
        if(s[i] == '@')
            return true;
    }
    return false;
}

// void getPointToInfo(set<Pointer> &nodes, map<string, vector<string>> &nodeToPtrs, bool isPrint){
//     /*it's a translate function only for llvm pass.
//     because llvm's pointer and variable are all connected to it's memory object,
//     so we need to trainslate the memory relationship to pointer-to relationship.

//     nodes: all pointer and variable nodes
//     nodeToPtrs: the result pointer-to relations,pointer and it's pts
//     isPrint:whether to print running imformation
//     */
//     set<Pointer>::iterator i,e;
//     map<string, vector<string> > ObjectToValue; //object to it's address name, that is, variable or pointer

//     for(i = nodes.begin(), e = nodes.end(); i != e; i++){
//         //construct ObjectToValue from nodes
//         if(isContainAt((*i).sid)){
//             set<Pointer> pts = (*i).pts;
//             string objname = (*(pts.begin())).sid;
//             ObjectToValue[objname].push_back((*i).sid);

//         }
//     }
//     for(i = nodes.begin(), e = nodes.end(); i != e; i++){
//         if(isContainAt((*i).sid)){
//             set<Pointer> pts = (*i).pts;
//             if(!pts.empty()){
//                 string objname = (*(pts.begin())).sid;
//                 set<Pointer> objpts = getPts(nodes, objname); // get object's pts
//                 set<Pointer>::iterator si,se;
//                 string vname = (*i).sid.substr(0, (*i).sid.find("addr") - 1); //convert p.addr to p
//                 vector<string> toPtrs; //contains point-to variables

//                 for(si = objpts.begin(), se = objpts.end(); si != se; si++){
//                     string toobjname = (*si).sid;
//                     vector<string> toValue = ObjectToValue[toobjname];
//                     vector<string>::iterator vi,ve;
//                     for(vi = toValue.begin(), ve = toValue.end(); vi != ve; vi++){
//                         toPtrs.push_back(*vi);
//                     }
//                 }
//                 nodeToPtrs[vname] = toPtrs; //pointer to variables map
//             }

//         }
//     }
//     if(isPrint){
//         //print pointer and it's pts
//         //variable's pts in empty.
//         for(map<string, vector<string>>::iterator mi = nodeToPtrs.begin(), me = nodeToPtrs.end(); mi != me; mi++){
//             cout << mi->first << " -> {";
//             for(vector<string>::iterator vi = mi->second.begin(), ve = mi->second.end(); vi != ve; vi++)
//                 cout << (*vi) << ", ";
//             cout << "}\n";
//         }
//     }  
// }

int main(){
    set<Pointer> nodes;
    vector<constraint> constraints;
    vector<edge> edges;       
    list<string> workQueue;
    map<string, vector<string>> nodeToPtrs;
    //int step = 0;

    init(nodes, constraints, edges, workQueue);
    calculateConstraints(nodes, constraints, edges, workQueue, true);
    //getPointToInfo(nodes, nodeToPtrs, true);   
}