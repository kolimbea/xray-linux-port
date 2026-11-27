// Minimal JSON reader for the GitHub releases API (objects, arrays, strings with escapes, numbers,
// literals). Parse errors yield a null value.
#pragma once
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace json
{
    struct Value
    {
        enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
        bool b = false;
        double n = 0;
        std::string s;
        std::vector<Value> a;
        std::map<std::string, Value> o;

        const Value &operator[](const std::string &key) const
        {
            static const Value none;
            auto it = o.find(key);
            return it == o.end() ? none : it->second;
        }
        std::string str() const { return kind == String ? s : std::string(); }
    };

    class Parser
    {
    public:
        explicit Parser(const std::string &text) : t(text) {}
        Value parse()
        {
            Value v;
            if (!value(v))
                return Value();
            return v;
        }

    private:
        const std::string &t;
        size_t i = 0;

        void ws()
        {
            while (i < t.size() && (t[i] == ' ' || t[i] == '\n' || t[i] == '\r' || t[i] == '\t'))
                ++i;
        }
        static void utf8(std::string &out, unsigned cp)
        {
            if (cp < 0x80)
                out += char(cp);
            else if (cp < 0x800)
                out += char(0xC0 | (cp >> 6)), out += char(0x80 | (cp & 0x3F));
            else if (cp < 0x10000)
                out += char(0xE0 | (cp >> 12)), out += char(0x80 | ((cp >> 6) & 0x3F)), out += char(0x80 | (cp & 0x3F));
            else
                out += char(0xF0 | (cp >> 18)), out += char(0x80 | ((cp >> 12) & 0x3F)), out += char(0x80 | ((cp >> 6) & 0x3F)),
                    out += char(0x80 | (cp & 0x3F));
        }
        bool hex4(unsigned &cp)
        {
            if (i + 4 > t.size())
                return false;
            cp = unsigned(strtoul(t.substr(i, 4).c_str(), nullptr, 16));
            i += 4;
            return true;
        }
        bool string(std::string &out)
        {
            if (t[i] != '"')
                return false;
            ++i;
            while (i < t.size() && t[i] != '"')
            {
                char c = t[i++];
                if (c != '\\')
                {
                    out += c;
                    continue;
                }
                if (i >= t.size())
                    return false;
                c = t[i++];
                switch (c)
                {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'u': {
                    unsigned cp;
                    if (!hex4(cp))
                        return false;
                    if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= t.size() && t[i] == '\\' && t[i + 1] == 'u')
                    {
                        i += 2;
                        unsigned lo;
                        if (!hex4(lo))
                            return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    utf8(out, cp);
                    break;
                }
                default: out += c;
                }
            }
            if (i >= t.size())
                return false;
            ++i;
            return true;
        }
        bool value(Value &v)
        {
            ws();
            if (i >= t.size())
                return false;
            const char c = t[i];
            if (c == '{')
            {
                v.kind = Value::Object;
                ++i;
                ws();
                if (i < t.size() && t[i] == '}')
                    return ++i, true;
                for (;;)
                {
                    ws();
                    std::string key;
                    if (i >= t.size() || !string(key))
                        return false;
                    ws();
                    if (i >= t.size() || t[i++] != ':')
                        return false;
                    if (!value(v.o[key]))
                        return false;
                    ws();
                    if (i < t.size() && t[i] == ',')
                    {
                        ++i;
                        continue;
                    }
                    if (i < t.size() && t[i] == '}')
                        return ++i, true;
                    return false;
                }
            }
            if (c == '[')
            {
                v.kind = Value::Array;
                ++i;
                ws();
                if (i < t.size() && t[i] == ']')
                    return ++i, true;
                for (;;)
                {
                    v.a.emplace_back();
                    if (!value(v.a.back()))
                        return false;
                    ws();
                    if (i < t.size() && t[i] == ',')
                    {
                        ++i;
                        continue;
                    }
                    if (i < t.size() && t[i] == ']')
                        return ++i, true;
                    return false;
                }
            }
            if (c == '"')
            {
                v.kind = Value::String;
                return string(v.s);
            }
            if (t.compare(i, 4, "true") == 0)
                return v.kind = Value::Bool, v.b = true, i += 4, true;
            if (t.compare(i, 5, "false") == 0)
                return v.kind = Value::Bool, i += 5, true;
            if (t.compare(i, 4, "null") == 0)
                return i += 4, true;
            char *end = nullptr;
            v.n = strtod(t.c_str() + i, &end);
            if (end == t.c_str() + i)
                return false;
            v.kind = Value::Number;
            i = size_t(end - t.c_str());
            return true;
        }
    };

    inline Value parse(const std::string &text)
    {
        return Parser(text).parse();
    }
} // namespace json
