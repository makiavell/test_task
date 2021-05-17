#pragma once
#include "task_medialooks.h"

#include <string_view>
#include <ctime>
#include <iomanip>
#include <string>
#include <type_traits>
#include <functional>

namespace medialooks
{

// TEST CASES:
// 1.1 add client
// 1.2 add multiple clients
// 2.1 remove client oldest/newest client/remove client in the middle of the list
// 2.2 remove running client
// 3.1 get client by id
// 3.2 get client by wrong id
// 3.3 get client by index, by wrong index
// 3.4 get client by wrong index
// 4.1 putClient data with timeout -> ok
// 4.2  -> missed
// 4.3  -> interrupted
// 4.4 putClient data with -1 timeout
// 5.1 get data -> ok
// 5.2  -> interrupted
// 5.3  -> timeout
// 5.4  -> not found
// 5.5 get data -1 timeout
 

// the main test function
void test();

// test cases:
// 4.1 putClient data with timeout -> ok
// 4.2  -> missed
//// 5.1 get data -> ok
void test0();

// test cases:
// 1.1 add client
// 1.2 add multiple clients
// 2.1 remove client oldest/newest client/remove client in the middle of the lis
void test1();

// test cases:
// 2.2 remove running client
// 5.4  -> not found
void test2();

// test cases:
// 3.1 get client by id
// 3.2 get client by wrong id
// 3.3 get client by index, by wrong index
// 3.4 get client by wrong index
void test3();

// test cases:
// 4.3  -> interrupted
// 4.4 putClient data with -1 timeout
void test4();

// 5.3  -> timeout
// 5.5 get data -1 timeout
// 5.1 get data -> ok
// 5.2  -> interrupted
// 5.3  -> timeout
void test5();

// multiple clients
void test6();

// testing
struct Logger
{
    std::string_view errToStr(ESplitterError e)
    {
        switch(e)
        {
        case ESplitterError::eOk: return "Ok";
        case ESplitterError::eTimeout: return "eTimeout";
        case ESplitterError::eMissBuffer: return "eMissBuffer";
        case ESplitterError::eClientNotFound: return "eClientNotFound";
        case ESplitterError::eInterrupted: return "eInterrupted";
        case ESplitterError::eGeneral: return "eGeneralError";
        default:
            return "unknown error!";
        }
    }

    void start()
    {
        std::cout.precision(3);
        tpStartTest = std::chrono::steady_clock::now();
        aLogMsg.clear();
        aLogMsg.emplace_back("\n\n\n\nStart\n");
    }

    void print(std::string_view threadName, std::string_view function, ESplitterError e, std::chrono::steady_clock::time_point start, std::string_view message)
    {
        //std::chrono::steady_clock::now();

        //const ESplitterError code(static_cast<ESplitterError>(e));
        const auto microsec = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - tpStartTest).count();
        const double millisec = 0.001 * microsec;
        // TODO: format time
        //std::cout << "time: " << microsec << " thread: " << threadName << " code: " << errToStr(code) << std::endl;

        std::stringstream ss;
        ss << "time: " << millisec
            << " thread: " << threadName
            << " function: " << function 
            << " code: " << errToStr(e)
            << " started: " << 0.001 * std::chrono::duration_cast<std::chrono::microseconds>(start - tpStartTest).count()
            << " " << message << std::endl;
        std::string msg = ss.str();

        std::lock_guard gb(m_mutLog);
        aLogMsg.emplace_back(std::move(msg));
        // aLogMsg.emplace_back() = std::string{} + "time: " + microsec + " thread: " + threadName + " code: " + errToStr(code);
    }

    void dumpConsole()
    {
        for(auto& m : aLogMsg)
            std::cout << m;
    }

    std::mutex m_mutLog;
    std::vector<std::string> aLogMsg;
    std::chrono::steady_clock::time_point tpStartTest;
};

Logger log;

struct StepBase
{
    virtual void run(ISplitter* s, std::string_view threadName) = 0;
    virtual ~StepBase() = default;
};

