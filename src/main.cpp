
#include "server.h"
#include "json.h"
#include "log.h"

int main(int argc, char *argv[])
{
    const std::string JSON_CONFIG_FILE_PATH = "config.json";
    const std::string JSON_KEY_PORT = "port";
    const std::string JSON_KEY_DEFAULT_FILE = "default file";
    const std::string JSON_KEY_LONG_CONN = "long connection";
    const std::string JSON_KEY_MAX_HTTP_CONN = "max http connection";
    const std::string JSON_KEY_MAX_EVENT = "max events";
    const std::string JSON_KEY_HTTP_TIMEOUT = "http timeout";
    const std::string JSON_KEY_THREAD_N = "thread number";
    const std::string JSON_KEY_MAX_REQUEST = "max requests";
    const std::string JSON_KEY_DB_FILE = "database file";
    const std::string JSON_KEY_MAX_N_EDIT = "max number of edit";
    const std::string JSON_KEY_DUMP_INTERVAL = "dump interval";
    const std::string JSON_KEY_MAX_N_DB_CONN = "max number of db connection";
    const std::string JSON_KEY_LOG_FILE_PATH = "log file path";
    const std::string JSON_KEY_LOG_MAX_LINE = "log max line";
    const std::string JSON_KEY_CERT_PATH = "cert path";
    const std::string JSON_KEY_CERT_PASSWD = "cert password";
    const std::string JSON_KEY_PRIVATE_KEY_PATH = "private key path";

    
    std::string content;
    struct stat file_stat;
    if (stat(JSON_CONFIG_FILE_PATH.c_str(), &file_stat) < 0)
    {
        std::cout << "JSON configuration file \"" << JSON_CONFIG_FILE_PATH << "\" does not exist." << std::endl;
        return 1;
    }
    FILE *file = fopen(JSON_CONFIG_FILE_PATH.c_str(), "r");
    content.resize(file_stat.st_size);
    fread(const_cast<char *>(content.data()), file_stat.st_size, 1, file);
    fclose(file);

    // 解析配置文件
    JSON json;
    auto json_parse_result = json.parse(content);
    assert(json_parse_result == JSON_PARSE_OK);

    // 初始化 Log 实例
    Log::getInstance()->init(json.get_object_value(JSON_KEY_LOG_FILE_PATH).get_string(),
                             json.get_object_value(JSON_KEY_LOG_MAX_LINE).get_number());

    // 初始化数据库
    Database::init(json.get_object_value(JSON_KEY_DB_FILE).get_string(),
                   json.get_object_value(JSON_KEY_MAX_N_EDIT).get_number(),
                   json.get_object_value(JSON_KEY_DUMP_INTERVAL).get_number(),
                   json.get_object_value(JSON_KEY_MAX_N_DB_CONN).get_number());
    
    // 开始运行服务端
    Server server(json.get_object_value(JSON_KEY_PORT).get_number(),
                  json.get_object_value(JSON_KEY_MAX_HTTP_CONN).get_number(),
                  json.get_object_value(JSON_KEY_MAX_EVENT).get_number(),
                  json.get_object_value(JSON_KEY_THREAD_N).get_number(),
                  json.get_object_value(JSON_KEY_MAX_REQUEST).get_number(),
                  json.get_object_value(JSON_KEY_HTTP_TIMEOUT).get_number());
    server.init(json.get_object_value(JSON_KEY_CERT_PATH).get_string(),
                json.get_object_value(JSON_KEY_CERT_PASSWD).get_string(),
                json.get_object_value(JSON_KEY_PRIVATE_KEY_PATH).get_string());
    LOG_INFO << "Server starting......" << Log::endl;
    server.start();
    LOG_INFO << "Server started." << Log::endl;
    server.loop();

    return 0;
}
