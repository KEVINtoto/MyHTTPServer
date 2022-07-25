
#include <assert.h>
#include <math.h>
#include <sys/stat.h>
#include "json.h"

JSONParser::JSONParser(JSONValue &val, const std::string &content)
    : m_value(val), m_pos(content.c_str())
{
}

JSON_PARSE_RESULT JSONParser::parse()
{
    m_value.set_type(JSON_NULL);
    parse_whitespace(); // 解析前置空格
    JSON_PARSE_RESULT ret = parse_value(); // 解析值
    if (ret == JSON_PARSE_OK)
    {
        parse_whitespace(); // 解析后置空格
        if (*m_pos != '\0') // 非单一根值
        {
            m_value.set_type(JSON_NULL);
            ret = JSON_PARSE_ROOT_NOT_SINGULAR;
            // LOG_ERROR << "JSON: parse root not singular." << Log::endl;
        }
    }
    else
    {
        m_value.set_type(JSON_NULL);
    }
    return ret;
}

// 解析空格、制表符、换行符和回车符
void JSONParser::parse_whitespace()
{
    while (*m_pos == ' ' || *m_pos == '\t' || *m_pos == '\n' || *m_pos == '\r')
    {
        ++m_pos;
    }
}

JSON_PARSE_RESULT JSONParser::parse_value()
{
    switch (*m_pos)
    {
    case 'n':
        return parse_literal("null", JSON_NULL);
    case 't':
        return parse_literal("true", JSON_TRUE);
    case 'f':
        return parse_literal("false", JSON_FALSE);
    case '\"':
        return parse_string();
    case '[':
        return parse_array();
    case '{':
        return parse_object();
    default:
        return parse_number();
    case '\0':
        return JSON_PARSE_EXPECT_VALUE;
    }
}

JSON_PARSE_RESULT JSONParser::parse_literal(const std::string &str, JSON_TYPE type)
{
    auto len = str.size();
    for (int i = 0; i < len; i++)
    {
        if (m_pos[i] != str[i])
        {
            return JSON_PARSE_INVALID_VALUE;
        }
    }
    m_pos += len;
    m_value.set_type(type);
    return JSON_PARSE_OK;
}

JSON_PARSE_RESULT JSONParser::parse_number()
{
    auto p = m_pos;
    if (*p == '-')
        p++;
    if (*p == '0')
        p++;
    else
    {
        if (!isdigit(*p))
            return JSON_PARSE_INVALID_VALUE;
        for (p++; isdigit(*p); p++)
            ;
    }
    if (*p == '.')
    {
        p++;
        if (!isdigit(*p))
            return JSON_PARSE_INVALID_VALUE;
        for (p++; isdigit(*p); p++)
            ;
    }
    if (*p == 'e' || *p == 'E')
    {
        p++;
        if (*p == '+' || *p == '-')
            p++;
        if (!isdigit(*p))
            return JSON_PARSE_INVALID_VALUE;
        for (p++; isdigit(*p); p++)
            ;
    }
    errno = 0;
    double num = strtod(m_pos, NULL);
    if (errno == ERANGE && (num == HUGE_VAL || num == -HUGE_VAL))
        return JSON_PARSE_NUMBER_TOO_BIG;
    m_value.set_number(num);
    m_pos = p;
    return JSON_PARSE_OK;
}

JSON_PARSE_RESULT JSONParser::parse_string()
{
    std::string tmp;
    auto ret = parse_string_raw(tmp);
    if (ret == JSON_PARSE_OK)
    {
        m_value.set_string(tmp);
    }
    return ret;
}