namespace aux2
{
template<typename T> struct is_shared_ptr : std::false_type {};
template<typename T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

template<typename T> struct is_vector : std::false_type {};
template<typename T> struct is_vector<std::vector<T>> : std::true_type {};

template<typename T> struct is_ref_wrapper : std::false_type {};
template<typename T> struct is_ref_wrapper<std::reference_wrapper<T>> : std::true_type {};

template<typename T>
std::ostream& print_type(std::ostream& out, T&& val)
{
    using raw_type = std::decay_t<T>;
    constexpr bool isPointer = std::is_pointer<raw_type>::value;
    constexpr bool isShared = is_shared_ptr<raw_type>::value;
    constexpr bool isVector = is_vector<raw_type>::value;
    constexpr bool isRefWrapper = is_ref_wrapper<raw_type>::value;
    constexpr bool bElse = !isPointer && !isShared && !isVector && !isRefWrapper;

    if constexpr(isPointer) print_type((out << "PointerVal "), *val);
    if constexpr(isShared) print_type((out << "SharedPtr "), *val.get());
    if constexpr(isVector)
    {
        out << '[';
        for(size_t ind = 0; ind < val.size(); ++ind)
        {
            print_type(out, int(val[ind]));
            if(ind != val.size() - 1)
                out << ", ";
        }
        out << ']';
    }
    if constexpr(isRefWrapper) print_type(out, val.get());
    if constexpr(bElse) out << val;
    return out;
}

template<std::size_t> struct int_ {};
template <class Tuple, size_t Pos>
std::ostream& print_tuple(std::ostream& out, const Tuple& t, int_<Pos>) 
{
    print_type(out, std::get<std::tuple_size<Tuple>::value-Pos>(t)) << ',';
    return print_tuple(out, t, int_<Pos-1>());
}
template <class Tuple>
std::ostream& print_tuple(std::ostream& out, const Tuple& t, int_<1>) 
{
    return print_type(out, std::get<std::tuple_size<Tuple>::value-1>(t));
}
template <class Tuple>
std::ostream& print_tuple(std::ostream& out, const Tuple& t, int_<0>)
{
    return out;
}
}// aux2
template<class Ch, class Tr, class... Args>
auto operator<<(std::basic_ostream<Ch, Tr>& os, std::tuple<Args...> const& t)
-> std::basic_ostream<Ch, Tr>&
{
    os << '(';
    aux2::print_tuple(os, t, aux2::int_<sizeof...(Args)>());
    return os << ')';
}

template<typename Foo, typename... Args>
struct Step : public StepBase
{
    Step(Foo f, std::string_view fn, int32_t d, Args... args) : m_foo(f), m_aArg(args...), m_function(fn), m_delay(d) {}
    virtual ~Step() = default;

    Foo m_foo;
    std::string_view m_function;
    int32_t m_delay;
    std::tuple<Args...> m_aArg;

    template<typename TObj>
    decltype(auto) runImp(TObj* obj)
    {
        return std::apply(m_foo, std::tuple_cat(std::make_tuple(obj), m_aArg));
    }

    void run(ISplitter* s, std::string_view threadName) override
    {
        auto timeStart = std::chrono::steady_clock::now();

        auto res = runImp(s);

        std::stringstream ss;
        ss << m_aArg;
        //string str = ss.str();
        auto str = ss.str();

        if constexpr(std::is_same<bool, decltype(res)>::value)
        {
            log.print(threadName, m_function, res ? ESplitterError::eOk : ESplitterError::eGeneral, timeStart, str);
        }
        if constexpr(std::is_same<int32_t, decltype(res)>::value)
        {
            log.print(threadName, m_function, static_cast<ESplitterError>(res), timeStart, str);
        }

        const auto duration = std::chrono::steady_clock::now() - timeStart;
        auto sleep = (duration > std::chrono::milliseconds(m_delay)) ? std::chrono::milliseconds(0) : (std::chrono::milliseconds(m_delay) - duration);
        if(sleep.count() > 0)
            std::this_thread::sleep_for(sleep);
    }
};

struct StepRepeat : public StepBase
{
    StepRepeat(StepBase* s, int32_t d, int32_t count) : m_step(s), m_delay(d), m_count(count) {}
    StepRepeat(StepBase* s, int32_t d, std::function<bool()> condition) : m_step(s), m_delay(d), m_condition(condition) {}
    virtual ~StepRepeat() = default;

