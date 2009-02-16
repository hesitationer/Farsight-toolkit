#include <iostream>
#include "GraphCut.h"
#include "mex.h"
#include "matrix.h"

/* memory management */
void* operator new(size_t size)
{
    void *ptr = NULL;
//    mexWarnMsgTxt("Overloaded new operator");
    ptr = malloc(size);//ptr = mxMalloc(size);
    //mxAssert(ptr!=NULL,"memory allocation error");
    //mexMakeMemoryPersistent(ptr);
    return ptr;
}
void* operator new[](size_t size)
{
    void *ptr = NULL;
//    mexWarnMsgTxt("Overloaded new[] operator");
    ptr = malloc(size);//ptr = mxMalloc(size);
    //mxAssert(ptr!=NULL,"memory allocation error");
    //mexMakeMemoryPersistent(ptr);
    return ptr;
}
void operator delete(void* ptr)
{
//    mexWarnMsgTxt("Overloaded delete operator");
    free(ptr);//mxFree(ptr);
}
void operator delete[](void* ptr)
{
//    mexWarnMsgTxt("Overloaded delete[] operator");
    free(ptr);//mxFree(ptr);
}
