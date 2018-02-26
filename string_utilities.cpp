#include "string_utilities.h"

#include "assert.h"
#include "sized_types.h"
#include "math_basics.h"
#include "variable_arguments.h"

#include <cstddef>
#include <cwchar>

int string_size(const char* string)
{
    ASSERT(string);
    const char* s;
    for(s = string; *s; s += 1);
    return s - string;
}

bool strings_match(const char* RESTRICT a, const char* RESTRICT b)
{
    ASSERT(a);
    ASSERT(b);
    while(*a && (*a == *b))
    {
        a += 1;
        b += 1;
    }
    return *a == *b;
}

int copy_string(char* RESTRICT to, int to_size, const char* RESTRICT from)
{
    ASSERT(from);
    ASSERT(to);
    int i;
    for(i = 0; i < to_size - 1; i += 1)
    {
        if(from[i] == '\0')
        {
            break;
        }
        to[i] = from[i];
    }
    to[i] = '\0';
    ASSERT(i < to_size);
    return i;
}

static bool memory_matches(const void* RESTRICT a, const void* RESTRICT b, int n)
{
    ASSERT(a);
    ASSERT(b);
    const unsigned char* p1 = static_cast<const unsigned char*>(a);
    const unsigned char* p2 = static_cast<const unsigned char*>(b);
    for(; n; n -= 1)
    {
        if(*p1 != *p2)
        {
            return false;
        }
        else
        {
            p1 += 1;
            p2 += 1;
        }
    }
    return true;
}

char* find_string(const char* RESTRICT a, const char* RESTRICT b)
{
    ASSERT(a);
    ASSERT(b);
    int n = string_size(b);
    while(*a)
    {
        if(memory_matches(a, b, n))
        {
            return const_cast<char*>(a);
        }
        a += 1;
    }
    return nullptr;
}

int find_char(const char* s, char c)
{
    for(int i = 0; s[i]; i += 1)
    {
        if(s[i] == c)
        {
            return i;
        }
    }
    return -1;
}

bool string_starts_with(const char* RESTRICT a, const char* RESTRICT b)
{
    ASSERT(a);
    ASSERT(b);
    int na = string_size(a);
    int nb = string_size(b);
    if(nb > na)
    {
        return false;
    }
    else
    {
        return memory_matches(a, b, nb);
    }
}

bool string_ends_with(const char* RESTRICT a, const char* RESTRICT b)
{
    ASSERT(a);
    ASSERT(b);
    int na = string_size(a);
    int nb = string_size(b);
    if(nb > na)
    {
        return false;
    }
    else
    {
        return memory_matches(&a[na - nb], b, nb);
    }
}

int count_char_occurrences(const char* string, char c)
{
    int count = 0;
    for(; *string; string += 1)
    {
        count += (*string == c);
    }
    return count;
}

static void reverse_string(char* s, int length)
{
    for(int i = 0, j = length - 1; i < j; i += 1, j -= 1)
    {
        char temp = s[i];
        s[i] = s[j];
        s[j] = temp;
    }
}

static bool is_lower_case(char c)
{
    return c >= 'a' && c <= 'z';
}

static char to_upper_case_char(char c)
{
    if(is_lower_case(c))
    {
        return 'A' + (c - 'a');
    }
    else
    {
        return c;
    }
}

static void to_upper_case(char* string)
{
    for(; *string; string += 1)
    {
        *string = to_upper_case_char(*string);
    }
}

int compare_alphabetic_ascii(const char* RESTRICT a, const char* RESTRICT b)
{
    while(*a && (*a == *b))
    {
        a += 1;
        b += 1;
    }
    char c0 = to_upper_case_char(*a);
    char c1 = to_upper_case_char(*b);
    return c0 - c1;
}

// String To Value..............................................................

static bool is_whitespace(char c)
{
    return c == ' ' || c - 9 <= 5;
}

static int char_to_integer(char c)
{
    if     ('0' <= c && c <= '9') return c - '0';
    else if('a' <= c && c <= 'z') return c - 'a' + 10;
    else if('A' <= c && c <= 'Z') return c - 'A' + 10;
    return 36;
}