JSON_PARSE_RESULT JSONParser::parse_string_raw(std::string &tmp)
{
    // m_pos 一开始指向 \"
    auto p = ++m_pos;
    unsigned u, u2;
    while (*p != '\"')
    {
        if (*p == '\0')
        {
            return JSON_PARSE_MISS_QUOTATION_MARK;
        }
        if (*p == '\\' && ++p)
        {
            switch (*p++)
            {
            case '\"':
                tmp += '\"';
                break;
            case '\\':
                tmp += '\\';
                break;
            case '/':
                tmp += '/';
                break;
            case 'b':
                tmp += '\b';
                break;
            case 'f':
                tmp += '\f';
                break;
            case 'n':
                tmp += '\n';
                break;
            case 'r':
                tmp += '\r';
                break;
            case 't':
                tmp += '\t';
                break;
            case 'u':
                if (!parse_hex4(p, u))
                {
                    return JSON_PARSE_INVALID_UNICODE_HEX;
                }
                if (u >= 0xD800 && u <= 0xDBFF)
                {
                    if (*p++ != '\\')
                    {
                        return JSON_PARSE_INVALID_UNICODE_SURROGATE;
                    }
                    if (*p++ != 'u')
                    {
                        return JSON_PARSE_INVALID_UNICODE_SURROGATE;
                    }
                    if (!parse_hex4(p, u2))
                    {
                        return JSON_PARSE_INVALID_UNICODE_HEX;
                    }
                    if (u2 < 0xDC00 || u2 > 0xDFFF)
                    {
                        return JSON_PARSE_INVALID_UNICODE_SURROGATE;
                    }
                    u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                }
                encode_utf8(tmp, u);
                break;
            default:
                return JSON_PARSE_INVALID_STRING_ESCAPE;
            }
        }
        else if ((unsigned char)*p < 0x20)
        {
            return JSON_PARSE_INVALID_STRING_CHAR;
        }
        else
        {
            tmp += *p++;
        }
    }
    m_pos = ++p;
    return JSON_PARSE_OK;
}

bool JSONParser::parse_hex4(const char *&p, unsigned &u)
{
    u = 0;
    for (int i = 0; i < 4; i++)
    {
        char ch = *p++;
        u <<= 4;
        if (isdigit(ch))
            u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F')
            u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f')
            u |= ch - ('a' - 10);
        else
            return false;
    }
    return true;
}

void JSONParser::encode_utf8(std::string &str, unsigned u)
{
    if (u <= 0x7f)
    {
        str += static_cast<char>(u & 0xFF);
    }
    else if (u <= 0x7FF)
    {
        str += static_cast<char>(0xC0 | ((u >> 6) & 0xFF));
        str += static_cast<char>(0x80 | (u & 0x3F));
    }
    else if (u <= 0xFFFF)
    {
        str += static_cast<char>(0xE0 | ((u >> 12) & 0xFF));
        str += static_cast<char>(0x80 | ((u >> 6) & 0x3F));
        str += static_cast<char>(0x80 | (u & 0x3F));
    }
    else
    {
        assert(u <= 0x10FFFF);
        str += static_cast<char>(0xF0 | ((u >> 18) & 0xFF));
        str += static_cast<char>(0x80 | ((u >> 12) & 0x3F));
        str += static_cast<char>(0x80 | ((u >> 6) & 0x3F));
        str += static_cast<char>(0x80 | (u & 0x3F));
    }
}

JSON_PARSE_RESULT JSONParser::parse_array()
{
    assert(*m_pos == '[');
    m_pos++;
    parse_whitespace();
    std::vector<JSONValue> tmp;
    if (*m_pos == ']')
    {
        ++m_pos;
        m_value.set_array(tmp);
        return JSON_PARSE_OK;
    }
    JSON_PARSE_RESULT ret;
    while (true)
    {
        ret = parse_value();
        if (ret != JSON_PARSE_OK)
        {
            m_value.set_type(JSON_NULL);
            return ret;
        }
        tmp.emplace_back(m_value);
        parse_whitespace();
        if (*m_pos == ',')
        {
            ++m_pos;
            parse_whitespace();
        }
        else if (*m_pos == ']')
        {
            ++m_pos;
            m_value.set_array(tmp);
            return JSON_PARSE_OK;
        }
        else
        {
            m_value.set_type(JSON_NULL);
            return JSON_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
        }
    }
}

