/*
 * Copyright (C) 2004-2021 Intel Corporation.
 * SPDX-License-Identifier: MIT
 */

//
// This tool counts the number of times a routine is executed and
// the number of instructions executed in a routine
//

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string.h>
#include "pin.H"
//#include "context_utils.h"

#include <stack>
#include <map>


using std::cerr;
using std::dec;
using std::endl;
using std::hex;
using std::ofstream;
using std::setw;
using std::string;

using std::cout;
using std::stack;
using std::map;
using std::pair;


/////////////////////
// GLOBAL VARIABLES
/////////////////////

// A knob for defining which context to test. One of:
// 1. default   - regular CONTEXT passed to the analysis routine using IARG_CONTEXT.
// 2. const     - const CONTEXT passed to the analysis routine using IARG_CONST_CONTEXT.
// 2. partial   - partial CONTEXT passed to the analysis routine using IARG_PARTIAL_CONTEXT.
/*
KNOB< string > KnobTestContext(KNOB_MODE_WRITEONCE, "pintool", "testcontext", "default",
                               "specify which context to test. One of default|const|partial.");

// A knob for defining the output file name
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "context_regvalue.out", "specify output file name");
*/

//ofstream outFile; // for rtn count

// Holds instruction count for a single procedure
typedef struct RtnCount
{
    string _name;
    string _image;
    //ADDRINT _address;
    RTN _rtn;
    //UINT64 _rtnCount;
    //UINT64 _icount;
    //struct RtnCount* _next;
} RTN_COUNT;

// Linked list of instruction counts for each routine
//RTN_COUNT* RtnList = 0;

typedef struct ShadowStackItem
{
    ADDRINT _rbp;
    ADDRINT _ret; //return value
    UINT64 _index; // Store the only index identifier of a item in Stack

} ShadowStackItem; //Define a Stack Item


UINT64 TotalRBP_Num = 0;

map<string, stack<ShadowStackItem> > ShadowStack;


/////////////////////
// ANALYSIS FUNCTIONS
/////////////////////


static void GetFunAddr(CONTEXT * ctxt, RTN_COUNT* rc){

    ADDRINT rip_val;
    PIN_GetContextRegval(ctxt, LEVEL_BASE::REG_RIP, reinterpret_cast<UINT8 *>(&rip_val));
    //ADDRINT rip_val = (ADDRINT)PIN_GetContextReg(ctxt, REG_RIP);
    //ADDRINT TakenIP = (ADDRINT)PIN_GetContextReg(ctxt, REG_INST_PTR);
    //ADDRINT RIP = (ADDRINT)PIN_GetContextReg(ctx, REG_RIP);

    cout<< "\t" << "[\033[34m+\033[0m]" << "" << " \033[34m\033[1mFunction: " << rc->_name
     << " | " << std::hex << "Address: 0x" << rip_val << "\033[0m"<<endl;

}

static void GetRegPre(CONTEXT * ctxt, RTN_COUNT* rc, ADDRINT ret){

    ADDRINT rbp_val; 
    PIN_GetContextRegval(ctxt, LEVEL_BASE::REG_RBP, reinterpret_cast<UINT8 *>(&rbp_val));

    cout<< "[\033[32m*\033[0m] Index: " << TotalRBP_Num << " Is calling " << rc->_name
     << " | " << std::hex << "Pre_REG_RBP: 0x" << rbp_val << " | Supposed Return Address: 0x" << ret << endl;


    ShadowStackItem ssi; //stack item
    ssi._rbp = rbp_val;
    ssi._index = TotalRBP_Num++;

    ssi._ret = ret;

    stack<ShadowStackItem> ss; // stack

    map<string, stack<ShadowStackItem> >::iterator iter = ShadowStack.find(rc->_name);
    if( iter != ShadowStack.end()){
        ss = iter->second;
    }

    ss.push(ssi);
    
    ShadowStack[rc->_name] = ss;

}

static void GetRegAfter(CONTEXT * ctxt,RTN_COUNT* rc){

    ADDRINT rbp_val; 
    PIN_GetContextRegval(ctxt, LEVEL_BASE::REG_RBP, reinterpret_cast<UINT8 *>(&rbp_val));


    ADDRINT rip_val;
    PIN_GetContextRegval(ctxt, LEVEL_BASE::REG_RIP, reinterpret_cast<UINT8 *>(&rip_val));
    

    map<string, stack<ShadowStackItem> >::iterator iter = ShadowStack.find(rc->_name);
    stack<ShadowStackItem> ss;

    if( iter != ShadowStack.end()){
        ss = iter->second;
    }else{
        cout<< "[\033[31mx\033[0m] [\033[31mFatel Error!\033[0m]   " << " After calling " << rc->_name << 
                std::hex << "After_REG_RBP: 0x" << rbp_val << endl;
        exit(-1);
    }

    ShadowStackItem ssi = ss.top();
  
     cout<< "[\033[32m*\033[0m] Index: " << ssi._index << " After calling " << rc->_name << 
        "  stack depth: " << ss.size() << endl;
    

    cout<< "\t" << "[\033[32m+\033[0m]" << 
            std::hex << " Prev_REG_RBP: 0x" << ssi._rbp << " | Supposed Return Address: 0x" << ssi._ret <<  endl;

    
    if(ssi._rbp != rbp_val){
        cout << "\t" << "[\033[31mx\033[0m] [\033[31mWRONG!\033[0m]   " 
                << std::hex << "After_REG_RBP: 0x" << rbp_val << endl;

        cout << "[\033[31m*\033[0m] YOU WISH!!" << endl;
        exit(-2);

    }else{
        cout<< std::hex << "\t" << "[\033[32m+\033[0m] After_REG_RBP: 0x" << rbp_val << endl;
    }


    if(ssi._ret != rip_val){
        cout << "\t" << "[\033[31mx\033[0m] [\033[31mWRONG!\033[0m]   " 
                << std::hex << " RETURN_ADDR: 0x" << rip_val << endl;

        cout << "[\033[31m*\033[0m] YOU WISH!!" << endl;
        exit(-3);
    }else{
        cout<< std::hex << "\t" << "[\033[32m+\033[0m] RETURN_ADDR: 0x" << rip_val << endl;
    }


    ss.pop();
    if(ss.empty()){
        ShadowStack.erase(iter);
    }else{
        ShadowStack[rc->_name] = ss;
    }
    
}