static bool string_to_u64(const char* string, char** after, int base, u64* value)
{
    ASSERT(string);
    ASSERT(base >= 0 && base != 1 && base <= 36);

    u64 result = 0;

    const char* s = string;

    while(is_whitespace(*s))
    {
        s += 1;
    }

    bool negative;
    if(*s == '-')
    {
        negative = true;
        s += 1;
    }
    else
    {
        negative = false;
        if(*s == '+')
        {
            s += 1;
        }
    }

    if(base < 0 || base == 1 || base > 36)
    {
        if(after)
        {
            *after = const_cast<char*>(string);
        }
        return false;
    }
    else if(base == 0)
    {
        if(*s != '0')
        {
            base = 10;
        }
        else if(s[1] == 'x' || s[1] == 'X')
        {
            base = 16;
            s += 2;
        }
        else
        {
            base = 8;
            s += 1;
        }
    }

    bool digits_read = false;
    bool out_of_range = false;
    for(; *s; s += 1)
    {
        int digit = char_to_integer(*s);
        if(digit >= base)
        {
            break;
        }
        else
        {
            digits_read = true;
            if(!out_of_range)
            {
                if(result > U64_MAX / base || result * base > U64_MAX - digit)
                {
                    out_of_range = true;
                }
                result = result * base + digit;
            }
        }
    }

    if(after)
    {
        if(!digits_read)
        {
            *after = const_cast<char*>(string);
        }
        else
        {
            *after = const_cast<char*>(s);
        }
    }
    if(out_of_range)
    {
        return false;
    }
    if(negative)
    {
        result = -result;
    }

    *value = result;
    return true;
}

bool string_to_int(const char* string, int* value)
{
    u64 u;
    bool success = string_to_u64(string, nullptr, 0, &u);
    *value = u;
    return success;
}

static bool is_decimal_digit(char c)
{
    return c >= '0' && c <= '9';
}

bool string_to_double(const char* string, double* result)
{
    const char* s = string;

    // Skip any leading whitespace.
    while(is_whitespace(*s))
    {
        s += 1;
    }

    // Parse any sign present.
    double sign = 1.0;
    if(*s == '+')
    {
        s += 1;
    }
    else if(*s == '-')
    {
        sign = -1.0;
        s += 1;
    }

    // Parse digits before the decimal point or exponent.
    double value;
    for(value = 0.0; is_decimal_digit(*s); s += 1)
    {
        value = (10.0 * value) + (*s - '0');
    }

    // Parse digits after the decimal point but before the exponent or the end.
    if(*s == '.')
    {
        s += 1;
        double power_of_ten = 10.0;
        while(is_decimal_digit(*s))
        {
            value += (*s - '0') / power_of_ten;
            power_of_ten *= 10.0;
            s += 1;
        }
    }

    // Parse any exponent present.
    if(*s == 'e' || *s == 'E')
    {
        s += 1;

        // Parse any sign for the exponent.
        bool negative = false;
        if(*s == '+')
        {
            s += 1;
        }
        else if(*s == '-')
        {
            negative = true;
            s += 1;
        }

        // Parse the unsigned exponent.
        int exponent = 0;
        while(is_decimal_digit(*s))
        {
            exponent = (10 * exponent) + (*s - '0');
            s += 1;
        }
        if(exponent > 308)
        {
            exponent = 308;
        }

        // Compute a scaling factor from the exponent and apply it.
        double scale = 1.0;
        for(int i = 0; i < exponent; i += 1)
        {
            scale *= 10.0;
        }
        if(negative)
        {
            value /= scale;
        }
        else
        {
            value *= scale;
        }
    }

    *result = sign * value;
    return true;
}

bool string_to_float(const char* string, float* value)
{
    double d;
    bool success = string_to_double(string, &d);
    *value = d;
    return success;
}

// Value To String..............................................................

namespace
{
    const char* nan_text = "NaN";
    const char* infinity_text = "infinity";
}

const char* bool_to_string(bool b)
{
    if(b)
    {
        return "true";
    }
    else
    {
        return "false";
    }
}

