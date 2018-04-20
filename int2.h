#ifndef INT2_H_
#define INT2_H_

// A pair of integers. This name could be confused with sized integer types like
// s8 where the 8 signifies 8 bits, not 8 integers.
struct Int2
{
    int x;
    int y;
};

Int2 operator + (Int2 i0, Int2 i1);
Int2& operator += (Int2& i0, Int2 i1);
Int2 operator - (Int2 i0, Int2 i1);
Int2& operator -= (Int2& i0, Int2 i1);

bool operator == (const Int2& a, const Int2& b);
bool operator != (const Int2& a, const Int2& b);

extern const Int2 int2_zero;

#endif // INT2_H_
