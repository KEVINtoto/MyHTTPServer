
#include "database.h"
#include "log.h"

const std::string KEY_VALUE_DELIMITER = " : ";
const std::string VALUE_DELIMITER = "\t";

std::shared_ptr<Database> Database::m_db(nullptr);

Database::Database(std::string filepath, int max_edit_, int dump_interval_, int max_conn_)
    : m_db_file_path(filepath),
      m_thread_sem(max_conn_),
      m_db_thread(new pthread_t)
{
    m_db_file_path = filepath;
    m_max_edit = max_edit_;
    m_remain_conn = max_conn_;
    m_dump_interval = dump_interval_;
    m_edit_count = 0;
    m_db_thread_stop = false;
    loadFile();
    pthread_create(m_db_thread.get(), NULL, db_thread_run, this);
}

Database::DBConnection Database::getDBConnection() {
    return m_db;
}

void Database::releaseDBConnection() {
}

void Database::init(std::string filepath, int max_edit_, int dump_interval_, int max_conn_)
{
    if (m_db.get() == nullptr)
    {
        m_db.reset(new Database(filepath, max_edit_, dump_interval_, max_conn_));
    }
}

Database::~Database()
{
    m_db_thread_stop = true;
    pthread_join(*m_db_thread.get(), NULL);
    dumpFile();
}

bool Database::add(const key_type &k, const values_array &vs)
{
    // 上写锁
    m_rw_locker.writeLock();
    bool flag = m_data_dict.insert(k, vs);
    if (flag)
    {
        ++m_edit_count;
        LOG_INFO << "Key:" << k << " added successfully." << Log::endl;
    }
    else
    {
        LOG_WARN << "The key: " << k << " already exists in the database" << Log::endl;
    }
    m_rw_locker.writeUnlock();
    return flag;
}

bool Database::del(const key_type &k)
{
    // 上写锁
    m_rw_locker.writeLock();
    bool flag = m_data_dict.erase(k);
    if (flag)
    {
        ++m_edit_count;
        LOG_INFO << "Key:" << k << " deleted successfully." << Log::endl;
    }
    else
    {
        LOG_WARN << "No key: " << k << " exists in the database." << Log::endl;
    }
    m_rw_locker.writeUnlock();
    return flag;
}

bool Database::find(const key_type &k, values_array &vs)
{
    // 上读锁
    m_rw_locker.readLock();
    bool flag = m_data_dict.find(k, vs);
    if (flag)
    {
        LOG_INFO << "Key:" << k << " search successfully." << Log::endl;
    }
    else
    {
        LOG_WARN << "No key: " << k << " exists in the database." << Log::endl;
    }
    m_rw_locker.readUnlock();
    return flag;
}

bool Database::mod(const key_type &k, const values_array &vs)
{
    m_rw_locker.writeLock();
    bool flag = m_data_dict.modify(k, vs);
    if (flag)
    {
        ++m_edit_count;
        LOG_INFO << "Key:" << k << " mod successfully." << Log::endl;
    }
    else
    {
        LOG_WARN << "No key: " << k << " exists in the database." << Log::endl;
    }
    m_rw_locker.writeUnlock();
    return false;
}

void Database::loadFile()
{
    m_file_reader.open(m_db_file_path);
    std::string line;
    while (getline(m_file_reader, line))
    {
        auto pos = line.find(KEY_VALUE_DELIMITER);
        std::string k = line.substr(0, pos);
        std::string str_values = line.substr(pos + KEY_VALUE_DELIMITER.size());
        values_array vs;
        std::regex findValue("(.*?)" + VALUE_DELIMITER);
        std::sregex_iterator iter(str_values.begin(), str_values.end(), findValue);
        std::sregex_iterator end;
        for (; iter != end; iter++)
        {
            vs.emplace_back((*iter).str(1));
        }
        m_data_dict.insert(k, vs);
    }
    m_file_reader.close();
}

void Database::printData()
{
    for (auto iter = m_data_dict.begin(); iter != m_data_dict.end(); iter++)
    {
        std::cout << iter->getKey() << " : ";
        auto vs = iter->getValue();
        for (auto &v : vs)
        {
            std::cout << v << " ";
        }
        std::cout << std::endl;
    }
}

void *Database::db_thread_run(void *arg)
{
    auto obj_ptr = (Database *)arg;
    obj_ptr->db_async_write();
    return obj_ptr;
}

void Database::db_async_write()
{
    while (!m_db_thread_stop)
    {
        sleep(m_dump_interval);
        if (m_edit_count < m_max_edit)
        {
            continue;
        }
        dumpFile();
    }
}

void Database::dumpFile()
{
    m_rw_locker.readLock();
    m_file_writer.open(m_db_file_path);
    for (auto iter = m_data_dict.begin(); iter != m_data_dict.end(); iter++)
    {
        auto node = *iter;
        m_file_writer << node.getKey() + KEY_VALUE_DELIMITER;
        auto vs = node.getValue();
        auto len = vs.size();
        for (int i = 0; i < len; i++)
        {
            m_file_writer << vs[i] + VALUE_DELIMITER;
        }
        m_file_writer << "\n";
    }
    m_edit_count = 0;
    m_rw_locker.readUnlock();
    m_file_writer.flush();
    m_file_writer.close();
}