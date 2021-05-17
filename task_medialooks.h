#pragma once
#include <vector>
#include <unordered_map>
#include <memory>
#include <queue>
#include <deque>
#include <chrono>
#include <thread>
#include <mutex>

#define IN
#define OUT

namespace medialooks
{

class ISplitter
{
public:
    // ISplitter интерфейс
    virtual ~ISplitter() = default;
    virtual bool SplitterInfoGet(OUT size_t* _pzMaxBuffers, OUT size_t* _pzMaxClients) = 0;

    // Кладём данные в очередь. Если какой-то клиент не успел ещё забрать свои данные, и количество буферов (задержка) для него больше максимального значения, то ждём пока не освободятся буфера (клиент заберет данные) не более _nTimeOutMsec (**). Если по истечению времени данные так и не забраны, то удаляем старые данные для этого клиента, добавляем новые (по принципу FIFO) (*). Возвращаем код ошибки, который дает понять что один или несколько клиентов “пропустили” свои данные.
    virtual int32_t SplitterPut(IN const std::shared_ptr<std::vector<uint8_t>>& _pVecPut, IN int32_t _nTimeOutMsec) = 0;

    // Сбрасываем все буфера, прерываем все ожидания. (после вызова допустима дальнейшая работа)
    virtual int32_t SplitterFlush() = 0;

    // Добавляем нового клиента - возвращаем уникальный идентификатор клиента.
    virtual bool SplitterClientAdd(OUT uint32_t* _punClientID) = 0;

    // Удаляем клиента по идентификатору, если клиент находиться в процессе ожидания буфера, то прерываем ожидание.
    virtual bool SplitterClientRemove(IN uint32_t _unClientID) = 0;

    // Перечисление клиентов, для каждого клиента возвращаем его идентификатор, количество буферов в очереди (задержку) для этого клиента а также количество отброшенных буферов.
    virtual bool SplitterClientGetCount(OUT size_t* _pnCount) = 0;
    virtual bool SplitterClientGetByIndex(IN size_t _zIndex, OUT uint32_t* _punClientID, OUT size_t* _pzLatency, OUT size_t* _pzDropped) = 0;

    // По идентификатору клиента возвращаем задержку
    virtual bool SplitterClientGetById(IN uint32_t _unClientID, OUT size_t* _pzLatency, OUT size_t* _pzDropped) = 0;

    // По идентификатору клиента запрашиваем данные, если данных пока нет, то ожидаем не более _nTimeOutMsec (**) пока не будут добавлены новые данные, в случае превышения времени ожидания - возвращаем ошибку.
    virtual int32_t SplitterGet(IN uint32_t _nClientID, OUT std::shared_ptr<std::vector<uint8_t>>& _pVecGet, IN int32_t _nTimeOutMsec) = 0;

    // Закрытие объекта сплиттера - все ожидания должны быть прерваны все вызовы возвращают соответствующую ошибку. Все клиенты удалены. (после вызова допустимо добавление новых клиентов и дальнейшая работа)
    virtual void SplitterClose() = 0;
};

enum class ESplitterError : int32_t
{
    eOk = 0,
    eTimeout = 1,
    eMissBuffer = 2,
    eClientNotFound = 3,
    eInterrupted = 4,
    eGeneral = 5
};

class ISplitterImp : public ISplitter
{
    using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
    using Clock = std::chrono::steady_clock;
    using BufferData = std::pair<TimePoint, const std::shared_ptr<std::vector<uint8_t>>>;
    struct Client
    {
        Client(uint32_t id) : m_id(id) {}
        uint32_t m_id{0};
        size_t m_dropped{0};
        TimePoint m_updated;