JSON_PARSE_RESULT JSONParser::parse_object()
{
    assert(*m_pos == '{');
    ++m_pos;
    parse_whitespace();
    std::vector<std::pair<std::string, JSONValue>> tmp;
    std::string key;
    if (*m_pos == '}')
    {
        ++m_pos;
        m_value.set_object(tmp);
        return JSON_PARSE_OK;
    }
    JSON_PARSE_RESULT ret;
    while (true)
    {
        if (*m_pos != '\"')
        {
            return JSON_PARSE_MISS_KEY;
        }
        if (parse_string_raw(key) != JSON_PARSE_OK)
        {
            return JSON_PARSE_MISS_KEY;
        }
        parse_whitespace();
        if (*m_pos++ != ':')
        {
            return JSON_PARSE_MISS_COLON;
        }
        parse_whitespace();
        ret = parse_value();
        if (ret != JSON_PARSE_OK)
        {
            return ret;
        }
        tmp.emplace_back(key, m_value);
        m_value.set_type(JSON_NULL);
        key.clear();
        parse_whitespace();
        if (*m_pos == ',')
        {
            ++m_pos;
            parse_whitespace();
        }
        else if (*m_pos == '}')
        {
            ++m_pos;
            m_value.set_object(tmp);
            return JSON_PARSE_OK;
        }
        else
        {
            m_value.set_type(JSON_NULL);
            return JSON_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
        }
    }
}

void JSONValue::init(const JSONValue &rhs)
{
    m_type = rhs.m_type;
    m_num = 0;
    switch (m_type)
    {
    case JSON_NUMBER:
        m_num = rhs.m_num;
        break;
    case JSON_STRING:
        new (&m_str) std::string(rhs.m_str);
        break;
    case JSON_ARRAY:
        new (&m_arr) std::vector<JSONValue>(rhs.m_arr);
        break;
    case JSON_OBJECT:
        new (&m_obj) std::vector<std::pair<std::string, JSONValue>>(rhs.m_obj);
        break;
    }
}

void JSONValue::free() noexcept
{
    switch (m_type)
    {
    case JSON_STRING:
        m_str.~basic_string();
        break;
    case JSON_ARRAY:
        m_arr.~vector<JSONValue>();
        break;
    case JSON_OBJECT:
        m_obj.~vector<std::pair<std::string, JSONValue>>();
    }
}

const JSONValue::JSON_STRING_TYPE &JSONValue::get_string() const
{
    assert(m_type == JSON_STRING);
    return m_str;
}
void JSONValue::set_string(const JSON_STRING_TYPE &tmp)
{
    if (m_type == JSON_STRING)
    {
        m_str = tmp;
    }
    else
    {
        free();
        m_type = JSON_STRING;
        new (&m_str) JSON_STRING_TYPE(tmp);
    }
}
const JSONValue::JSON_NUM_TYPE &JSONValue::get_number() const
{
    assert(m_type == JSON_NUMBER);
    return m_num;
}
void JSONValue::set_number(const JSON_NUM_TYPE &tmp)
{
    free();
    m_type = JSON_NUMBER;
    m_num = tmp;
}
const JSONValue::JSON_ARRAY_TYPE &JSONValue::get_array() const
{
    assert(m_type == JSON_ARRAY);
    return m_arr;
}
void JSONValue::set_array(const JSON_ARRAY_TYPE &tmp)
{
    if (m_type == JSON_ARRAY)
    {
        m_arr = tmp;
    }
    else
    {
        free();
        m_type = JSON_ARRAY;
        new (&m_arr) JSON_ARRAY_TYPE(tmp);
    }
}
const JSONValue::JSON_OBJECT_TYPE &JSONValue::get_object()
{
    assert(m_type == JSON_OBJECT);
    return m_obj;
}
void JSONValue::set_object(const JSONValue::JSON_OBJECT_TYPE &tmp)
{
    if (m_type == JSON_OBJECT)
    {
        m_obj = tmp;
    }
    else
    {
        free();
        m_type = JSON_OBJECT;
        new (&m_obj) JSON_OBJECT_TYPE(tmp);
    }
}

const JSONValue &JSONValue::get_array_element(int index) const
{
    assert(m_type == JSON_ARRAY);
    return m_arr[index];
}

const JSONValue &JSONValue::get_object_value(std::string key) const
{
    assert(m_type == JSON_OBJECT);
    for (auto &p : m_obj)
    {
        if (p.first == key)
            return p.second;
    }
}


