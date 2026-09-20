#include <iostream>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <string>
#include <deque>
#include <vector>
#include <random>
#include <algorithm>
#include <cstdint>
#include "../source/Buffer.hpp"

using namespace server_buffer;

static int g_passed = 0;
static int g_failed = 0;
static int g_printed = 0;
static const int kMaxPrintFail = 50;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (cond) {                                                           \
            ++g_passed;                                                       \
        } else {                                                              \
            ++g_failed;                                                       \
            if (g_printed < kMaxPrintFail) {                                  \
                std::cout << "[FAIL] " << (msg) << "  (" << __FILE__ << ":"   \
                          << __LINE__ << ")\n";                               \
                ++g_printed;                                                  \
            }                                                                 \
        }                                                                     \
    } while (0)

// 用 std::deque<char> 作为 FIFO 参考模型

// 检查环形缓冲区的基本不变量（不做任何读写，不改变状态）
static void CheckInvariants(ServerBuffer &b, const std::deque<char> &model,
                            const std::string &where)
{
    CHECK(b.ReadableSize() == model.size(),
          where + ": ReadableSize=" + std::to_string(b.ReadableSize()) +
              " model=" + std::to_string(model.size()));
    CHECK(b.IdelSize() == b.Size() - 1 - b.ReadableSize(),
          where + ": IdelSize != Size - 1 - ReadableSize");
    CHECK(b.ReadableSize() <= b.Size() - 1,
          where + ": ReadableSize exceeds capacity (Size-1)");
    CHECK(b.Empty() == model.empty(), where + ": Empty() mismatch");

    std::ptrdiff_t rd = b.ReadPosition() - b.Begin();
    std::ptrdiff_t wr = b.WritePosition() - b.Begin();
    CHECK(rd >= 0 && static_cast<size_t>(rd) < b.Size(),
          where + ": _rindex out of range");
    CHECK(wr >= 0 && static_cast<size_t>(wr) < b.Size(),
          where + ": _windex out of range");
}

static bool SameBytes(const std::string &got, const std::deque<char> &model,
                      size_t len)
{
    if (got.size() != len || model.size() < len)
        return false;
    return std::equal(got.begin(), got.end(), model.begin());
}

// 读空当前可读内容并和 model 比对，然后把数据写回，保持缓冲区状态语义不变
static void VerifyFullContent(ServerBuffer &b, const std::deque<char> &model,
                              const std::string &where)
{
    size_t n = b.ReadableSize();
    std::string got = b.ReadAsString(n);
    CHECK(SameBytes(got, model, n), where + ": content mismatch after ReadAsString");
    if (!got.empty())
        b.Write(got.data(), got.size());
}

// 在读取前先依据缓冲区自己报告的可读长度判断，避免直接触发内部断言导致整个
// 测试进程中止；这样可以把“状态损坏”作为一种失败报告出来并继续跑后续用例。
static bool GuardedRead(ServerBuffer &b, size_t len, std::string &out,
                        const std::string &where)
{
    if (len > b.ReadableSize()) {
        CHECK(false, where + ": request " + std::to_string(len) +
                         " > ReadableSize " + std::to_string(b.ReadableSize()));
        return false;
    }
    out = b.ReadAsString(len);
    return true;
}

static std::string RandomData(std::mt19937 &rng, size_t len)
{
    std::string s(len, '\0');
    for (size_t i = 0; i < len; ++i)
        s[i] = static_cast<char>(rng() & 0xFF);
    return s;
}

