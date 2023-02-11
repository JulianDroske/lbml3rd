#include "stdint.h"
#include "stdarg.h"
#include "stdlib.h"
#include "string.h"
#include "jurt.h"



/* ================ Universal Utils ================ */
void nano_sleep(long nanoseconds){
	#ifdef WINDOWS
		/* Declarations */
		HANDLE timer;	/* Timer handle */
		LARGE_INTEGER li;	/* Time defintion */
		/* Create timer */
		if(!(timer = CreateWaitableTimer(NULL, TRUE, NULL)))
			return;
		/* Set timer properties */
		li.QuadPart = -ns;
		if(!SetWaitableTimer(timer, &li, 0, NULL, NULL, FALSE)){
			CloseHandle(timer);
			return;
		}
		/* Start & wait for timer */
		WaitForSingleObject(timer, INFINITE);
		/* Clean resources */
		CloseHandle(timer);
		/* Slept without problems */
		// return TRUE;
	#else
		// tv_sec, tv_nsec
		struct timespec ns = {nanoseconds/(long)1e9, nanoseconds%(long)1e9};
		nanosleep(&ns, NULL);
	#endif
}

/* ================ JuRt Logger Format ================ */
void l_inf(char* format, ...){
	va_list va;
	int len = strlen(format);
	char* newStr = (char*) malloc((len+sizeof(INF_STARTSTR)+sizeof(INFERR_ENDSTR))*sizeof(char));
	sprintf(newStr, INF_STARTSTR "%s" INFERR_ENDSTR, format);
	va_start(va, format);
	vprintf(newStr, va);
	va_end(va);
	free(newStr);
}

void l_err(char* format, ...){
	va_list va;
	int len = strlen(format);
	char* newStr = (char*) malloc((len+sizeof(ERR_STARTSTR)+sizeof(INFERR_ENDSTR))*sizeof(char));
	sprintf(newStr, ERR_STARTSTR "%s" INFERR_ENDSTR, format);
	va_start(va, format);
	vprintf(newStr, va);
	va_end(va);
	free(newStr);
}



/* ================ Assert ================ */

int64_t assertp(int64_t fd, char* msg, ERRFUNC cb, int terminate){
	if(fd > 0) return fd;
	l_inf("assertp: %s", msg);
	if(cb) cb(fd);
	if(terminate){
		l_err("program terminated.");
		exit(fd|1);
	}
	return fd;
}

int64_t assert0(int64_t fd, char* msg, ERRFUNC cb, int terminate){
	if(fd == 0) return fd;
	l_inf("assert0: %s", msg);
	if(cb) cb(fd);
	if(terminate){
		l_err("program terminated.");
		exit(fd|1);
	}
	return fd;
}

int64_t assert0p(int64_t fd, char* msg, ERRFUNC cb, int terminate){
	if(fd >= 0) return fd;
	l_inf("assert0: %s", msg);
	if(cb) cb(fd);
	if(terminate){
		l_err("program terminated.");
		exit(fd|1);
	}
	return fd;
}



/* ================ Dynamic Array Helper Library ================ */

int DA_isInvalidArray(DynamicArrayPtr da){
	return da->array == 0;
}

DynamicArrayPtr DA_create(int size_per_item){
	int act_chunksiz = size_per_item * D_ARRAY_CHUNK_SIZE;
	void* array = malloc(act_chunksiz);
	if(!array){
		l_err("cannot alloc mem for array.");
		return NULL;
	}
	DynamicArray da = {
		array,
		0,
		size_per_item,
		act_chunksiz,
		act_chunksiz
	};
	DynamicArrayPtr daptr = malloc(sizeof(DynamicArray));
	memcpy(daptr, &da, sizeof(DynamicArray));
	return daptr;
}

