#ifndef SORTING_H_
#define SORTING_H_

#define SWAP(type, a, b)\
    do { type temp = (a); (a) = (b); (b) = temp; } while(0)

#define DEFINE_INSERTION_SORT(type, before, suffix)\
    static void insertion_sort_##suffix(type* a, int count)\
    {\
        for(int i = 1; i < count; ++i)\
        {\
            type temp = a[i];\
            int j;\
            for(j = i; j > 0 && before(temp, a[j - 1]); --j)\
            {\
                a[j] = a[j - 1];\
            }\
            a[j] = temp;\
        }\
    }

#define DEFINE_QUICK_SORT(type, before, suffix)\
    static void quick_sort_innards(type* a, int left, int right)\
    {\
        while(left + 16 < right)\
        {\
            int middle = (left + right) / 2;\
            type median;\
            if(before(a[left], a[right]))\
            {\
                if(before(a[middle], a[left]))\
                {\
                    median = a[left];\
                }\
                else if(before(a[middle], a[right]))\
                {\
                    median = a[middle];\
                }\
                else\
                {\
                    median = a[right];\
                }\
            }\
            else\
            {\
                if(before(a[middle], a[right]))\
                {\
                    median = a[right];\
                }\
                else if(before(a[middle], a[left]))\
                {\
                    median = a[middle];\
                }\
                else\
                {\
                    median = a[left];\
                }\
            }\
            int i = left - 1;\
            int j = right + 1;\
            int pivot;\
            for(;;)\
            {\
                do {j -= 1;} while(before(median, a[j]));\
                do {i += 1;} while(before(a[i], median));\
                if(i >= j)\
                {\
                    pivot = j;\
                    break;\
                }\
                else\
                {\
                    SWAP(type, a[i], a[j]);\
                }\
            }\
            quick_sort_innards(a, left, pivot);\
            left = pivot + 1;\
        }\
    }\
    \
    DEFINE_INSERTION_SORT(type, before, suffix);\
    \
    static void quick_sort_##suffix(type* a, int count)\
    {\
        quick_sort_innards(a, 0, count - 1);\
        insertion_sort_##suffix(a, count);\
    }

#define DEFINE_HEAP_SORT(type, before, suffix)\
    static void sift_down_##suffix(type* a, int left, int right)\
    {\
        int root = left;\
        while(2 * root + 1 <= right)\
        {\
            int child = 2 * root + 1;\
            int index = root;\
            if(before(a[index], a[child]))\
            {\
                index = child;\
            }\
            if(child + 1 <= right && before(a[index], a[child + 1]))\
            {\
                index = child + 1;\
            }\
            if(index == root)\
            {\
                return;\
            }\
            else\
            {\
                SWAP(type, a[index], a[root]);\
                root = index;\
            }\
        }\
    }\
    static void heap_sort_##suffix(type* a, int count)\
    {\
        for(int left = floor((count - 2) / 2); left >= 0; --left)\
        {\
            sift_down_##suffix(a, left, count - 1);\
        }\
        for(int right = count - 1; right > 0;)\
        {\
            SWAP(type, a[right], a[0]);\
            right -= 1;\
            sift_down_##suffix(a, 0, right);\
        }\
    }

#define REVERSE_ARRAY(type, array, count) \
    do \
    { \
        for(int start = 0, end = (count) - 1; \
                start < end; \
                start += 1, end -= 1) \
            SWAP(type, (array)[start], (array)[end]); \
    } while(0)

#endif // SORTING_H_