static int s64_to_string(char* string, int size, s64 value, unsigned int base)
{
    if(base < 2 || base > 36)
    {
        base = 10;
    }
    if(base == 10 && value < 0)
    {
        *string = '-';
        string += 1;
        value = -value;
    }

    // generate digits in order of least to most significant
    int i = 0;
    do
    {
        int digit = value % base;
        if(digit < 0xA)
        {
            string[i] = '0' + digit;
        }
        else
        {
            string[i] = 'A' + digit - 0xA;
        }
        i += 1;
        value /= base;
    } while(value && i < size - 1);

    string[i] = '\0';

    // make it read correctly from most to least significant digits
    reverse_string(string, i);

    return i;
}

// Can output at most 20 characters.
int int_to_string(char* string, int size, int value)
{
    return s64_to_string(string, size, value, 10);
}

enum class Form
{
    Use_Shortest,
    Force_Scientific_Notation,
    Force_Decimal,
};

static void double_to_string(char* string, int size, double value, unsigned int precision, Form form, bool force_sign, bool positive_space, bool force_decimal_point)
{
    double n = value;
    // handle special cases
    if(isnan(n))
    {
        copy_string(string, size, nan_text);
    }
    else if(isinf(n))
    {
        copy_string(string, size, infinity_text);
    }
    else if(n == 0.0)
    {
        copy_string(string, size, "0");
    }
    else
    {
        // handle ordinary case
        char* s = string;
        bool negative = n < 0;
        if(negative)
        {
            n = -n;
        }

        // Calculate the magnitude.
        int m = log10(n);
        bool include_exponent;
        switch(form)
        {
            case Form::Force_Scientific_Notation:
            {
                include_exponent = true;
                break;
            }
            case Form::Force_Decimal:
            {
                include_exponent = false;
                break;
            }
            case Form::Use_Shortest:
            {
                include_exponent = m >= 14 || (negative && m >= 9) || m <= -9;
                break;
            }
        }
        if(negative)
        {
            *s = '-';
            s += 1;
        }
        else if(force_sign)
        {
            *s = '+';
            s += 1;
        }
        else if(positive_space)
        {
            *s = ' ';
            s += 1;
        }

        // Set up for scientific notation.
        int m1;
        if(include_exponent)
        {
            if(m < 0) m -= 1.0;
            n /= pow(10.0, m);
            m1 = m;
            m = 0;
        }
        if(m < 1.0)
        {
            m = 0;
        }

        // Convert the number.
        double inverse_precision = pow(0.1, precision);
        while(n > inverse_precision || m >= 0)
        {
            double weight = pow(10.0, m);
            if(weight > 0 && !isinf(weight))
            {
                int digit = floor(n / weight);
                n -= digit * weight;
                *s = '0' + digit;
                s += 1;
            }
            if(m == 0 && n > 0)
            {
                *s = '.';
                s += 1;
            }
            m -= 1;
        }
        if(include_exponent)
        {
            // Convert the exponent.
            *s = 'e';
            s += 1;
            if(m1 > 0)
            {
                *s = '+';
            }
            else
            {
                *s = '-';
                m1 = -m1;
            }
            s += 1;

            m = 0;
            while(m1 > 0)
            {
                *s = '0' + m1 % 10;
                s += 1;
                m1 /= 10;
                m += 1;
            }
            s -= m;

            reverse_string(s, m);
            s += m;
        }
        *s = '\0';
    }
}

void float_to_string(char* string, int size, float value, unsigned int precision)
{
    double_to_string(string, size, value, precision, Form::Use_Shortest, false, false, false);
}

// Formatting...................................................................

namespace
{
    const int digits_cap = 32;

    enum class Length
    {
        Unspecified,
        Byte,
        Short,
        Long,
        Long_Long,
        Int_Max,
        Size_T,
        Ptr_Diff,
        Long_Double,
    };
}

struct FormatContext
{
    char digits[digits_cap];
    const char* format;
    char* buffer;
    Length length;
    int chars_written;
    int buffer_size;
    int digits_count;
    int width;
    int precision;
    bool left_justify;
    bool force_sign;
    bool positive_space;
    bool pound_flag;
    bool left_pad_with_zero;
};