    StepBase* m_step{nullptr};
    int32_t m_count{0};
    std::function<bool()> m_condition{nullptr};
    int32_t m_delay{0};

    void run(ISplitter* s, std::string_view threadName) override
    {
        const auto timeStart = std::chrono::steady_clock::now();
        if(m_condition)
        {
            while(m_condition())
                m_step->run(s, threadName);
        }
        else if(m_count > 0) // number of iteration
        {
            for(int32_t i = 0; i < m_count; ++i)
                m_step->run(s, threadName);
        }
        else
        {
            const auto millisec = std::chrono::milliseconds(-m_count);
            auto tp = std::chrono::steady_clock::now();
            while((tp - timeStart) < millisec)
            {
                m_step->run(s, threadName);
                tp = std::chrono::steady_clock::now();
            }
        }

        if(m_delay > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(m_delay));
    }
};

void runTask(const std::vector<StepBase*>& v, ISplitter* s, std::string_view threadName)
{
    for(const auto& st : v)
        st->run(s, threadName);
}

using BufferData = std::vector<uint8_t>;
void test0()
{
    // test cases:
    // 4.1 putClient data with timeout -> ok
    // 4.2  -> missed
    // 5.1 get data -> ok
    std::shared_ptr<ISplitter> s = SplitterCreate(2, 2);

    uint32_t id{0};
    s->SplitterClientAdd(&id);

    const int32_t pause = 100;
    const int32_t maxWait = 50;

    std::vector<StepBase*> aStepsProducer{
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{1}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{2}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{3}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{4}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{5}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{6}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{7}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{8}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{9}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{10}), maxWait},
    };

    auto clientBuffer = std::make_shared<BufferData>();

    auto stepGet = new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id, std::ref(clientBuffer), maxWait};
    std::vector<StepBase*> aStepsConsumer{
    new StepRepeat{stepGet, 500, [&clientBuffer]() {return clientBuffer->empty() || clientBuffer->back() != 3; }},
    new StepRepeat{stepGet, 0, -500}
    };

    log.start();

    std::thread tp(runTask, std::ref(aStepsProducer), s.get(), "producer");
    std::thread tc1(runTask, std::ref(aStepsConsumer), s.get(), "consumer");
    tp.join();
    tc1.join();

    log.dumpConsole();
    aux2::print_type((std::cout << "buffer "), clientBuffer) << std::endl;
}

void test1()
{
    // test cases:
    // 1.1 add client
    // 1.2 add multiple clients
    // 2.1 remove client oldest/newest client/remove client in the middle of the list
    uint32_t id1{0};
    uint32_t id2{0};
    uint32_t id3{0};
    uint32_t id4{0};
    uint32_t id5{0};
    std::vector<StepBase*> aSteps{
        new Step{&ISplitter::SplitterClientAdd, "SplitterClientAdd", 0, &id1},
        new Step{&ISplitter::SplitterClientAdd, "SplitterClientAdd", 0, &id2},
        new Step{&ISplitter::SplitterClientAdd, "SplitterClientAdd", 0, &id3},
        new Step{&ISplitter::SplitterClientAdd, "SplitterClientAdd", 0, &id4}, // add - error
        new Step{&ISplitter::SplitterClientRemove, "SplitterClientRemove", 0, 0}, // remove - error
        new Step{&ISplitter::SplitterClientRemove, "SplitterClientRemove", 0, 100}, // remove - error
        new Step{&ISplitter::SplitterClientRemove, "SplitterClientRemove", 0, std::ref(id2)}, // remove middle
        new Step{&ISplitter::SplitterClientRemove, "SplitterClientRemove", 0, std::ref(id3)}, // remove newest
        new Step{&ISplitter::SplitterClientAdd, "SplitterClientAdd", 0, &id5},
        new Step{&ISplitter::SplitterClientRemove, "SplitterClientRemove", 0, std::ref(id1)}, // remove oldest
        new Step{&ISplitter::SplitterClientRemove, "SplitterClientRemove", 0, std::ref(id5)}, // remove the last client
    };

    std::shared_ptr<ISplitter> s = SplitterCreate(3, 3);

    log.start();
    runTask(aSteps, s.get(), "main");

    log.dumpConsole();
}

