#ifndef ARRAY_H_
#define ARRAY_H_

#include "memory.h"

// Experimental auto-expanding array type
#define DEFINE_ARRAY_WITH_SUFFIX(type, suffix)\
	struct Array##suffix\
	{\
		type* items;\
		Heap* heap;\
		int count;\
		int cap;\
		type& operator [] (int index) {return this->items[index];}\
		const type& operator [] (int index) const {return this->items[index];}\
	};\
	bool reserve(Array##suffix* array, int extra)\
	{\
		while(array->count + extra >= array->cap)\
		{\
			if(array->cap == 0)\
			{\
				array->cap = 10;\
			}\
			else\
			{\
				array->cap *= 2;\
			}\
			type* new_array = HEAP_REALLOCATE(array->heap, type, array->items, array->cap);\
			if(!new_array)\
			{\
				return false;\
			}\
			array->items = new_array;\
		}\
		return true;\
	}\
	void create(Array##suffix* array, Heap* heap)\
	{\
		array->heap = heap;\
		array->items = nullptr;\
		array->count = 0;\
		array->cap = 0;\
		reserve(array, 1);\
	}\
	void destroy(Array##suffix* array)\
	{\
		if(array)\
		{\
			HEAP_DEALLOCATE(array->heap, array->items);\
			array->items = nullptr;\
			array->heap = nullptr;\
		}\
	}\
	void add(Array##suffix* array, type item)\
	{\
		(*array)[array->count] = item;\
		array->count += 1;\
	}\
	void add_and_expand(Array##suffix* array, type item)\
	{\
		add(array, item);\
		reserve(array, 1);\
	}

#define DEFINE_ARRAY(type)\
	DEFINE_ARRAY_WITH_SUFFIX(type, type)

#endif // ARRAY_H_
