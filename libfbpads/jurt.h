#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#ifdef _WIN32
	#define WINDOWS
#else
	#define LINUX
#endif

#ifdef WINDOWS
	#include "windows.h"
#else
	#include "pthread.h"
	#include "errno.h"
	#include "signal.h"
#endif



/* ================ Precompile Helper ================ */

#ifdef _WIN32
	#define IMPORTF_PREV(chunkName,suff) binary_##chunkName##suff
#else
	#define IMPORTF_PREV(chunkName,suff) _binary_##chunkName##suff
#endif

#define IMPORTF_PREV_START(n) IMPORTF_PREV(n,_start)
#define IMPORTF_PREV_END(n) IMPORTF_PREV(n,_end)
#define IMPORTF_PREV_SIZE(n) (&IMPORTF_PREV_END(n)-&IMPORTF_PREV_START(n))


#define IMPORTF_AUTOV(name) \
	extern char IMPORTF_PREV_START(name);\
	extern char IMPORTF_PREV_END(name);

/* [================ Precompile Helper ================] */

/*
	nanosleep port
	Usage:
		nano_sleep(nanoseconds)
*/
void nano_sleep(long);

/* ================ Universal Utils ================ */




/* ================ JuRt Logger Format ================ */

/*
	Static strings
*/
#define INF_STARTSTR "\x1b[33;40m"
#define ERR_STARTSTR "\x1b[30;43m"
#define INFERR_ENDSTR "\x1b[0;0m\n"

/*
	Show information
	Usage:
		l_inf(msg, ...)
*/
void l_inf(char*, ...);

/*
	Show an error
	Usage:
		l_err(msg, ...)
*/
void l_err(char*, ...);

/* [================ JuRt Logger Format ================] */



/* ================ Assert ================ */
/* to check if returned value is valid */

/*
	callback function type to deal with error
	Args:
		vl: vl@assert?
*/
typedef void (*ERRFUNC)(int64_t);

/*
	check if bigger than 0
	Usage:
		assertp(vl, errmsg, callbackFunc, terminate)
	Returns:
		vl@asserp
*/
int64_t assertp(int64_t, char*, ERRFUNC, int);

/*
	check if equals 0
	Usage:
		assert0(vl, errmsg, callbackFunc, terminate)
	Returns:
		vl@assert0
*/
int64_t assert0(int64_t, char*, ERRFUNC, int);

/*
	check if bigger or equals 0
	Usage:
		assert0p(vl, errmsg, callbackFunc, terminate)
	Returns:
		vl@assert0
*/
int64_t assert0p(int64_t, char*, ERRFUNC, int);

/* [================ Assert ================] */



/* ================ Dynamic Array Helper Library ================ */

/* chunk size of array, also minimum size of array  */
#define D_ARRAY_CHUNK_SIZE 16

/* dynamic array structure, used to manage an array */
typedef struct {
	void* array;
	int len;
	int item_size;
	int chunk_size;
	int alloc_size;
} DynamicArray;

typedef DynamicArray* DynamicArrayPtr;

/*
	check if a dynamic array is invalid (creation failure)
	Usage:
		DA_isInvalidArray(da)
	Returns:
		1 if invalid, otherwise 0
*/
int DA_isInvalidArray(DynamicArrayPtr);

/*
	create a dynamic array
	Usage:
		DA_create(size_per_item)
	Returns:
		DynamicArray, DynamicArray.array=NULL if error
*/
DynamicArray* DA_create(int);

/*
	+ need to be free()ed? +
	get an item from DynamicArray
	Usage:
		DA_get(da, index)
	Returns:
		address of item, NULL if an error occured
*/
void* DA_get(DynamicArrayPtr, int);

/*
	get an int from DynamicArray
	TODO: returns another value instead of -1
	Usage:
		DA_get_int(da, index)
	Returns:
		value of item and -1 if an error occured
*/
int DA_get_int(DynamicArrayPtr, int);

/*
	push a duplicated value at the end of the array
	Usage:
		DA_push(da, data)
*/
void DA_push(DynamicArrayPtr, void*);

/*
	++++++++ need to be free()ed ++++++++
	pop from an array
	Usage:
		DA_pop(da)
	Returns:
		value of the last item, or NULL if none left
*/
void* DA_pop(DynamicArrayPtr);

/*
	pop an integer from an array
	TODO: returns another value instead of -1
	Usage:
		DA_pop_int(da)
	Returns:
		value of the last item and -1 if an error occured
*/
int DA_pop_int(DynamicArrayPtr);

/*
	free the entire array
	Usage:
		DA_free(da)
*/
void DA_free(DynamicArrayPtr);

/* [================ Dynamic Array Helper Library ================] */



/* ================ PThread Helper Library ================ */

#ifdef JURT_ENABLE_PTHREAD
/* callback function as init */
typedef void* (*THREAD_INNER_FUNC)(void*);

/* thread max count */
#define THREAD_MAX_COUNT 256

/*
	get pthread_t by id:index
	Usage:
		getThreadById(id)
	Returns:
		typed *; otherwise 0(NULL)
*/
#ifdef WINDOWS
	HANDLE* getThreadById(int);
#else
	pthread_t* getThreadById(int);
#endif

/*
	create a thread with pthread
	Usage:
		createThread(threadFunction, args, doNotDetach)
	Returns:
		id, bigger than 0, or other values as errors
*/
int createThread(THREAD_INNER_FUNC, void*, int);

/*
	wait for an undetached thread to complete
	Usage:
		waitThread(id)
	Returns:
		value that thread returns, or NULL when no return or error
*/
void* waitThread(int);

/*
	destroy a thread
	Usage:
		killThread(id)
	Returns:
		0 as success, other as error
*/
int killThread(int);
#endif // JURT_ENABLE_PTHREAD

/* [================ PThread Helper Library ================] */