void test2()
{
    // test cases:
    // 2.2 remove running client
    // 5.4  -> not found

    std::shared_ptr<ISplitter> s = SplitterCreate(2, 2);

    uint32_t id1{0};
    uint32_t id2{0};
    s->SplitterClientAdd(&id1);
    s->SplitterClientAdd(&id2);

    const int32_t pause = 100;
    const int32_t maxWait = 50;

    std::vector<StepBase*> aStepsProducer{
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{1}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{2}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{3}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{4}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{5}), maxWait},
    new Step{&ISplitter::SplitterClientRemove, "SplitterClientRemove", 0, std::ref(id2)},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{6}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{7}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{8}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{9}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{10}), maxWait},
    };

    auto clientBuffer1 = std::make_shared<BufferData>();
    auto stepGet1 = new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), maxWait};

    std::vector<StepBase*> aStepsConsumer1{
    new StepRepeat{stepGet1, 500, [&clientBuffer1]() {return clientBuffer1->empty() || clientBuffer1->back() != 3; }},
    new StepRepeat{stepGet1, 0, -1000}
    };

    auto clientBuffer2 = std::make_shared<BufferData>();
    auto stepGet2 = new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id2, std::ref(clientBuffer2), maxWait};
    std::vector<StepBase*> aStepsConsumer2{
    new StepRepeat{stepGet2, 0, -600}
    };

    log.start();

    std::thread tp(runTask, std::ref(aStepsProducer), s.get(), "producer");
    std::thread tc1(runTask, std::ref(aStepsConsumer1), s.get(), "consumer1");
    std::thread tc2(runTask, std::ref(aStepsConsumer2), s.get(), "consumer2");
    tp.join();
    tc1.join();
    tc2.join();

    log.dumpConsole();
    aux2::print_type((std::cout << "buffer1 "), clientBuffer1) << std::endl;
    aux2::print_type((std::cout << "buffer2 "), clientBuffer2) << std::endl;
}

void test3()
{
    // test cases:
    // 3.1 get client by id
    // 3.2 get client by wrong id
    // 3.3 get client by index, by wrong index
    // 3.4 get client by wrong index
    uint32_t id1{0};
    uint32_t id2{0};
    uint32_t id3{0};
    uint32_t id4{0};
    uint32_t id5{0};

    const int32_t maxWait = 50;

    size_t count{0};
    uint32_t clientId{0};
    size_t latency{0};
    size_t dropped{0};

    auto clientBuffer1 = std::make_shared<BufferData>();
    auto stepGet1 = new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), maxWait};

    auto clientBuffer2 = std::make_shared<BufferData>();
    auto stepGet2 = new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id2, std::ref(clientBuffer2), maxWait};
    
    std::vector<StepBase*> aSteps{
        new Step{&ISplitter::SplitterClientAdd, "SplitterClientAdd", 0, &id1},
        new Step{&ISplitter::SplitterClientAdd, "SplitterClientAdd", 0, &id2},
        new Step{&ISplitter::SplitterClientAdd, "SplitterClientAdd", 0, &id3},
        new Step{&ISplitter::SplitterClientAdd, "SplitterClientAdd", 0, &id4}, 
        new Step{&ISplitter::SplitterClientAdd, "SplitterClientAdd", 0, &id5},
        new Step{&ISplitter::SplitterPut, "SplitterPut", 0, std::make_shared<BufferData>(BufferData{1}), maxWait},
        stepGet1,
        stepGet2,
        new Step{&ISplitter::SplitterPut, "SplitterPut", 0, std::make_shared<BufferData>(BufferData{2}), maxWait},
        stepGet1,
        stepGet2,
        new Step{&ISplitter::SplitterPut, "SplitterPut", 0, std::make_shared<BufferData>(BufferData{3}), maxWait},
        stepGet1,
        new Step{&ISplitter::SplitterPut, "SplitterPut", 0, std::make_shared<BufferData>(BufferData{4}), maxWait},
        stepGet1,
        new Step{&ISplitter::SplitterClientGetCount, "SplitterClientGetCount", 0, &count},
        new Step{&ISplitter::SplitterClientGetByIndex, "SplitterClientGetByIndex", 0, 0, &clientId, &latency, &dropped},
        new Step{&ISplitter::SplitterClientGetByIndex, "SplitterClientGetByIndex", 0, 10, &clientId, &latency, &dropped},
        new Step{&ISplitter::SplitterClientGetById, "SplitterClientGetById", 0, std::ref(id1), &latency, &dropped},
        new Step{&ISplitter::SplitterClientGetById, "SplitterClientGetById", 0, std::ref(id2), &latency, &dropped},
        new Step{&ISplitter::SplitterClientGetById, "SplitterClientGetById", 0, 999, &latency, &dropped},
    };

    std::shared_ptr<ISplitter> s = SplitterCreate(2, 5);

    log.start();
    runTask(aSteps, s.get(), "main");
    log.dumpConsole();
}

