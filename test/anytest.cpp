#include "../source/Connection.hpp"
#include <atomic>
#include <string>
#include <iostream>
#include <cstdlib>

using namespace server_connection;

static std::atomic<int> g_passed{0};
static std::atomic<int> g_failed{0};

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            g_passed.fetch_add(1);                                             \
        } else {                                                               \
            g_failed.fetch_add(1);                                             \
            std::cout << "[FAIL] " << (msg) << "  (" << __FILE__ << ":"        \
                      << __LINE__ << ")\n";                                    \
        }                                                                      \
    } while (0)

struct Tracker
{
    static std::atomic<int> alive;
    int v;
    explicit Tracker(int x = 0) : v(x) { alive.fetch_add(1); }
    Tracker(const Tracker& o) : v(o.v) { alive.fetch_add(1); }
    ~Tracker() { alive.fetch_sub(1); }
};
std::atomic<int> Tracker::alive{0};

int main()
{
    std::cout << "===== Any test start =====\n";

    // 模板构造 + get<T>
    Any a(42);
    CHECK(a.get<int>() != nullptr && *a.get<int>() == 42,
          "Any template ctor + get<int>");

    // 拷贝构造
    Any b(a);
    CHECK(*b.get<int>() == 42, "Any copy ctor");
    CHECK(b.get<int>() != a.get<int>(), "Any copy is deep");

    // operator= + get<std::string>
    Any c;
    c = std::string("hello");
    CHECK(*c.get<std::string>() == "hello", "Any operator= + get<string>");

    // get<double>
    Any d;
    d = 3.5;
    CHECK(*d.get<double>() == 3.5, "Any get<double>");

    // swap
    Any x(1), y(2);
    x.swap(y);
    CHECK(*x.get<int>() == 2 && *y.get<int>() == 1, "Any::swap");

    // 空 Any 的拷贝不应崩溃
    Any e1;
    Any e2(e1);
    CHECK(true, "Any default ctor + empty copy no crash");

    // 赋值返回自身引用，可链式
    Any z(0);
    z = 7;
    CHECK(*z.get<int>() == 7, "Any operator= returns usable reference");

    // 析构释放内部对象
    {
        Tracker t(7);
        Any at(t);
        CHECK(Tracker::alive.load() == 2, "Any stores a copy of value");
    }
    CHECK(Tracker::alive.load() == 0, "Any releases stored object on dtor");

    std::cout << "===== Any test end =====\n";
    std::cout << "passed: " << g_passed.load()
              << ", failed: " << g_failed.load() << "\n";
    return g_failed.load() == 0 ? 0 : 1;
}