// ---------------------------------------------------------------------------
// 1. 基础功能（保留原示例的语义并加以校验）
// ---------------------------------------------------------------------------
static void test_basic()
{
    ServerBuffer buffer;
    std::deque<char> model;

    CHECK(buffer.Size() == DEFAULT_SERVER_BUFFER_SIZE, "basic: default size");
    CHECK(buffer.WriteableSize() == DEFAULT_SERVER_BUFFER_SIZE - 1, "basic: initial writeable");
    CHECK(buffer.ReadableSize() == 0, "basic: initial readable");
    CHECK(buffer.Empty(), "basic: initial empty");
    CHECK(!buffer.Full(), "basic: initial not full");

    std::string str("hello world");
    buffer.WriteString(str);
    buffer.WriteString(str);
    model.insert(model.end(), str.begin(), str.end());
    model.insert(model.end(), str.begin(), str.end());

    CHECK(buffer.ReadableSize() == model.size(), "basic: readable after 2 writes");
    CHECK(!buffer.Empty(), "basic: not empty");
    CheckInvariants(buffer, model, "basic");

    std::string ret = buffer.ReadAsString(str.size());
    CHECK(ret == str, "basic: first read content");
    for (size_t i = 0; i < str.size(); ++i)
        model.pop_front();
    VerifyFullContent(buffer, model, "basic");

    std::cout << "[INFO] basic done\n";
}

// ---------------------------------------------------------------------------
// 2. 各种大小的写入/读取（跨越 2KB / 10KB 阈值，触发扩容）
// ---------------------------------------------------------------------------
static void test_sizes()
{
    const size_t sizes[] = {0,     1,      2,      100,      2047,    2048,
                            2049,  1024 * 10 - 1, 1024 * 10, 1024 * 10 + 1,
                            100 * 1024, 1024 * 1024};
    std::mt19937 rng(12345);

    for (size_t sz : sizes) {
        ServerBuffer b;
        std::deque<char> model;
        std::string data = RandomData(rng, sz);

        if (sz)
            b.Write(data.data(), sz);
        model.insert(model.end(), data.begin(), data.end());

        CheckInvariants(b, model, "sizes[" + std::to_string(sz) + "]");
        VerifyFullContent(b, model, "sizes[" + std::to_string(sz) + "]");

        std::string out;
        bool read_ok = true;
        if (sz)
            read_ok = GuardedRead(b, sz, out, "sizes[" + std::to_string(sz) + "]");
        if (read_ok)
            CHECK(out == data, "sizes[" + std::to_string(sz) + "]: read back");
        model.clear();
        CHECK(b.Empty(), "sizes[" + std::to_string(sz) + "]: drained empty");
    }
    std::cout << "[INFO] sizes done\n";
}

// ---------------------------------------------------------------------------
// 3. 扩容压力：只写不读，持续超过阈值
// ---------------------------------------------------------------------------
static void test_resize_stress()
{
    ServerBuffer b;
    std::deque<char> model;
    std::mt19937 rng(999);
    size_t total = 0;

    for (int i = 0; i < 200; ++i) {
        size_t len = 1 + rng() % 4096;
        std::string data = RandomData(rng, len);
        b.Write(data.data(), data.size());
        model.insert(model.end(), data.begin(), data.end());
        total += len;
        CheckInvariants(b, model, "resize_stress#" + std::to_string(i));
    }
    CHECK(total > THRESHOLD_SERVER_BUFFER_SIZE, "resize_stress: total exceeds threshold");
    CHECK(b.Size() > DEFAULT_SERVER_BUFFER_SIZE, "resize_stress: buffer grew");
    VerifyFullContent(b, model, "resize_stress");

    // 分块读净并逐块比对
    size_t remaining = model.size();
    while (remaining) {
        if (b.ReadableSize() != model.size()) {
            CHECK(false, "resize_stress: state corrupted before drain");
            break;
        }
        size_t len = std::min<size_t>(remaining, 1 + rng() % 5000);
        std::string got;
        if (!GuardedRead(b, len, got, "resize_stress"))
            break;
        CHECK(SameBytes(got, model, len), "resize_stress: chunk read mismatch");
        model.erase(model.begin(), model.begin() + len);
        remaining -= len;
    }
    CHECK(b.Empty(), "resize_stress: empty after drain");
    std::cout << "[INFO] resize_stress done (grew to " << b.Size() << " bytes)\n";
}