void* DA_get(DynamicArrayPtr da, int index){
	if(!da->array){
		l_err("DA_get: array is null.");
		return NULL;
	}
	if(index >= da->len){
		l_err("index %d out of range.", index);
		return NULL;
	}
	if(index < 0){
		l_err("index < 0.");
		return NULL;
	}
	return da->array+index*da->item_size;
}

int DA_get_int(DynamicArrayPtr da, int index){
	void* data = DA_get(da, index);
	if(!data){
		l_err("DA_get got an error.");
		return -1;
	}
	int integer = *(int*)data;
	// free(data);
	return integer;
}

// TODO check bug
void DA_push(DynamicArrayPtr da, void* item){
		// l_inf("push0, len=%d, chunk_size=%d, itemsiz=%d, alloc=%d", da->len, da->chunk_size, da->item_size, da->alloc_size);
	if((da->len+1)*da->item_size > da->alloc_size){
		l_inf("DA_push: %d, %d", da->alloc_size, da->chunk_size);
		void* new_da = realloc(da->array, da->alloc_size += da->chunk_size);
		// l_inf("push2");
		if(!new_da){
			l_err("cannot alloc memory for pushing");
			return;
		}
		da->array = new_da;
	}
		// l_inf("push3");
	memcpy(da->array+da->len*da->item_size, item, da->item_size);
		// l_inf("push4");
	++da->len;
}

void* DA_pop(DynamicArray* da){
	if(da->len == 0) return NULL;
	void* data = malloc(da->item_size);
	memcpy(data, da->array+(da->len-1)*da->item_size, da->item_size);
	--da->len;
	if(da->len%D_ARRAY_CHUNK_SIZE == D_ARRAY_CHUNK_SIZE-1){
		// free
		void* new_arr = realloc(da->array, da->alloc_size -= da->chunk_size);
		if(!new_arr){
			l_err("cannot realloc for DA_pop");
		}else da->array = new_arr;
	}
	return data;
}

int DA_pop_int(DynamicArray* da){
	void* data = DA_pop(da);
	if(!data){
		l_err("DA_pop got an error.");
		return -1;
	}

	int integer = *(int*)(data);
	free(data);
	return integer;
}

void DA_free(DynamicArray* da){
	if(da->array){
		free(da->array);
		da->array = NULL;
	}
}


/* ================ PThread Helper Library ================ */

#ifdef JURT_ENABLE_PTHREAD
/*
	++++++++ NON_EXPORTED ++++++++
	to store thread ids
	[0] stores nothing
*/
#ifdef WINDOWS
	HANDLE i_thread_ids[THREAD_MAX_COUNT];
#else
	pthread_t i_thread_ids[THREAD_MAX_COUNT];
#endif

/*
	++++++++ NON_EXPORTED ++++++++
	to identify using threads
	[0] stores the count of using threads
*/
int i_thread_ids_using[THREAD_MAX_COUNT];

/*
	++++++++ NON_EXPORTED ++++++++
	clean up a thread
	Usage:
		thread_cleanup(id)
	Returns:
		0 as success, other as error
*/
int thread_cleanup(int);

/*
	++++++++ NON_EXPORTED ++++++++
	thread wrapper arg
*/
struct thread_wrapper_arg{
	int id;
	THREAD_INNER_FUNC func;
	void* args;
};
typedef struct thread_wrapper_arg thread_wrapper_arg;

/*
	++++++++ NON_EXPORTED ++++++++
	thread wrapper
	Usage:
		_THREAD_WRAPPER(innerFunc, args);
*/
#ifdef WINDOWS
	DWORD WINAPI _THREAD_WRAPPER(LPVOID);
#else
	void* _THREAD_WRAPPER(void*);
#endif

