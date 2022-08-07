// Copyright (c) NetXS Group.
// Licensed under the MIT license.

#ifndef NETXS_XML_HPP
#define NETXS_XML_HPP

#include "../ui/layout.hpp"

namespace netxs::xml
{
    using namespace netxs::utf;
    using namespace netxs::ui::atoms;
    using dict = std::unordered_map<text, text>;

    static constexpr auto spaces = " \n\r\t";
    enum class type
    {
        none,
        token,
        open,
        close,
    };
    template<class T>
    auto escape(T&& line)
    {
        auto temp = text{ std::forward<T>(line) };
        utf::change(temp, "\\"s,   "\\\\"s);
        utf::change(temp, "\""s,   "\\\""s);
        utf::change(temp, "\x1b"s, "\\e"s );
        utf::change(temp, "\n"s,   "\\n"s );
        utf::change(temp, "\r"s,   "\\r"s );
        utf::change(temp, "\t"s,   "\\t"s );
        utf::change(temp, "\a"s,   "\\a"s );
        return temp;
    }
    auto unescape(qiew line)
    {
        auto crop = text{};
        crop.reserve(line.size());
        while (line)
        {
            auto c = line.pop_front();
            if (c == '\\' && line)
            {
                auto c = line.pop_front();
                switch (c)
                {
                    case 'e' : crop.push_back('\x1b'); break;
                    case 't' : crop.push_back('\t'  ); break;
                    case 'r' : crop.push_back('\n'  ); break;
                    case 'n' : crop.push_back('\n'  ); break;
                    case 'a' : crop.push_back('\a'  ); break;
                    case '\"': crop.push_back('\"'  ); break;
                    case '\'': crop.push_back('\''  ); break;
                    default:   crop.push_back('\\'  );
                               crop.push_back(c     ); break;
                };
            }
            else crop.push_back(c);
        }
        return crop;
    }
    auto open(view& data, type& next)
    {
        auto iter = data.find('<');
        if (iter != view::npos)
        {
            data.remove_prefix(iter + 1);
            next = type::open;
            return true;
        }
        else
        {
            next = type::none;
            return faux;
        }
    }
    auto attr(view& data, text& item, type& next)
    {
        utf::trim_front(data, spaces);
        item.clear();
        if (data.empty()) next = type::none;
        else
        {
            auto c = data.front();
            if (c == '/')
            {
                data.remove_prefix(1);
                if (data.size() && data.front() == '>')
                {
                    data.remove_prefix(1);
                    next = type::close;
                }
                else next = type::none;
            }
            else
            {
                item = get_tail(data, " \t\n\r=");
                next = type::token;
                utf::to_low(item);
            }
        }
        return next == type::token;
    }
    auto value(view& data)
    {
        auto crop = text{};
        utf::trim_front(data, spaces);

        if (data.empty() || data.front() != '=') return crop;
        data.remove_prefix(1); // remove '='.
        utf::trim_front(data, spaces);

        auto delim = data.front();
        if (delim != '/')
        {
            if (delim != '\'' && delim != '\"') crop = utf::get_tail(data, " \t\n\r/>");
            else                                crop = utf::get_quote(data, view(&delim, 1));
            crop = xml::unescape(crop);
        }
        return crop;
    }
    template<class T>
    auto take(dict& item, text const& attr, T fallback = {})
    { }
    template<>
    auto take<si32>(dict& item, text const& attr, si32 result)
    {
        if (auto v = utf::to_int(item[attr]))
        {
            result = v.value();
        }
        return result;
    }
    template<>
    auto take<view>(dict& item, text const& attr, view result)
    {
        if (auto iter = item.find(attr); iter != item.end())
        {
            result = view{ iter->second };
        }
        return result;
    }
    template<>
    auto take<bool>(dict& item, text const& attr, bool result)
    {
        if (auto iter = item.find(attr); iter != item.end())
        {
            auto& value = utf::to_low(iter->second);
            result = value.empty() || value.starts_with("1")  // 1 - true
                                   || value.starts_with("o")  // on
                                   || value.starts_with("y")  // yes
                                   || value.starts_with("t"); // true
        }
        return result;
    }
    template<>
    auto take<twod>(dict& item, text const& attr, twod result)
    {
        if (auto iter = item.find(attr); iter != item.end())
        {
            auto shadow = view{ iter->second };
            utf::trim_front(shadow, " ({[\"\'");
            if (auto x = utf::to_int(shadow))
            {
                utf::trim_front(shadow, " ,.x/:;");
                if (auto y = utf::to_int(shadow))
                {
                    result.x = x.value();
                    result.y = y.value();
                }
            }
        }
        return result;
    }
    template<>
    auto take<rgba>(dict& item, text const& attr, rgba result)
    {
        auto tobyte = [](auto c)
        {
                 if (c >= '0' && c <= '9') return c - '0';
            else if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            else                           return 0;
        };

        if (auto iter = item.find(attr); iter != item.end())
        {
            auto& value = utf::to_low(iter->second);
            auto shadow = view{ value };
            utf::trim_front(shadow, " ({[\"\'");
            if (shadow.starts_with('#')) // hex: #rrggbbaa
            {
                shadow.remove_prefix(1);
                if (shadow.size() >= 8) // hex: #rrggbbaa
                {
                    result.chan.r = (tobyte(shadow[0]) << 4) + tobyte(shadow[1]);
                    result.chan.g = (tobyte(shadow[2]) << 4) + tobyte(shadow[3]);
                    result.chan.b = (tobyte(shadow[4]) << 4) + tobyte(shadow[5]);
                    result.chan.a = (tobyte(shadow[6]) << 4) + tobyte(shadow[7]);
                }
                else if (shadow.size() >= 6) // hex: #rrggbb
                {
                    result.chan.r = (tobyte(shadow[0]) << 4) + tobyte(shadow[1]);
                    result.chan.g = (tobyte(shadow[2]) << 4) + tobyte(shadow[3]);
                    result.chan.b = (tobyte(shadow[4]) << 4) + tobyte(shadow[5]);
                    result.chan.a = 0xff;
                }
                else log(" xml: unknown hex rgba format: { ", value, " }, expected #rrggbbaa or #rrggbb rgba hex value");
            }
            else if (shadow.starts_with("0x")) // hex: 0xaabbggrr
            {
                shadow.remove_prefix(2);
                if (shadow.size() >= 8) // hex: 0xaabbggrr
                {
                    result.chan.a = (tobyte(shadow[0]) << 4) + tobyte(shadow[1]);
                    result.chan.b = (tobyte(shadow[2]) << 4) + tobyte(shadow[3]);
                    result.chan.g = (tobyte(shadow[4]) << 4) + tobyte(shadow[5]);
                    result.chan.r = (tobyte(shadow[6]) << 4) + tobyte(shadow[7]);
                }
                else if (shadow.size() >= 6) // hex: 0xbbggrr
                {
                    result.chan.b = (tobyte(shadow[0]) << 4) + tobyte(shadow[1]);
                    result.chan.g = (tobyte(shadow[2]) << 4) + tobyte(shadow[3]);
                    result.chan.r = (tobyte(shadow[4]) << 4) + tobyte(shadow[5]);
                    result.chan.a = 0xff;
                }
                else log(" xml: unknown hex rgba format: { ", value, " }, expected 0xaabbggrr or 0xbbggrr rgba hex value");
            }
            else if (utf::check_any(shadow, ",;/")) // dec: 000,000,000,000
            {
                if (auto r = utf::to_int(shadow))
                {
                    result.chan.r = r.value();
                    utf::trim_front(shadow, ",./:;");
                    if (auto g = utf::to_int(shadow))
                    {
                        result.chan.g = g.value();
                        utf::trim_front(shadow, ",./:;");
                        if (auto b = utf::to_int(shadow))
                        {
                            result.chan.b = b.value();
                            utf::trim_front(shadow, ",./:;");
                            if (auto a = utf::to_int(shadow)) result.chan.a = a.value();
                            else                              result.chan.a = 0xff;
                            return result;
                        }
                    }
                }
                log(" xml: unknown rgba format: { ", value, " }, expected 000,000,000,000 decimal rgba value");
            }
            else // Single ANSI color value
            {
                if (auto c = utf::to_int(shadow); c.value() >=0 && c.value() <=255)
                {
                    result = rgba::color256[c.value()];
                }
                else log(" xml: unknown ANSI 256-color value format: { ", value, " }, expected 0-255 decimal value");
            }
        }
        return result;
    }