void test4()
{
    // test cases:
    // 4.3  -> interrupted
    // 4.4 putClient data with -1 timeout

    std::shared_ptr<ISplitter> s = SplitterCreate(2, 2);

    uint32_t id1{0};
    s->SplitterClientAdd(&id1);

    const int32_t pause = 100;
    const int32_t maxWait = 40;
    const int32_t timeoutBig = -1;

    std::vector<StepBase*> aStepsProducer{
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{1}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{2}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{3}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{4}), timeoutBig},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{5}), timeoutBig},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{6}), timeoutBig},
    };

    auto clientBuffer1 = std::make_shared<BufferData>();
    std::vector<StepBase*> aStepsConsumer1{
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), timeoutBig},
    new Step{&ISplitter::SplitterGet, "SplitterGet", 1000, id1, std::ref(clientBuffer1), timeoutBig},
    new Step{&ISplitter::SplitterFlush, "SplitterFlush", 0}, // interrupt producer
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), timeoutBig},
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), 50}
    };

    log.start();

    std::thread tp(runTask, std::ref(aStepsProducer), s.get(), "producer");
    std::thread tc1(runTask, std::ref(aStepsConsumer1), s.get(), "consumer1");
    tp.join();
    tc1.join();

    log.dumpConsole();
    aux2::print_type((std::cout << "buffer1 "), clientBuffer1) << std::endl;
}

void test5()
{
    // test cases:
    // 5.3  -> timeout
    // 5.5 get data -1 timeout
    // 5.1 get data -> ok
    // 5.2  -> interrupted
    // 5.3  -> timeout

    std::shared_ptr<ISplitter> s = SplitterCreate(2, 2);

    uint32_t id1{0};
    s->SplitterClientAdd(&id1);

    const int32_t pause = 100;
    const int32_t maxWait = 40;
    const int32_t timeoutBig = 100;

    std::vector<StepBase*> aStepsProducer{
    new Step{&ISplitter::SplitterPut, "SplitterPut", 500, std::make_shared<BufferData>(BufferData{1}), maxWait},
    new Step{&ISplitter::SplitterFlush, "SplitterFlush", 0}, // interrupt consumer

    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{2}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{3}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{4}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{5}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{6}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{7}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{8}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{9}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{10}), maxWait},
    };

    auto clientBuffer1 = std::make_shared<BufferData>();
    std::vector<StepBase*> aStepsConsumer1{
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), timeoutBig},
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), timeoutBig},
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), timeoutBig},
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), timeoutBig},
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), -1},
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), timeoutBig},
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), timeoutBig},
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), timeoutBig},
    new Step{&ISplitter::SplitterGet, "SplitterGet", 0, id1, std::ref(clientBuffer1), timeoutBig},
    };

    log.start();

    std::thread tp(runTask, std::ref(aStepsProducer), s.get(), "producer");
    std::thread tc1(runTask, std::ref(aStepsConsumer1), s.get(), "consumer1");
    tp.join();
    tc1.join();

    log.dumpConsole();
    aux2::print_type((std::cout << "buffer1 "), clientBuffer1) << std::endl;
}