#ifdef WINDOWS
	HANDLE* getThreadById(int id){
#else
	pthread_t* getThreadById(int id){
#endif
	if(i_thread_ids_using[id]) return &i_thread_ids[id];
	return NULL;
}

int thread_cleanup(int id){
	#ifdef WINDOWS
		l_err("impl");
	#else
		pthread_t* thp = NULL;
		if(!(thp=(pthread_t*)assertp(
			(long)getThreadById(id),
			"cleanup: cannot find thread",
			NULL, 0
		))) return 1;

		pthread_t thread = *thp;

		int ret = pthread_kill(thread, 0);
		if(ret && ret != ESRCH){
			l_err("thread %d is still alive, cannot cleanup.", id);
			return 2;
		}
	#endif

	i_thread_ids_using[id] = 0;
	--i_thread_ids_using[0];

	l_inf("thread %d cleaned up.", id);
	return 0;
}

#ifdef WINDOWS
DWORD WINAPI _THREAD_WRAPPER(LPVOID args){
#else
void* _THREAD_WRAPPER(void* args){
#endif
	thread_wrapper_arg arg = *(thread_wrapper_arg*)args;
	l_inf("thread %d started.", arg.id);
	void* ret = arg.func(arg.args);
	l_inf("thread %d end.", arg.id);
	// TODO on thread end ?
	thread_cleanup(arg.id);
	return ret?ret:"normal exit";
}


int createThread(THREAD_INNER_FUNC func, void* args, int nodetach){
	if(i_thread_ids_using[0]>=THREAD_MAX_COUNT-1){
		l_err("thread pool is full");
		return 0;
	}

	#ifdef WINDOWS
		HANDLE thread;
	#else
		pthread_t thread;
	#endif

	// find a free pool
	int id = 0;
	for(int i=1; i<THREAD_MAX_COUNT; ++i){
		if(!i_thread_ids_using[i]){
			id = i;
			break;
		}
	}
	if(!id){
		l_err("cannot find a free thread pool.");
		return 0;
	}

	// prepare args
	thread_wrapper_arg arg;
	arg.args = args;
	arg.func = func;
	arg.id = id;


	#ifdef WINDOWS
		thread = CreateThread(NULL, 0, _THREAD_WRAPPER, &arg, 0, 0);
	#else
		pthread_attr_t attr;
		pthread_attr_init(&attr);
		if(!nodetach) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
		pthread_create(&thread, &attr, _THREAD_WRAPPER, (void*)&arg);
		pthread_attr_destroy(&attr);
	#endif


	// put
	i_thread_ids[id] = thread;
	i_thread_ids_using[id] = 1;
	++i_thread_ids_using[0];


	l_inf("thread %d created.", id);

	return id;
}

void* waitThread(int id){
	void* res = NULL;
	#ifdef WINDOWS
		HANDLE* threadp = getThreadById(id);
		HANDLE thread;
	#else
		pthread_t* threadp = getThreadById(id);
		pthread_t thread;
	#endif

	if(!threadp){
		l_err("waitThread: cannot find thread %d.", id);
		return NULL;
	}
	thread = *threadp;

	#ifdef WINDOWS
		l_inf("waitThread: Win32 does not support getting return value from threads.");
		DWORD stat = WaitForSingleObject(thread, INFINITE);
		if(stat != WAIT_OBJECT_0){
			l_err("cannot wait for thread %d, error code=%d.", id, stat);
		}
	#else
		int stat = pthread_join(thread, &res);
		if(stat){
			l_err("cannot wait for thread %d, error code=%d.", id, stat);
		}
	#endif
	return res;
}

int killThread(int id){
	#ifdef WINDOWS
		l_err("impl");
		return -1;
	#else
		pthread_t* threadp = getThreadById(id);
		if(!threadp){
			l_err("killThread: cannot find thread %d.", id);
			return 1;
		}

		pthread_t thread = *threadp;

		if(assert0(
			(long)pthread_cancel(thread),
			"cannot kill thread.",
			NULL, 0
		)){
			l_inf("thread %d may be killed.", id);
			return 0;
		}
	#endif
	thread_cleanup(id);
	return 2;
}
#endif // JURT_ENABLE_PTHREAD
