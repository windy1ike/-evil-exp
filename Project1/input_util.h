#ifndef INPUT_UTIL_H
#define INPUT_UTIL_H

/* Return codes for read_*_or_back functions */
#define INPUT_OK    1   /* valid input received          */
#define INPUT_QUIT  0   /* user entered /q (cancel flow) */
#define INPUT_EOF  (-1) /* EOF or read error             */
#define INPUT_BACK (-2) /* user entered /b (go back)     */

#ifdef __cplusplus
extern "C" {
#endif

	int read_line_or_back(const char* prompt, char* out, int outSize);
	int read_int_or_back(const char* prompt, int* out);
	int read_double_or_back(const char* prompt, double* out);
	int read_bool01_or_back(const char* prompt, int* out);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_UTIL_H */