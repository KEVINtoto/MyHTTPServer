
#pragma once

#include <memory>
#include <string>
#include <vector>

class JSONParser;
class JSONValue;
class JSON;

enum JSON_TYPE
{
    JSON_NULL,   // null
    JSON_TRUE,   // true
    JSON_FALSE,  // false
    JSON_NUMBER, // 整数或浮点数
    JSON_STRING, // 字符串，"..." 形式
    JSON_ARRAY,  // 数组，[...] 形式
    JSON_OBJECT  // 对象，{...} 形式，存在嵌套
};

enum JSON_PARSE_RESULT
{
    JSON_PARSE_OK = 0,                       // 解析成功
    JSON_PARSE_EXPECT_VALUE,                 // 只解析到空白，期望解析到值
    JSON_PARSE_INVALID_VALUE,                // 无效值
    JSON_PARSE_ROOT_NOT_SINGULAR,            // 一个值之后，在空白之后还有其他字符
    JSON_PARSE_NUMBER_TOO_BIG,               // 超过 double 类型可表示的范围
    JSON_PARSE_MISS_QUOTATION_MARK,          // 缺少引号
    JSON_PARSE_INVALID_STRING_ESCAPE,        // 无效的字符串转义
    JSON_PARSE_INVALID_STRING_CHAR,          // 无效的字符串字符
    JSON_PARSE_INVALID_UNICODE_HEX,          // 无效的 unicode 码十六进制
    JSON_PARSE_INVALID_UNICODE_SURROGATE,    // 无效的 unicode 码代理项
    JSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET, // 缺少逗号或方括号
    JSON_PARSE_MISS_KEY,                     // 缺失键值
    JSON_PARSE_MISS_COLON,                   // 缺少冒号
    JSON_PARSE_MISS_COMMA_OR_CURLY_BRACKET,  // 缺少逗号或花括号
};

class JSONParser
{
public:
    JSONParser(JSONValue &, const std::string &);
    JSON_PARSE_RESULT parse();

private:
    void parse_whitespace();
    JSON_PARSE_RESULT parse_value();
    JSON_PARSE_RESULT parse_literal(const std::string &, JSON_TYPE);
    JSON_PARSE_RESULT parse_number();
    JSON_PARSE_RESULT parse_string();
    JSON_PARSE_RESULT parse_string_raw(std::string &);
    bool parse_hex4(const char *&, unsigned &);
    void encode_utf8(std::string &, unsigned);
    JSON_PARSE_RESULT parse_array();
    JSON_PARSE_RESULT parse_object();

    JSONValue &m_value;
    const char *m_pos;
};

class JSONValue
{
public:
    typedef double JSON_NUM_TYPE;
    typedef std::string JSON_STRING_TYPE;
    typedef std::vector<JSONValue> JSON_ARRAY_TYPE;
    typedef std::vector<std::pair<std::string, JSONValue>> JSON_OBJECT_TYPE;

    JSON_PARSE_RESULT parse(const std::string &content)
    {
        return JSONParser(*this, content).parse();
    }
    JSON_TYPE get_type() const
    {
        return m_type;
    }
    void set_type(JSON_TYPE t)
    {
        free();
        m_type = t;
    }
    const JSON_STRING_TYPE &get_string() const;
    void set_string(const JSON_STRING_TYPE &);
    const JSON_NUM_TYPE &get_number() const;
    void set_number(const JSON_NUM_TYPE &);
    const JSON_ARRAY_TYPE &get_array() const;
    const JSONValue &get_array_element(int index) const;
    void set_array(const JSON_ARRAY_TYPE &);
    const JSON_OBJECT_TYPE &get_object();
    void set_object(const JSON_OBJECT_TYPE &);
    const JSONValue &get_object_value(std::string key) const;

    JSONValue()
    {
        m_num = 0;
    }
    JSONValue(const JSONValue &json_value)
    {
        init(json_value);
    }
    JSONValue &operator=(const JSONValue &json_value)
    {
        free();
        init(json_value);
    }
    ~JSONValue() noexcept { free(); }

private:
    void init(const JSONValue &);
    void free() noexcept;

    JSON_TYPE m_type;
    union
    {
        JSON_NUM_TYPE m_num;
        JSON_STRING_TYPE m_str;
        JSON_ARRAY_TYPE m_arr;
        JSON_OBJECT_TYPE m_obj;
    };
};

class JSON
{
public:
    JSON() : m_value(new JSONValue) {}
    JSON_PARSE_RESULT parse(const std::string &content)
    {
        return m_value->parse(content);
    }
    JSON_TYPE get_type() const
    {
        return m_value->get_type();
    }
    const JSONValue::JSON_STRING_TYPE &get_string() const
    {
        return m_value->get_string();
    }
    const JSONValue::JSON_NUM_TYPE &get_number() const
    {
        return m_value->get_number();
    }
    const JSONValue::JSON_ARRAY_TYPE &get_array() const
    {
        return m_value->get_array();
    }
    const JSONValue &get_array_element(int index) const
    {
        return m_value->get_array_element(index);
    }
    const JSONValue::JSON_OBJECT_TYPE &get_object()
    {
        return m_value->get_object();
    }
    const JSONValue &get_object_value(std::string key) const
    {
        return m_value->get_object_value(key);
    }

private:
    std::unique_ptr<JSONValue> m_value;
};