    using frag = netxs::sptr<text>;
    using suit = std::list<frag>;

    class element
    {
        using sptr = netxs::sptr<class element>;
        using wptr = netxs::wptr<class element>;
        using byid = std::unordered_map<text, sptr>;

    public:
        template<class ...Args>
        static auto create(Args&&... args)
        {
            return std::make_shared<element>(std::forward<Args>(args)...);
        }

        class vect
            : public std::vector<sptr>
        {
        public:
            template<class ...Args>
            auto& add(Args&&... args)
            {
                return emplace_back(element::create(std::forward<Args>(args)...));
            }
            auto& add(sptr item)
            {
                return emplace_back(item);
            }
        };

        using subs = std::unordered_map<text, vect>;

        static auto& map()
        {
            static byid map;
            return map;
        }
        template<class T>
        static auto map(T&& key, sptr item)
        {
            map().emplace(std::forward<T>(key), item);
        }

        frag tag_ptr;
        suit val_ptr_list;
        subs sub;
        wptr def;
        bool is_template{};

        static constexpr auto spaces = " \n\r\t";
        static constexpr auto rawtext_delims = " \t\n\r/><";
        static constexpr auto token_delims = " \t\n\r=*/><";

        enum type
        {
            eof,           // end of file
            token,         // name
            raw_text,      //         ex: raw text
            quoted_text,   // '"'     ex: " text "
            begin_tag,     // '<'     ex: <name ...
            close_tag,     // '</'    ex: ... </name>
            comment_begin, // '<!--'  ex: ... <!-- ...
            comment_close, // '-->'   ex: ... -->
            close_inline,  // '>'     ex: ... >
            empty_tag,     // '/>'    ex: ... />
            equal,         // '='     ex: name=value
            defaults,      // '*'     ex: name*
        };

