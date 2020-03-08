// TODO! replace this with scanf and printf calls in Orb after string literals
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#else
#define DLLEXPORT
#endif

#include <cstdio>
#include <cinttypes>

#define IO_FUNCS(type, keyword, print_format, scan_format) extern "C" DLLEXPORT type scan_##keyword() {\
    type x;\
    std::scanf("%" scan_format, &x);\
    return x;\
}\
\
extern "C" DLLEXPORT void print_##keyword(type x) {\
    std::printf("%" print_format, x);\
}\
\
extern "C" DLLEXPORT void println_##keyword(type x) {\
    std::printf("%" print_format "\n", x);\
}

extern "C" DLLEXPORT char scan_c8(char x) {
    return getchar();
}

extern "C" DLLEXPORT void print_c8(char x) {
    putchar(x);
}

extern "C" DLLEXPORT void println_c8(char x) {
    putchar(x);
    std::printf("\n");
}

extern "C" DLLEXPORT void println(void) {
    std::printf("\n");
}

extern "C" DLLEXPORT void print_str(const char *str) {
    std::printf("%s", str);
}

extern "C" DLLEXPORT void println_str(const char *str) {
    std::printf("%s\n", str);
}

IO_FUNCS(std::int8_t, i8, PRId8, SCNd8)
IO_FUNCS(std::int16_t, i16, PRId16, SCNd16)
IO_FUNCS(std::int32_t, i32, PRId32, SCNd32)
IO_FUNCS(std::int64_t, i64, PRId64, SCNd64)

IO_FUNCS(std::uint8_t, u8, PRIu8, SCNu8)
IO_FUNCS(std::uint16_t, u16, PRIu16, SCNu16)
IO_FUNCS(std::uint32_t, u32, PRIu32, SCNu32)
IO_FUNCS(std::uint64_t, u64, PRIu64, SCNu64)

IO_FUNCS(float, f32, ".4f", "f")
IO_FUNCS(double, f64, ".4lf", "lf")