// ---------------------------------------------------------------------------
// 4. 回绕压力：制造 _rindex > _windex 的回绕读写分支
// ---------------------------------------------------------------------------
static void test_wraparound()
{
    std::mt19937 rng(2024);
    ServerBuffer b;
    std::deque<char> model;

    // 让 windex 恰好写到末尾回绕到 0，从而出现 rindex > windex
    std::string a = RandomData(rng, 1000);
    b.Write(a.data(), a.size());
    model.insert(model.end(), a.begin(), a.end());

    std::string first;
    if (GuardedRead(b, 500, first, "wraparound"))
        CHECK(SameBytes(first, model, 500), "wraparound: first read");
    model.erase(model.begin(), model.begin() + 500);

    size_t to_end = b.Size() - (b.WritePosition() - b.Begin());
    std::string fill = RandomData(rng, to_end);
    b.Write(fill.data(), fill.size());
    model.insert(model.end(), fill.begin(), fill.end());

    std::ptrdiff_t rd = b.ReadPosition() - b.Begin();
    std::ptrdiff_t wr = b.WritePosition() - b.Begin();
    CHECK(rd > wr, "wraparound: expected wrapped state (rindex > windex)");
    CheckInvariants(b, model, "wraparound");

    // 再写一段，保证可读跨过尾部边界，命中 Read 的回绕分支
    std::string more = RandomData(rng, 400);
    b.Write(more.data(), more.size());
    model.insert(model.end(), more.begin(), more.end());
    CheckInvariants(b, model, "wraparound2");

    while (!model.empty()) {
        if (b.ReadableSize() != model.size()) {
            CHECK(false, "wraparound: state corrupted before drain");
            break;
        }
        size_t len = 1 + rng() % std::min<size_t>(model.size(), 600);
        std::string got;
        if (!GuardedRead(b, len, got, "wraparound"))
            break;
        CHECK(SameBytes(got, model, len), "wraparound: wrapped read mismatch");
        model.erase(model.begin(), model.begin() + len);
    }
    CHECK(b.Empty(), "wraparound: drained");
    std::cout << "[INFO] wraparound done\n";
}

// ---------------------------------------------------------------------------
// 5. 边界：正好写满可写空间（潜在“满/空”歧义）
// ---------------------------------------------------------------------------
static void test_exact_fill()
{
    ServerBuffer b;
    std::mt19937 rng(7);
    // 可用容量为 Size()-1，写满即可写空间
    std::string data = RandomData(rng, b.WriteableSize());

    b.Write(data.data(), data.size());

    // 期望：写满容量后 ReadableSize()==Size()-1、非空且 Full()
    CHECK(b.ReadableSize() == b.Size() - 1,
          "exact_fill: ReadableSize should equal Size-1 after filling capacity");
    CHECK(!b.Empty(), "exact_fill: should not be Empty after filling capacity");
    CHECK(b.Full(), "exact_fill: should be Full after filling capacity");

    std::string got = b.ReadAsString(b.ReadableSize());
    CHECK(got == data, "exact_fill: filled content should be readable");

    // 重新写回以恢复满状态，再验证满状态下追加写入会扩容且不丢数据
    b.Write(got.data(), got.size());
    CHECK(b.Full(), "exact_fill: Full after rewrite");

    size_t old_size = b.Size();
    std::string extra = RandomData(rng, 100);
    b.Write(extra.data(), extra.size());
    CHECK(b.Size() > old_size, "exact_fill: buffer should grow when full");
    CHECK(b.ReadableSize() == (old_size - 1) + extra.size(),
          "exact_fill: no data loss after growing from full");
    std::string all = b.ReadAsString(b.ReadableSize());
    std::string expected = data + extra;
    CHECK(all == expected, "exact_fill: content preserved after growing from full");
    std::cout << "[INFO] exact_fill done\n";
}