void test6()
{
    std::shared_ptr<ISplitter> s = SplitterCreate(3, 5);

    uint32_t id1{0};
    uint32_t id2{0};
    uint32_t id3{0};
    uint32_t id4{0};
    uint32_t id5{0};
    s->SplitterClientAdd(&id1);
    s->SplitterClientAdd(&id2);
    s->SplitterClientAdd(&id3);
    s->SplitterClientAdd(&id4);
    s->SplitterClientAdd(&id5);
    auto clientBuffer1 = std::make_shared<BufferData>();
    auto clientBuffer2 = std::make_shared<BufferData>();
    auto clientBuffer3 = std::make_shared<BufferData>();
    auto clientBuffer4 = std::make_shared<BufferData>();
    auto clientBuffer5 = std::make_shared<BufferData>();

    const int32_t pause = 100;
    const int32_t maxWait = 40;
    const int32_t totalDuration = 2000;
    const int32_t timeoutBig = -1;
    const int32_t delayBase = 100;

    std::vector<StepBase*> aStepsProducer{
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{1}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{2}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{3}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{4}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{5}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{6}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{7}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{8}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{9}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{10}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{11}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{12}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{13}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{14}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{15}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{16}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{17}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{18}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{19}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{20}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{21}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{22}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{23}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{24}), maxWait},
    new Step{&ISplitter::SplitterPut, "SplitterPut", pause, std::make_shared<BufferData>(BufferData{25}), maxWait},
    };

    auto stepGet1 = new Step{&ISplitter::SplitterGet, "SplitterGet", delayBase*0, id1, std::ref(clientBuffer1), timeoutBig};
    auto stepGet2 = new Step{&ISplitter::SplitterGet, "SplitterGet", delayBase*1, id2, std::ref(clientBuffer2), timeoutBig};
    auto stepGet3 = new Step{&ISplitter::SplitterGet, "SplitterGet", delayBase*2, id3, std::ref(clientBuffer3), timeoutBig};
    auto stepGet4 = new Step{&ISplitter::SplitterGet, "SplitterGet", delayBase*3, id4, std::ref(clientBuffer4), timeoutBig};
    auto stepGet5 = new Step{&ISplitter::SplitterGet, "SplitterGet", delayBase*4, id5, std::ref(clientBuffer5), timeoutBig};

    std::vector<StepBase*> aStepsConsumer1{new StepRepeat{stepGet1, 0, -2000}};
    std::vector<StepBase*> aStepsConsumer2{new StepRepeat{stepGet2, 0, -2000}};
    std::vector<StepBase*> aStepsConsumer3{new StepRepeat{stepGet3, 0, -2000}};
    std::vector<StepBase*> aStepsConsumer4{new StepRepeat{stepGet4, 0, -2000}};
    std::vector<StepBase*> aStepsConsumer5{new StepRepeat{stepGet5, 0, -2000} };

    log.start();

    std::thread tp(runTask, std::ref(aStepsProducer), s.get(), "producer");
    std::thread tc1(runTask, std::ref(aStepsConsumer1), s.get(), "consumer1");
    std::thread tc2(runTask, std::ref(aStepsConsumer2), s.get(), "consumer2");
    std::thread tc3(runTask, std::ref(aStepsConsumer3), s.get(), "consumer3");
    std::thread tc4(runTask, std::ref(aStepsConsumer4), s.get(), "consumer4");
    std::thread tc5(runTask, std::ref(aStepsConsumer5), s.get(), "consumer5");
    tp.join();
    tc1.join();
    tc2.join();
    tc3.join();
    tc4.join();
    tc5.join();

    log.dumpConsole();
    aux2::print_type((std::cout << "buffer1 "), clientBuffer1) << std::endl;
    aux2::print_type((std::cout << "buffer2 "), clientBuffer2) << std::endl;
    aux2::print_type((std::cout << "buffer3 "), clientBuffer3) << std::endl;
    aux2::print_type((std::cout << "buffer4 "), clientBuffer4) << std::endl;
    aux2::print_type((std::cout << "buffer5 "), clientBuffer5) << std::endl;
}

void test()
{
    return;

    test0();
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();

    std::string temp;
    std::cin >> temp;
}

} // namespace medialooks