        Client* m_prev{nullptr};
        Client* m_next{nullptr};
    };
    struct Keeper
    {
        Keeper(std::atomic<uint32_t>& v, std::condition_variable& cv) : m_val(v), m_cv(cv) { ++m_val; }
        ~Keeper()
        {
            --m_val;
            m_cv.notify_all();
        }
        std::atomic<uint32_t>& m_val;
        std::condition_variable& m_cv;
    };
public:
    ISplitterImp(IN size_t _zMaxBuffers, IN size_t _zMaxClients) : m_zMaxBuffers(_zMaxBuffers), m_zMaxClients(_zMaxClients) {}
    ~ISplitterImp() = default;
    bool SplitterInfoGet(OUT size_t* _pzMaxBuffers, OUT size_t* _pzMaxClients) override
    {
        (*_pzMaxBuffers) = m_zMaxBuffers;
        (*_pzMaxClients) = m_zMaxClients;
        return true;
    }
    int32_t SplitterPut(IN const std::shared_ptr<std::vector<uint8_t>>& _pVecPut, IN int32_t _nTimeOutMsec) override
    {
        if(m_bInterrupt)
            return std::underlying_type_t<ESplitterError>(ESplitterError::eInterrupted);
        Keeper keeper(m_nThreadCount, m_cvClear);

        TimePoint tpRemove;
        ESplitterError er = ESplitterError::eOk;
        if(m_zMaxBuffers == m_aData.size()) // no lock here, it is ok
        {
            auto conditionWakeUp = [this, &tpRemove] // checks if the slowest client got the first buffer element
            {
                if(m_bInterrupt)
                    return true;
                tpRemove = slowestTime();
                bool b = tpRemove > m_aData.front().first;
                return b;
            };

            bool bUpdateDropped = false;
            std::unique_lock lk(m_mutClient);
            if(_nTimeOutMsec < 0)
            {
                m_cvPut.wait(lk, conditionWakeUp);
                if(m_bInterrupt)
                    return std::underlying_type_t<ESplitterError>(ESplitterError::eInterrupted);
            }
            else
            {
                m_cvPut.wait_for(lk, std::chrono::milliseconds(_nTimeOutMsec), conditionWakeUp);
                if(m_bInterrupt)
                    return std::underlying_type_t<ESplitterError>(ESplitterError::eInterrupted);
                bUpdateDropped = true;
            }

            if(bUpdateDropped)
            {
                Client* client = m_old;
                const TimePoint timeDrop = std::max(tpRemove, m_aData.front().first);
                while(client && client->m_updated < timeDrop)
                {
                    ++client->m_dropped;
                    er = ESplitterError::eMissBuffer;
                    client = client->m_next;
                }
            }
        }

        // add data to the buffer
        {
            std::lock_guard gb(m_mutBuffer);
            if(m_zMaxBuffers == m_aData.size())
            {
                m_aData.pop_front(); // at least one pop
                while(!m_aData.empty() && tpRemove > m_aData.front().first)
                    m_aData.pop_front();
            }
            m_aData.emplace_back(Clock::now(), _pVecPut);
        }
        m_cvGet.notify_all();
        return std::underlying_type_t<ESplitterError>(er);
    }
    int32_t SplitterFlush() override
    {
        return flushImp(false);
    }
    bool SplitterClientAdd(OUT uint32_t* _punClientID) override
    {
        std::lock_guard g(m_mutClient);
        if(m_aClient.size() == m_zMaxClients)
            return false;

        ++m_clientId;
        (*_punClientID) = m_clientId;
        auto& client = m_aClient.emplace_back(std::make_unique<Client>(m_clientId));
        client->m_updated = Clock::now();
        putClient(client.get());
        return true;
    }
    bool SplitterClientRemove(IN uint32_t _unClientID) override
    {      
        {
            std::lock_guard g(m_mutClient);
            size_t ind;
            if(!clientIndex(_unClientID, &ind))
                return false;

            extractClient(m_aClient[ind].get());
            m_aClient.erase(m_aClient.begin() + ind);
        }
        m_cvPut.notify_all();
        return true;
    }
    bool SplitterClientGetCount(OUT size_t* _pnCount) override
    {
        std::lock_guard g(m_mutClient);
        (*_pnCount) = m_aClient.size();
        return true;
    }
    bool SplitterClientGetByIndex(IN size_t _zIndex, OUT uint32_t* _punClientID, OUT size_t* _pzLatency, OUT size_t* _pzDropped) override
    {
        std::lock_guard g(m_mutClient);
        if(m_aClient.size() <= _zIndex)
            return false;

        const TimePoint tp = Clock::now();
        const Client& c = *m_aClient[_zIndex];
        (*_punClientID) = c.m_id;
        (*_pzLatency) = std::chrono::duration_cast<std::chrono::milliseconds>(tp - c.m_updated).count();
        (*_pzDropped) = c.m_dropped;
        return true;
    }
    bool SplitterClientGetById(IN uint32_t _unClientID, OUT size_t* _pzLatency, OUT size_t* _pzDropped) override
    {
        std::lock_guard g(m_mutClient);
        size_t ind;
        if(!clientIndex(_unClientID, &ind))
            return false;

        const TimePoint tp = Clock::now();
        const Client& c = *m_aClient[ind];
        (*_pzLatency) = std::chrono::duration_cast<std::chrono::milliseconds>(tp - c.m_updated).count();
        (*_pzDropped) = c.m_dropped;
        return true;
    }
    int32_t SplitterGet(IN uint32_t _nClientID, OUT std::shared_ptr<std::vector<uint8_t>>& _pVecGet, IN int32_t _nTimeOutMsec) override
    {
        if(m_bInterrupt)
            return std::underlying_type_t<ESplitterError>(ESplitterError::eInterrupted);
        Keeper keeper(m_nThreadCount, m_cvClear);

        TimePoint tpClient;
        {   // get the client time
            size_t ind;
            std::lock_guard g(m_mutClient);
            if(!clientIndex(_nClientID, &ind))
                return std::underlying_type_t<ESplitterError>(ESplitterError::eClientNotFound);
            tpClient = m_aClient[ind]->m_updated;
        }

        // waiting
        {
            std::unique_lock<std::mutex> lk(m_mutBuffer);
            auto conditionWakeUp = [this, tpClient]
            { 
                return m_bInterrupt || hasData(tpClient);
            };
            if(_nTimeOutMsec < 0)
            {
                m_cvGet.wait(lk, conditionWakeUp);
                if(m_bInterrupt)
                    return std::underlying_type_t<ESplitterError>(ESplitterError::eInterrupted);
            }
            else
            {
                m_cvGet.wait_for(lk, std::chrono::milliseconds(_nTimeOutMsec), conditionWakeUp);
                if(m_bInterrupt)
                    return std::underlying_type_t<ESplitterError>(ESplitterError::eInterrupted);
                if(!hasData(tpClient))
                    return std::underlying_type_t<ESplitterError>(ESplitterError::eTimeout);
            }

            // fill _pVecGet
            auto itBegin = std::upper_bound(m_aData.begin(), m_aData.end(), tpClient, [](TimePoint tp, const BufferData& el) { return tp < el.first; });
            for(auto it = itBegin; it != m_aData.end(); ++it)
            {
                // assure it->first > tpClient
                _pVecGet->insert(_pVecGet->end(), it->second->begin(), it->second->end());
            }
        }

        // update the client
        {   // get client time
            size_t ind;
            std::lock_guard g(m_mutClient);
            if(!clientIndex(_nClientID, &ind))
                return std::underlying_type_t<ESplitterError>(ESplitterError::eClientNotFound);
            Client* client = m_aClient[ind].get();
            client->m_updated = Clock::now();
            if(m_recent != client)
            {
                extractClient(client);
                putClient(client);
            }
        }
        m_cvPut.notify_all();
        return std::underlying_type_t<ESplitterError>(ESplitterError::eOk);
    }
    void SplitterClose() override
    {
        flushImp(true);
    }
private:
    int32_t flushImp(bool bClear)
    {
        m_bInterrupt = true;
        m_cvPut.notify_all();
        m_cvGet.notify_all();

        ESplitterError code = ESplitterError::eOk;
        {
            std::unique_lock<std::mutex> lk(m_mutBuffer);
            m_cvClear.wait(lk, [this, &code]()
                {
                    if(0 == m_nThreadCount)
                        return true;
                    else
                    {
                        code = ESplitterError::eInterrupted;
                        return false;
                    }
                });
            if(bClear)
                m_aData.clear();
        }

        if(bClear)
        {
            std::lock_guard guard(m_mutClient);
            m_aClient.clear();
        }
        m_bInterrupt = false;
        return std::underlying_type_t<ESplitterError>(code);
    }
    void extractClient(Client* client)
    {
        if(m_old == m_recent)
        {
            m_old = nullptr;
            m_recent = nullptr;
        }
        else if(m_old == client)
        {
            m_old = client->m_next;
            m_old->m_prev = nullptr;
        }
        else if(m_recent == client)
        {
            m_recent = client->m_prev;
            m_recent->m_next = nullptr;
        }
        else
        {
            Client* p = client->m_prev;
            Client* n = client->m_next;
            p->m_next = n;
            n->m_prev = p;
        }
        client->m_prev = nullptr;
        client->m_next = nullptr;
    }
    void putClient(Client* client)
    {
        if(m_recent)
        {
            m_recent->m_next = client;
            client->m_prev = m_recent;
            m_recent = client;
        }
        else
        {
            m_recent = client;
            m_old = client;
        }
    }
    bool hasData(TimePoint tp) const
    {
        return !m_aData.empty() && tp < m_aData.back().first;
    }
    bool clientIndex(IN uint32_t _nClientID, OUT size_t* _pzIndex) const
    {   
        if(m_aClient.empty())
            return false;
        auto it = std::upper_bound(m_aClient.begin(), m_aClient.end(), _nClientID, [](uint32_t id, const std::unique_ptr<Client>& el) { return id < el->m_id; });
        if(it == m_aClient.begin())
            return false;
        auto itElem = it-1;
        if((*itElem)->m_id == _nClientID)
        {
            (*_pzIndex) = itElem - m_aClient.begin();
            return true;
        }
        else
            return false;
    }
    TimePoint slowestTime() const
    {
        return m_old ? m_old->m_updated : Clock::now();
    }

private:
    size_t m_zMaxBuffers;
    size_t m_zMaxClients;
    uint32_t m_clientId{0};

    std::mutex m_mutClient;
    std::vector<std::unique_ptr<Client>> m_aClient;
    Client* m_recent{nullptr};
    Client* m_old{nullptr};

    std::atomic<bool> m_bInterrupt{false};
    std::atomic<uint32_t> m_nThreadCount{0};

    std::mutex m_mutBuffer;
    std::condition_variable m_cvClear;
    std::condition_variable m_cvPut;
    std::condition_variable m_cvGet;
    std::deque<BufferData> m_aData;
};

std::shared_ptr<ISplitter> SplitterCreate(IN size_t _zMaxBuffers, IN size_t _zMaxClients)
{
    return std::make_shared<ISplitterImp>(_zMaxBuffers, _zMaxClients);
}

} // medialooks