#ifndef LOOP_MACROS_H_
#define LOOP_MACROS_H_

#define FOR_N(index, count)\
	for(int index = 0; index < (count); index += 1)

#define FOR_N_BACKWARD(index, count)\
	for(int index = (count) - 1; index >= 0; index -= 1)

#endif // LOOP_MACROS_H_
