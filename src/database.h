
#pragma once

#include <iostream>
#include <fstream>
#include <regex>
#include <pthread.h>
#include <unistd.h>
#include "locker.h"
#include "skiplist.h"

class Database {
public:
    typedef std::string key_type;
    typedef std::string value_type;
    typedef std::vector<value_type> values_array;
    typedef std::shared_ptr<Database> DBConnection;

    static DBConnection getDBConnection();
    static void releaseDBConnection();

    static void init(std::string filepath, int max_edit_, int dump_interval_, int max_conn_);

    ~Database();
    
    bool add(const key_type &, const values_array &);
    bool del(const key_type &);
    bool find(const key_type &, values_array &);
    bool mod(const key_type &, const values_array &);
    void printData();

private:
    Database() {}
    Database(std::string filepath, int max_edit_, int dump_interval_, int max_conn_);
    Database(const Database &) {}
    Database& operator=(const Database &) {}

    void dumpFile();
    void loadFile();
    static void *db_thread_run(void *);
    void db_async_write();

private:
    static std::shared_ptr<Database> m_db;
    SkipList<key_type, values_array> m_data_dict;
    std::string m_db_file_path;

    unsigned int m_max_edit;
    unsigned int m_dump_interval;
    unsigned int m_edit_count;
    int m_remain_conn;

    std::ifstream m_file_reader;
    std::ofstream m_file_writer;

    RWLocker m_rw_locker;
    Locker m_thread_locker;
    Sem m_thread_sem;

    std::unique_ptr<pthread_t> m_db_thread;
    bool m_db_thread_stop;
};
