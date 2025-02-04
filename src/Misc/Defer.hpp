#include <utility>

// https://stackoverflow.com/a/42060129/163827
#ifndef defer
struct DeferDummy {};
template <class F>
struct Deferrer {
    F f;
    ~Deferrer() { f(); }
};
template <class F>
auto operator*(DeferDummy, F f) -> Deferrer<F> {
    return {f};
}
#define DEFER_(LINE) zz_defer##LINE
#define DEFER(LINE) DEFER_(LINE)
// NOLINTNEXTLINE(bugprone-macro-parentheses)
#define defer auto DEFER(__LINE__) = DeferDummy{} *[&]()
#endif // defer