        static constexpr view view_comment_begin = "<!--";
        static constexpr view view_comment_close = "-->" ;
        static constexpr view view_close_tag     = "</"  ;
        static constexpr view view_begin_tag     = "<"   ;
        static constexpr view view_empty_tag     = "/>"  ;
        static constexpr view view_close_inline  = ">"   ;
        static constexpr view view_quoted_text   = "\""  ;
        static constexpr view view_equal         = "="   ;
        static constexpr view view_defaults      = "*"   ;

        auto peek(view temp, type& what, type& last)
        {
            last = what;
                 if (temp.empty())                         what = type::eof;
            else if (temp.starts_with(view_comment_begin)) what = type::comment_begin;
            else if (temp.starts_with(view_close_tag    )) what = type::close_tag;
            else if (temp.starts_with(view_begin_tag    )) what = type::begin_tag;
            else if (temp.starts_with(view_empty_tag    )) what = type::empty_tag;
            else if (temp.starts_with(view_close_inline )) what = type::close_inline;
            else if (temp.starts_with(view_quoted_text  )) what = type::quoted_text;
            else if (temp.starts_with(view_equal        )) what = type::equal;
            else if (temp.starts_with(view_defaults     )
                  && last == type::token)        what = type::defaults;
            else if (last == type::close_tag
                  || last == type::begin_tag
                  || last == type::token
                  || last == type::defaults
                  || last == type::raw_text
                  || last == type::quoted_text)  what = type::token;
            else                                 what = type::raw_text;
        }
        void skip(view& data, type what)
        {
            switch (what)
            {
                case type::comment_begin: data.remove_prefix(view_comment_begin.size()); break;
                case type::close_tag:     data.remove_prefix(view_close_tag    .size()); break;
                case type::begin_tag:     data.remove_prefix(view_begin_tag    .size()); break;
                case type::empty_tag:     data.remove_prefix(view_empty_tag    .size()); break;
                case type::close_inline:  data.remove_prefix(view_close_inline .size()); break;
                case type::quoted_text:   data.remove_prefix(view_quoted_text  .size()); break;
                case type::equal:         data.remove_prefix(view_equal        .size()); break;
                case type::defaults:      data.remove_prefix(view_defaults     .size()); break;
                default: break;
            };
        }
        auto take_token(view& data)
        {
            auto item = utf::get_tail(data, token_delims);
            utf::to_low(item);
            return std::make_shared<text>(std::move(item));
        }
        auto take_value(view& data)
        {
            auto crop = text{};
            if (data.size())
            {
                auto delim = data.front();
                if (delim != '\'' && delim != '\"') crop = utf::get_tail(data, rawtext_delims);
                else                                crop = utf::get_quote(data, view(&delim, 1));
            }
            return std::make_shared<text>(xml::unescape(crop));
        }
        auto fail(type what, type last, view data)
        {
            auto str = [&](type what)
            {
                switch (what)
                {
                    case type::eof:           return view{ "{EOF}" }     ;
                    case type::token:         return view{ "{token}" }   ;
                    case type::raw_text:      return view{ "{raw text}" };
                    case type::quoted_text:   return view_quoted_text    ;
                    case type::begin_tag:     return view_begin_tag      ;
                    case type::close_tag:     return view_close_tag      ;
                    case type::comment_begin: return view_comment_begin  ;
                    case type::comment_close: return view_comment_close  ;
                    case type::close_inline:  return view_close_inline   ;
                    case type::empty_tag:     return view_empty_tag      ;
                    case type::equal:         return view_equal          ;
                    case type::defaults:      return view_defaults       ;
                    default:                  return view{ "{unknown}" } ;
                };
            };
            log(" xml: unexpected '", str(what), "' after '", str(last), "' near '...", xml::escape(data.substr(0, 80)), "...'");
        }
        auto take_pair(view& data, type& what, type& last)
        {
            tag_ptr = take_token(data);
            utf::trim_front(data, spaces);
            peek(data, what, last);
            if (what == type::defaults)
            {
                is_template = true;
                skip(data, what);
                utf::trim_front(data, spaces);
                peek(data, what, last);
            }
            if (what == type::equal)
            {
                skip(data, what);
                utf::trim_front(data, spaces);
                peek(data, what, last);
                if (what == type::quoted_text
                 || what == type::raw_text)
                {
                    val_ptr_list.push_back(take_value(data));
                    utf::trim_front(data, spaces);
                    peek(data, what, last);
                }
                else fail(what, last, data);
            }
        }
        auto take(view& data)
        {
            auto what = type::eof;
            auto last = type::eof;
            auto defs = std::unordered_map<text, wptr>{};
            auto push = [&](sptr& item)
            {
                auto& sub_tag = *(item->tag_ptr);
                if (item->is_template) defs[sub_tag] = item;
                else
                {
                    auto iter = defs.find(sub_tag);
                    if (iter != defs.end())
                    {
                        item->def = iter->second;
                    }
                }
                sub[sub_tag].add(item);        
            };

            utf::trim_front(data, spaces);
            peek(data, what, last);
            if (what == type::begin_tag)
            {
                skip(data, what);
                utf::trim_front(data, spaces);
                peek(data, what, last);
                if (what == type::token)
                {
                    take_pair(data, what, last);
                    if (what == type::token)
                    {
                        do // Proceed inlined subs
                        {
                            auto item = element::create();
                            item->take_pair(data, what, last);
                            push(item);
                        }
                        while (what == type::token);
                    }

                    if (what == type::empty_tag) // />
                    {
                        skip(data, what);
                    }
                    else if (what == type::close_inline) // proceed nested subs
                    {
                        skip(data, what);
                        auto temp = data;
                        utf::trim_front(temp, spaces);
                        peek(temp, what, last);
                        do
                        {
                            if (what == type::quoted_text)
                            {
                                data = temp;
                                val_ptr_list.push_back(take_value(data));
                                utf::trim_front(data, spaces);
                                temp = data;
                            }
                            else if (what == type::raw_text)
                            {
                                auto size = data.find('<');
                                val_ptr_list.push_back(std::make_shared<text>(data.substr(0, size)));
                                if (size == view::npos)
                                {
                                    data = {};
                                    what = type::eof;
                                    fail(type::eof, what, data);
                                    return;
                                }
                                data.remove_prefix(size);
                                temp = data;
                            }
                            else if (what == type::begin_tag)
                            {
                                data = temp;
                                auto item = element::create();
                                item->take(data);
                                push(item);
                                temp = data;
                                utf::trim_front(temp, spaces);
                            }
                            else if (what == type::comment_begin) // <!--
                            {
                                auto size = data.find(view_comment_close);
                                if (size == view::npos)
                                {
                                    data = {};
                                    what = type::eof;
                                    fail(type::eof, what, data);
                                    return;
                                }
                                data.remove_prefix(size + view_comment_close.size());
                                temp = data;
                                utf::trim_front(temp, spaces);
                            }
                            else if (what != type::close_tag
                                  && what != type::eof)
                            {
                                fail(what, last, data);
                                skip(temp, what);
                                data = temp;
                            }
                            peek(temp, what, last);
                        }
                        while (what != type::close_tag
                            && what != type::eof);

                        if (what == type::close_tag) // </token>
                        {
                            skip(temp, what);
                            utf::trim_front(temp, spaces);
                            peek(temp, what, last);
                            if (what == type::token)
                            {
                                auto token_ptr = take_token(temp);
                                if (*token_ptr == *tag_ptr)
                                {
                                    data = temp;
                                    auto tail = data.find('>');
                                    if (tail != view::npos) data.remove_prefix(tail + 1);
                                    else                    data = {};
                                }
                                else log(" xml: unexpected closing tag name '", *token_ptr, "', expected: '", *tag_ptr, "' near '...", xml::escape(data.substr(0, 80)), "...'");
                            }
                            else fail(what, last, data);
                        }
                        else fail(what, last, data);
                    }
                    else fail(what, last, data);
                }
                else fail(what, last, data);
            }
            else fail(what, last, data);
        }
        text show(sz_t indent = 0)
        {
            auto data = text{};
            data += text(indent, ' ') + '<' + *tag_ptr;
            if (is_template) data += view_defaults;
            if (val_ptr_list.size())
            {
                auto val = text{};
                for (auto& val_ptr : val_ptr_list)
                {
                    val += *val_ptr;
                }
                if (val.size())
                {
                    if (utf::check_any(val, rawtext_delims)) data += "=\"" + xml::escape(val) + "\"";
                    else                                     data += '='   + xml::escape(val) + ' ';
                }
            }
            if (sub.empty()) data += "/>\n";
            else
            {
                data += ">\n";
                for (auto& [sub_tag, sub_list] : sub)
                {
                    for (auto& item : sub_list)
                    {
                        data += item->show(indent + 4);
                    }
                }
                data += text(indent, ' ') + "</" + *tag_ptr + ">\n";
            }
            return data;
        }
    };

    class document
    {
    public:
        suit    base;
        element root;

        template<class T>
        document(T&& data)
        {
            auto shadow = view{ std::forward<T>(data) };
            root.take(shadow);
        }
    };
}

#endif // NETXS_XML_HPP