// ---------------------------------------------------------------------------
// 6. GetLine 行为
// ---------------------------------------------------------------------------
static void test_getline()
{
    ServerBuffer b;
    std::string payload = "aaa\nbbb\nccc";
    b.WriteString(payload);
    CHECK(b.GetLine() == "aaa\n", "getline: first line");
    CHECK(b.GetLine() == "bbb\n", "getline: second line");
    CHECK(b.GetLine() == "", "getline: no CRLF -> empty");
    CHECK(b.ReadableSize() == 3, "getline: trailing bytes preserved");
    CHECK(b.ReadAsString(3) == "ccc", "getline: trailing content");

    // 回绕：换行只落在前缀区（Begin 区），应返回正确的第一行
    ServerBuffer w;
    std::string logical;
    std::string a(1000, 'A');
    w.Write(a.data(), a.size());
    logical += a;
    w.ReadAsString(900);            // rindex=900
    logical.erase(0, 900);
    size_t to_end = w.Size() - (w.WritePosition() - w.Begin());
    std::string fill(to_end, 'B');  // windex 回绕到 0
    w.Write(fill.data(), fill.size());
    logical += fill;
    std::string prefix = "line1\nrestdata";  // 换行仅出现在前缀区
    w.WriteString(prefix);
    logical += prefix;

    CHECK((w.WritePosition() - w.Begin()) < (w.ReadPosition() - w.Begin()),
          "getline: expected wrapped state");
    size_t nl = logical.find('\n');
    std::string expected_line = logical.substr(0, nl + 1);
    CHECK(w.GetLine() == expected_line, "getline: wrapped first line");
    CHECK(w.ReadableSize() == logical.size() - (nl + 1),
          "getline: wrapped remainder size");
    std::string rest = w.ReadAsString(w.ReadableSize());
    CHECK(rest == logical.substr(nl + 1), "getline: wrapped remainder content");

    // 回绕且无换行：返回空串且不消耗数据
    ServerBuffer w2;
    std::string c(1500, 'C');
    w2.Write(c.data(), c.size());
    w2.ReadAsString(1000);          // rindex=1000, windex=1500
    size_t te = w2.Size() - (w2.WritePosition() - w2.Begin());
    std::string f2(te, 'D');        // windex 回绕到 0
    w2.Write(f2.data(), f2.size());
    w2.WriteString("tail");         // 前缀区，无换行
    size_t before = w2.ReadableSize();
    CHECK(w2.GetLine() == "", "getline: wrapped no CRLF -> empty");
    CHECK(w2.ReadableSize() == before, "getline: no CRLF not consumed");
    std::cout << "[INFO] getline done\n";
}

// ---------------------------------------------------------------------------
// 7. WriteServerBuffer 搬运
// ---------------------------------------------------------------------------
static void test_write_server_buffer()
{
    std::mt19937 rng(88);

    // 1) 异体 · 非回绕源
    {
        ServerBuffer src, dst;
        std::string data = RandomData(rng, 3000);
        src.Write(data.data(), data.size());
        dst.WriteServerBuffer(src);
        CHECK(dst.ReadableSize() == data.size(), "wsb nonwrap: transferred size");
        std::string transferred;
        if (GuardedRead(dst, data.size(), transferred, "wsb nonwrap"))
            CHECK(transferred == data, "wsb nonwrap: transferred content");
        CHECK(src.ReadableSize() == data.size(), "wsb nonwrap: source unchanged");
    }

    // 2) 异体 · 回绕源
    {
        ServerBuffer src, dst;
        std::string a(1000, 'A');
        src.Write(a.data(), a.size());
        src.ReadAsString(900);                       // rindex=900
        size_t to_end = src.Size() - (src.WritePosition() - src.Begin());
        std::string fill(to_end, 'B');               // windex 回绕到 0
        src.Write(fill.data(), fill.size());
        std::string logical;
        logical += std::string(100, 'A');            // 尾部剩余
        logical += fill;
        CHECK((src.WritePosition() - src.Begin()) < (src.ReadPosition() - src.Begin()),
              "wsb wrap: expected wrapped source");
        dst.WriteServerBuffer(src);
        CHECK(dst.ReadableSize() == logical.size(), "wsb wrap: transferred size");
        std::string transferred;
        if (GuardedRead(dst, logical.size(), transferred, "wsb wrap"))
            CHECK(transferred == logical, "wsb wrap: transferred content");
        CHECK(src.ReadableSize() == logical.size(), "wsb wrap: source unchanged");
    }

    // 3) 自体 · 非回绕（内容翻倍）
    {
        ServerBuffer b;
        std::string d = RandomData(rng, 1500);
        b.Write(d.data(), d.size());
        b.WriteServerBuffer(b);
        CHECK(b.ReadableSize() == 2 * d.size(), "wsb self: size doubled");
        std::string got = b.ReadAsString(b.ReadableSize());
        CHECK(got == d + d, "wsb self: content doubled");
    }

    // 4) 自体 · 回绕（按逻辑顺序翻倍，顺带触发扩容）
    {
        ServerBuffer b;
        std::string a(1000, 'A');
        b.Write(a.data(), a.size());
        b.ReadAsString(900);
        size_t to_end = b.Size() - (b.WritePosition() - b.Begin());
        std::string fill(to_end, 'B');
        b.Write(fill.data(), fill.size());           // windex 回绕到 0
        std::string logical;
        logical += std::string(100, 'A');
        logical += fill;
        CHECK((b.WritePosition() - b.Begin()) < (b.ReadPosition() - b.Begin()),
              "wsb self wrap: expected wrapped state");
        b.WriteServerBuffer(b);
        CHECK(b.ReadableSize() == 2 * logical.size(), "wsb self wrap: size doubled");
        std::string got = b.ReadAsString(b.ReadableSize());
        CHECK(got == logical + logical, "wsb self wrap: content doubled");
    }

    std::cout << "[INFO] write_server_buffer done\n";
}