static void default_context(FormatContext* context)
{
    // Make sure this was reset properly.
    ASSERT(context->digits_count == 0);

    context->length = Length::Unspecified;
    context->width = -1;
    context->precision = 6;
    context->left_justify = false;
    context->force_sign = false;
    context->positive_space = false;
    context->pound_flag = false;
    context->left_pad_with_zero = false;
}

static void add_char(FormatContext* context, char c)
{
    if(context->chars_written < context->buffer_size - 1)
    {
        context->buffer[context->chars_written] = c;
    }
    context->chars_written += 1;
}

static void add_digit(FormatContext* context, char digit)
{
    if(context->digits_count < digits_cap - 1)
    {
        context->digits[context->digits_count] = digit;
        context->digits_count += 1;
    }
    else
    {
        // Too many digits!
        ASSERT(false);
    }
}

static bool parse_digits(FormatContext* context, int* value)
{
    // Null-terminate the digit buffer.
    int index = context->digits_count;
    if(index > digits_cap - 1)
    {
        index = digits_cap - 1;
    }
    context->digits[index] = '\0';

    // Reset the counter.
    context->digits_count = 0;

    return string_to_int(context->digits, value);
}

static void find_flags(FormatContext* context)
{
    for(; *context->format; context->format += 1)
    {
        switch(*context->format)
        {
            case '-':
            {
                context->left_justify = true;
                break;
            }
            case '+':
            {
                context->force_sign = true;
                break;
            }
            case ' ':
            {
                context->positive_space = true;
                break;
            }
            case '#':
            {
                context->pound_flag = true;
                break;
            }
            case '0':
            {
                context->left_pad_with_zero = true;
                break;
            }
            default:
            {
                // If anything aside from a flag is found, keep it and exit,
                return;
            }
        }
    }

    // % lacked a matching specifier
    ASSERT(false);
}

static void find_width(FormatContext* context, va_list arguments)
{
    for(; *context->format; context->format += 1)
    {
        char next = *context->format;
        if(is_decimal_digit(next))
        {
            add_digit(context, next);
        }
        else if(next == '*')
        {
            context->width = va_arg(arguments, int);
            return;
        }
        else
        {
            // Anything other than a width means this step is done.
            if(context->digits_count > 0)
            {
                bool success = parse_digits(context, &context->width);
            }
            return;
        }
    }

    // % lacked a matching specifier
    ASSERT(false);
}

static void find_precision(FormatContext* context, va_list arguments)
{
    if(*context->format != '.')
    {
        return;
    }
    else
    {
        // skip past the '.'
        context->format += 1;
    }

    for(; *context->format; context->format += 1)
    {
        char next = *context->format;
        if(is_decimal_digit(next))
        {
            add_digit(context, next);
        }
        else if(next == '*')
        {
            context->precision = va_arg(arguments, int);
            return;
        }
        else
        {
            // Anything other than the precision means this step is done.
            if(context->digits_count > 0)
            {
                bool success = parse_digits(context, &context->precision);
            }
            return;
        }
    }

    // % lacked a matching specifier
    ASSERT(false);
}

static void find_length(FormatContext* context)
{
    for(; *context->format; context->format += 1)
    {
        switch(*context->format)
        {
            case 'h':
            {
                if(context->length == Length::Unspecified)
                {
                    context->length = Length::Short;
                }
                else if(context->length == Length::Short)
                {
                    context->length = Length::Byte;
                    return;
                }
                break;
            }
            case 'l':
            {
                if(context->length == Length::Unspecified)
                {
                    context->length = Length::Long;
                }
                else if(context->length == Length::Long)
                {
                    context->length = Length::Long_Long;
                    return;
                }
                break;
            }
            case 'j':
            {
                context->length = Length::Int_Max;
                return;
            }
            case 'z':
            {
                context->length = Length::Size_T;
                return;
            }
            case 't':
            {
                context->length = Length::Ptr_Diff;
                return;
            }
            case 'L':
            {
                context->length = Length::Long_Double;
                return;
            }
            default:
            {
                // Any other characters mean this step is done.
                return;
            }
        }
    }

    // % lacked a matching specifier
    ASSERT(false);
}