// This function is called before every instruction is executed
//VOID docount(UINT64* counter) { (*counter)++; }

const char* StripPath(const char* path)
{
    const char* file = strrchr(path, '/');
    if (file)
        return file + 1;
    else
        return path;
}

// Pin calls this function every time a new rtn is executed
VOID Routine(RTN rtn, VOID* v)
{
    // Allocate a counter for this routine
    RTN_COUNT* rc = new RTN_COUNT;

    // The RTN goes away when the image is unloaded, so save it now
    // because we need it in the fini
    rc->_name     = RTN_Name(rtn);
    rc->_image    = StripPath(IMG_Name(SEC_Img(RTN_Sec(rtn))).c_str());
    //rc->_address  = RTN_Address(rtn);
    //rc->_icount   = 0;
    //rc->_rtnCount = 0;

    // Add to list of routines
    //rc->_next = RtnList;
    //RtnList   = rc;

    //only care about non-libc function
    if(rc->_image != "ld-linux-x86-64.so.2" && rc->_image != "libc.so.6" && rc->_image != "[vdso]" && rc->_name[0]!='.'){   
        RTN_Open(rtn);
	
        //Insert before function call
        RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)GetRegPre, IARG_CONTEXT, IARG_PTR, rc, IARG_RETURN_IP, IARG_END);
        
	    //Insert after function call, before ret instruction. (After leave instruction)
        //RTN_InsertCall(rtn, IPOINT_AFTER, (AFUNPTR)GetRegAfter, IARG_CONTEXT, IARG_PTR, rc,IARG_END); 

        //Find first instruction
        INS_InsertCall(RTN_InsHead(rtn), IPOINT_BEFORE, (AFUNPTR)GetFunAddr, IARG_CONTEXT, IARG_PTR, rc, IARG_END);

        //Find ret instruction, and insert after ret instruction.
        for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins))
        {
            // if(INS_IsCall(ins)){
            //     //insert every call instruction
            //     INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)GetFunAddr, IARG_CONTEXT, IARG_PTR, rc, IARG_END);
            // }

            if (INS_IsRet(ins))
            {
                // 插桩每条return指令
                // IPOINT_TAKEN_BRANCH总是最后使用
                //INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)Before, IARG_CONTEXT, IARG_END);
                INS_InsertCall(ins, IPOINT_TAKEN_BRANCH, (AFUNPTR)GetRegAfter, IARG_CONTEXT, IARG_PTR, rc,IARG_END);
            }
        }
        

        // Insert a call at the entry point of a routine to increment the call count

        //RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)docount, IARG_PTR, &(rc->_rtnCount), IARG_END);

        RTN_Close(rtn);
    }

    
}

// This function is called when the application exits
// It prints the name and count for each procedure
VOID Fini(INT32 code, VOID* v)
{
    cout<< "Finish" << endl;
    /*
    outFile << setw(23) << "Procedure"
            << " " << setw(15) << "Image"
            << " " << setw(18) << "Address"
            << " " << setw(12) << "Calls"
            << " " << setw(12) << "Instructions" << endl;
    //cout << RtnList << endl;
    for (RTN_COUNT* rc = RtnList; rc; rc = rc->_next)
    {
        //if (rc->_icount > 0 || 1)
        if(rc->_rtnCount > 0)
            outFile << setw(23) << rc->_name << " " << setw(15) << rc->_image << " " << setw(18) << hex << rc->_address << dec
                    << " " << setw(12) << rc->_rtnCount << " " << setw(12) << rc->_icount << endl;
    }
    */
}

/* ===================================================================== */
/* Print Help Message                                                    */
/* ===================================================================== */

INT32 Usage()
{
    // cerr << "This Pintool counts the number of times a routine is executed" << endl;
    // cerr << "and the number of instructions executed in a routine" << endl;
    // cerr << endl << KNOB_BASE::StringKnobSummary() << endl;
    // return -1;
    cerr << "This Pintool will protect programs from buffer overflow." << endl;
    cerr << "Usage: pin -t path/to/obj-intel64/Buffer_Overflow_Protector.so  -- path/to/program/program_name" << endl;
    return -1;
}

/* ===================================================================== */
/* Main                                                                  */
/* ===================================================================== */

int main(int argc, char* argv[])
{
    // Initialize symbol table code, needed for rtn instrumentation
    PIN_InitSymbols();

    //outFile.open("proccount.out");
    //OutFile.open("reg_check.out");

    // Initialize pin
    if (PIN_Init(argc, argv)) return Usage();

    // Register Routine to be called to instrument rtn
    RTN_AddInstrumentFunction(Routine, 0);

    // Register Fini to be called when the application exits
    PIN_AddFiniFunction(Fini, 0);

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}