// ---------------------------------------------------------------------------
// 8. 随机模糊压力测试：写入/读取与参考模型逐字节比对
// ---------------------------------------------------------------------------
static void test_fuzz(int iters, uint32_t seed)
{
    ServerBuffer b;
    std::deque<char> model;
    std::mt19937 rng(seed);
    const size_t kSoftLimit = 256 * 1024;

    for (int i = 0; i < iters; ++i) {
        // 每轮开始先确认缓冲区状态与模型一致，避免状态损坏后继续读取触发断言
        if (b.ReadableSize() != model.size()) {
            CHECK(false, "fuzz[" + std::to_string(seed) + "] iter " +
                             std::to_string(i) + ": state desync ReadableSize=" +
                             std::to_string(b.ReadableSize()) + " model=" +
                             std::to_string(model.size()));
            b.Reset();
            model.clear();
            continue;
        }

        bool doRead = model.empty() || (rng() % 100 < 45) || model.size() > kSoftLimit;

        if (!doRead) {
            size_t len;
            uint32_t r = rng() % 100;
            if (r < 90)
                len = rng() % (8 * 1024 + 1);
            else
                len = rng() % (64 * 1024 + 1);
            if (len == 0)
                continue;
            std::string data = RandomData(rng, len);
            b.Write(data.data(), data.size());
            model.insert(model.end(), data.begin(), data.end());
        } else {
            size_t len = rng() % (model.size() + 1);
            std::string got = b.ReadAsString(len);
            if (!SameBytes(got, model, len)) {
                CHECK(false, "fuzz[" + std::to_string(seed) + "] iter " +
                                 std::to_string(i) + ": read content mismatch");
                b.Reset();
                model.clear();
                continue;
            }
            model.erase(model.begin(), model.begin() + len);
        }

        if (b.ReadableSize() != model.size()) {
            CHECK(false, "fuzz[" + std::to_string(seed) + "] iter " +
                             std::to_string(i) + ": ReadableSize=" +
                             std::to_string(b.ReadableSize()) + " model=" +
                             std::to_string(model.size()));
            b.Reset();
            model.clear();
            continue;
        }
        CheckInvariants(b, model, "fuzz[" + std::to_string(seed) + "] iter " +
                                      std::to_string(i));
    }

    std::cout << "[INFO] fuzz seed=" << seed << " iters=" << iters
              << " final_size=" << b.Size() << " remaining=" << model.size()
              << " done\n";
}

int main()
{
    std::cout << "===== ServerBuffer stress test start =====\n";

    test_basic();
    test_sizes();
    test_resize_stress();
    test_wraparound();
    test_exact_fill();
    test_getline();
    test_write_server_buffer();

    test_fuzz(100000, 1);
    test_fuzz(100000, 42);
    test_fuzz(100000, 20240920);

    std::cout << "===== ServerBuffer stress test end =====\n";
    std::cout << "passed: " << g_passed << ", failed: " << g_failed << "\n";
    if (g_failed > kMaxPrintFail)
        std::cout << "(only first " << kMaxPrintFail << " failures printed)\n";

    return g_failed == 0 ? 0 : 1;
}
