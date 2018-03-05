#ifndef UNICODE_H_
#define UNICODE_H_

int utf8_codepoint_count(const char* s);
char32_t utf8_get_codepoint(const char* string, int* bytes_read);
int utf8_get_prior_codepoint(const char* string, int start, char32_t* result);
int utf8_to_utf32(const char* from, int from_size, char32_t* to, int to_size);

#endif // UNICODE_H_