static void process_specifier(FormatContext* context, va_list arguments)
{
    char specifier = *context->format;
    char* buffer = context->buffer + context->chars_written;
    int buffer_left = context->buffer_size - context->chars_written;

    const int int_buffer_size = 21;
    const int float_buffer_size = 32;

    switch(specifier)
    {
        case 'd':
        case 'i':
        {
            char int_buffer[int_buffer_size];
            switch(context->length)
            {
                case Length::Long_Double:
                {
                    // This sub-specifier is just treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                case Length::Byte:
                case Length::Short:
                {
                    int value = va_arg(arguments, int);
                    int_to_string(int_buffer, int_buffer_size, value);
                    break;
                }
                case Length::Long:
                {
                    long value = va_arg(arguments, long);
                    s64_to_string(int_buffer, int_buffer_size, value, 10);
                    break;
                }
                case Length::Long_Long:
                {
                    long long value = va_arg(arguments, long long);
                    s64_to_string(int_buffer, int_buffer_size, value, 10);
                    break;
                }
                case Length::Int_Max:
                {
                    intmax_t value = va_arg(arguments, intmax_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 10);
                    break;
                }
                case Length::Size_T:
                {
                    size_t value = va_arg(arguments, size_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 10);
                    break;
                }
                case Length::Ptr_Diff:
                {
                    ptrdiff_t value = va_arg(arguments, ptrdiff_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 10);
                    break;
                }
            }
            int copied = copy_string(buffer, buffer_left, int_buffer);
            context->chars_written += copied;
            break;
        }
        case 'u':
        {
            char int_buffer[int_buffer_size];
            switch(context->length)
            {
                case Length::Long_Double:
                {
                    // This sub-specifier is just treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                case Length::Byte:
                case Length::Short:
                {
                    unsigned int value = va_arg(arguments, unsigned int);
                    int_to_string(int_buffer, int_buffer_size, value);
                    break;
                }
                case Length::Long:
                {
                    unsigned long value = va_arg(arguments, unsigned long);
                    s64_to_string(int_buffer, int_buffer_size, value, 10);
                    break;
                }
                case Length::Long_Long:
                {
                    unsigned long long value = va_arg(arguments, unsigned long long);
                    s64_to_string(int_buffer, int_buffer_size, value, 10);
                    break;
                }
                case Length::Int_Max:
                {
                    uintmax_t value = va_arg(arguments, uintmax_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 10);
                    break;
                }
                case Length::Size_T:
                {
                    size_t value = va_arg(arguments, size_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 10);
                    break;
                }
                case Length::Ptr_Diff:
                {
                    ptrdiff_t value = va_arg(arguments, ptrdiff_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 10);
                    break;
                }
            }
            int copied = copy_string(buffer, buffer_left, int_buffer);
            context->chars_written += copied;
            break;
        }
        case 'o':
        {
            char int_buffer[int_buffer_size];
            switch(context->length)
            {
                case Length::Long_Double:
                {
                    // This sub-specifier is just treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                case Length::Byte:
                case Length::Short:
                {
                    unsigned int value = va_arg(arguments, unsigned int);
                    s64_to_string(int_buffer, int_buffer_size, value, 8);
                    break;
                }
                case Length::Long:
                {
                    unsigned long value = va_arg(arguments, unsigned long);
                    s64_to_string(int_buffer, int_buffer_size, value, 8);
                    break;
                }
                case Length::Long_Long:
                {
                    unsigned long long value = va_arg(arguments, unsigned long long);
                    s64_to_string(int_buffer, int_buffer_size, value, 8);
                    break;
                }
                case Length::Int_Max:
                {
                    uintmax_t value = va_arg(arguments, uintmax_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 8);
                    break;
                }
                case Length::Size_T:
                {
                    size_t value = va_arg(arguments, size_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 8);
                    break;
                }
                case Length::Ptr_Diff:
                {
                    ptrdiff_t value = va_arg(arguments, ptrdiff_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 8);
                    break;
                }
            }
            int copied = copy_string(buffer, buffer_left, int_buffer);
            context->chars_written += copied;
            break;
        }
        case 'x':
        {
            char int_buffer[int_buffer_size];
            switch(context->length)
            {
                case Length::Long_Double:
                {
                    // This sub-specifier is just treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                case Length::Byte:
                case Length::Short:
                {
                    unsigned int value = va_arg(arguments, unsigned int);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
                case Length::Long:
                {
                    unsigned long value = va_arg(arguments, unsigned long);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
                case Length::Long_Long:
                {
                    unsigned long long value = va_arg(arguments, unsigned long long);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
                case Length::Int_Max:
                {
                    uintmax_t value = va_arg(arguments, uintmax_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
                case Length::Size_T:
                {
                    size_t value = va_arg(arguments, size_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
                case Length::Ptr_Diff:
                {
                    ptrdiff_t value = va_arg(arguments, ptrdiff_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
            }
            int copied = copy_string(buffer, buffer_left, int_buffer);
            context->chars_written += copied;
            break;
        }
        case 'X':
        {
            char int_buffer[int_buffer_size];
            switch(context->length)
            {
                case Length::Long_Double:
                {
                    // This sub-specifier is just treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                case Length::Byte:
                case Length::Short:
                {
                    unsigned int value = va_arg(arguments, unsigned int);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
                case Length::Long:
                {
                    unsigned long value = va_arg(arguments, unsigned long);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
                case Length::Long_Long:
                {
                    unsigned long long value = va_arg(arguments, unsigned long long);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
                case Length::Int_Max:
                {
                    uintmax_t value = va_arg(arguments, uintmax_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
                case Length::Size_T:
                {
                    size_t value = va_arg(arguments, size_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
                case Length::Ptr_Diff:
                {
                    ptrdiff_t value = va_arg(arguments, ptrdiff_t);
                    s64_to_string(int_buffer, int_buffer_size, value, 16);
                    break;
                }
            }
            to_upper_case(int_buffer);
            int copied = copy_string(buffer, buffer_left, int_buffer);
            context->chars_written += copied;
            break;
        }
        case 'f':
        {
            char float_buffer[float_buffer_size];
            switch(context->length)
            {
                default:
                {
                    // Any other sub-specifiers are treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                {
                    double value = va_arg(arguments, double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Force_Decimal, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
                case Length::Long_Double:
                {
                    long double value = va_arg(arguments, long double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Force_Decimal, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
            }
            int copied = copy_string(buffer, buffer_left, float_buffer);
            context->chars_written += copied;
            break;
        }
        case 'F':
        {
            char float_buffer[float_buffer_size];
            switch(context->length)
            {
                default:
                {
                    // Any other sub-specifiers are treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                {
                    double value = va_arg(arguments, double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Force_Decimal, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
                case Length::Long_Double:
                {
                    long double value = va_arg(arguments, long double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Force_Decimal, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
            }
            to_upper_case(float_buffer);
            int copied = copy_string(buffer, buffer_left, float_buffer);
            context->chars_written += copied;
            break;
        }
        case 'e':
        {
            char float_buffer[float_buffer_size];
            switch(context->length)
            {
                default:
                {
                    // Any other sub-specifiers are treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                {
                    double value = va_arg(arguments, double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Force_Scientific_Notation, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
                case Length::Long_Double:
                {
                    long double value = va_arg(arguments, long double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Force_Scientific_Notation, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
            }
            int copied = copy_string(buffer, buffer_left, float_buffer);
            context->chars_written += copied;
            break;
        }
        case 'E':
        {
            char float_buffer[float_buffer_size];
            switch(context->length)
            {
                default:
                {
                    // Any other sub-specifiers are treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                {
                    double value = va_arg(arguments, double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Force_Scientific_Notation, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
                case Length::Long_Double:
                {
                    long double value = va_arg(arguments, long double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Force_Scientific_Notation, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
            }
            to_upper_case(float_buffer);
            int copied = copy_string(buffer, buffer_left, float_buffer);
            context->chars_written += copied;
            break;
        }
        case 'g':
        {
            char float_buffer[float_buffer_size];
            switch(context->length)
            {
                default:
                {
                    // Any other sub-specifiers are treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                {
                    double value = va_arg(arguments, double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Use_Shortest, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
                case Length::Long_Double:
                {
                    long double value = va_arg(arguments, long double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Use_Shortest, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
            }
            int copied = copy_string(buffer, buffer_left, float_buffer);
            context->chars_written += copied;
            break;
        }
        case 'G':
        {
            char float_buffer[float_buffer_size];
            switch(context->length)
            {
                default:
                {
                    // Any other sub-specifiers are treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                {
                    double value = va_arg(arguments, double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Use_Shortest, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
                case Length::Long_Double:
                {
                    long double value = va_arg(arguments, long double);
                    double_to_string(float_buffer, float_buffer_size, value, context->precision, Form::Use_Shortest, context->force_sign, context->positive_space, context->pound_flag);
                    break;
                }
            }
            to_upper_case(float_buffer);
            int copied = copy_string(buffer, buffer_left, float_buffer);
            context->chars_written += copied;
            break;
        }
        case 'a':
        {
            double value = va_arg(arguments, double);
            // Not yet supported!
            ASSERT(false);
            break;
        }
        case 'A':
        {
            double value = va_arg(arguments, double);
            // Not yet supported!
            ASSERT(false);
            break;
        }
        case 'c':
        {
            switch(context->length)
            {
                default:
                {
                    // Any other sub-specifiers are treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                {
                    char value = va_arg(arguments, int);
                    add_char(context, value);
                    break;
                }
                case Length::Long:
                {
                    wint_t value = va_arg(arguments, wint_t);
                    add_char(context, value);
                    break;
                }
            }
            break;
        }
        case 's':
        {
            switch(context->length)
            {
                default:
                {
                    // Any other sub-specifiers are treated as unspecified.
                    ASSERT(false);
                }
                case Length::Unspecified:
                {
                    char* value = va_arg(arguments, char*);
                    context->chars_written += copy_string(buffer, buffer_left, value);
                    break;
                }
                case Length::Long:
                {
                    wchar_t* value = va_arg(arguments, wchar_t*);
                    // Not yet supported!
                    ASSERT(false);
                    break;
                }
            }
            break;
        }
        case 'p':
        {
            // No length sub-specifiers apply here.
            ASSERT(context->length == Length::Unspecified);

            void* value = va_arg(arguments, void*);
            upointer address = reinterpret_cast<upointer>(value);

            char int_buffer[int_buffer_size];
            s64_to_string(int_buffer, int_buffer_size, address, 16);
            int copied = copy_string(buffer, buffer_left, int_buffer);
            context->chars_written += copied;
            break;
        }
        case 'n':
        {
            signed int* value = va_arg(arguments, signed int*);
            ASSERT(value);
            if(value)
            {
                *value = context->chars_written;
            }
            break;
        }
        case '%':
        {
            add_char(context, '%');
            break;
        }
        default:
        {
            // % lacked a matching specifier
            ASSERT(false);
            break;
        }
    }
}

// TODO: Bug report December 31, 2017 - segfault in process_specifier when
// exceeding the buffer_size.
void va_list_format_string(char* buffer, int buffer_size, const char* format, va_list arguments)
{
    FormatContext context = {};
    context.buffer = buffer;
    context.buffer_size = buffer_size;
    context.format = format;
    context.chars_written = 0;

    for(; *context.format; context.format += 1)
    {
        char next = *context.format;
        if(next != '%')
        {
            add_char(&context, next);
        }
        else
        {
            // Step past the '%'.
            context.format += 1;
            next = *context.format;
            if(next)
            {
                // Read the specifier and insert the argument where the
                // specifier was in the format string.
                default_context(&context);
                find_flags(&context);
                find_width(&context, arguments);
                find_precision(&context, arguments);
                find_length(&context);
                process_specifier(&context, arguments);
            }
        }
    }

    // Ensure null-termination of the output buffer.
    int i = context.chars_written;
    if(i >= buffer_size - 1)
    {
        buffer[buffer_size - 1] = '\0';
    }
    else
    {
        buffer[i] = '\0';
    }
}

// This is an *unfinished* rendition of snprintf from <cstdio>. Do not expect
// all options to be supported, yet.
void format_string(char* buffer, int buffer_size, const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    va_list_format_string(buffer, buffer_size, format, arguments);
    va_end(arguments);
}
