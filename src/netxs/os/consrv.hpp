// Copyright (c) NetXS Group.
// Licensed under the MIT license.

#ifndef NETXS_CONSRV_HPP
#define NETXS_CONSRV_HPP

#if not defined(_DEBUG)
    #define log(...)
#endif

struct consrv
{
    struct cook
    {
        wide wstr{};
        text ustr{};
        view rest{};
        ui32 ctrl{};

        auto save(bool isutf16)
        {
            wstr.clear();
            utf::to_utf(ustr, wstr);
            if (isutf16) rest = { reinterpret_cast<char*>(wstr.data()), wstr.size() * 2 };
            else         rest = ustr;
        }
        auto drop()
        {
            ustr.clear();
            wstr.clear();
            rest = ustr;
        }
    };

    struct memo
    {
        size_t                           seek{};
        std::list<std::pair<si32, rich>> undo{};
        std::list<std::pair<si32, rich>> redo{};
        std::vector<rich>                data{};
        rich                             idle{};

        auto save(para& line)
        {
            auto& data = line.content();
            if (undo.empty() || !data.same(undo.back().second))
            {
                undo.emplace_back(line.caret, data);
                redo.clear();
            }
        }
        auto swap(para& line, bool back)
        {
            if (back) std::swap(undo, redo);
            if (undo.size())
            {
                auto& data = line.content();
                while (undo.size() && data.same(undo.back().second)) undo.pop_back();
                if (undo.size())
                {
                    redo.emplace_back(line.caret, data);
                    line.content(undo.back().second);
                    line.caret = undo.back().first;
                    undo.pop_back();
                }
            }
            if (back) std::swap(undo, redo);
        }
        auto done(para& line)
        {
            auto& new_data = line.content();
            auto trimmed = utf::trim(new_data.utf8(), utf::spaces);

            if (trimmed.size() && (data.empty() || data.back() != new_data))
            {
                data.push_back(new_data);
                seek = data.size() - 1;
            }
            undo.clear();
            redo.clear();
        }
        auto& fallback() const
        {
            if (data.size()) return data[seek];
            else             return idle;
        }
        auto roll()
        {
            if (data.size())
            {
                if (seek == 0) seek = data.size() - 1;
                else           seek--;
            }
        }
        auto find(para& line)
        {
            if (data.empty()) return faux;
            auto stop = seek;
            auto size = line.caret;
            while (!line.substr(0, size).same(data[seek].substr(0, size))
                 || line.content()      .same(data[seek]))
            {
                roll();
                if (seek == stop) return faux;
            }
            save(line);
            line.content() = data[seek];
            line.caret = size;
            line.caret_check();
            return true;
        }
        auto deal(para& line, size_t i)
        {
            if (i < data.size())
            {
                save(line);
                line.content(data[seek = i]);
            }
        }
        auto prev(para& line) { if (seek > 0 && line.content().same(data[seek])) roll();
                                deal(line, seek           ); }
        auto pgup(para& line) { deal(line, 0              ); }
        auto next(para& line) { deal(line, seek + 1       ); }
        auto pgdn(para& line) { deal(line, data.size() - 1); }
    };

    struct clnt
    {
        struct hndl
        {
            enum type
            {
                unused,
                events,
                scroll,
            };

            clnt& boss;
            ui32& mode;
            type  kind;
            void* link;
            text  rest;

            hndl(clnt& boss, ui32& mode, type kind, void* link)
                : boss{ boss },
                  mode{ mode },
                  kind{ kind },
                  link{ link }
            { }
            friend auto& operator << (std::ostream& s, hndl const& h)
            {
                if (h.kind == hndl::type::events)
                {
                    s << "events 0";
                    if (h.mode & nt::console::inmode::echo      ) s << " | ECHO";
                    if (h.mode & nt::console::inmode::insert    ) s << " | INSERT";
                    if (h.mode & nt::console::inmode::cooked    ) s << " | COOKED_READ";
                    if (h.mode & nt::console::inmode::mouse     ) s << " | MOUSE_INPUT";
                    if (h.mode & nt::console::inmode::preprocess) s << " | PROCESSED_INPUT";
                    if (h.mode & nt::console::inmode::quickedit ) s << " | QUICK_EDIT";
                    if (h.mode & nt::console::inmode::winsize   ) s << " | WINSIZE";
                    if (h.mode & nt::console::inmode::vt        ) s << " | VIRTUAL_TERMINAL_INPUT";
                }
                else
                {
                    s << "scroll 0";
                    if (h.mode & nt::console::outmode::preprocess ) s << " | PROCESSED_OUTPUT";
                    if (h.mode & nt::console::outmode::wrap_at_eol) s << " | WRAP_AT_EOL";
                    if (h.mode & nt::console::outmode::vt         ) s << " | VIRTUAL_TERMINAL_PROCESSING";
                    if (h.mode & nt::console::outmode::no_auto_cr ) s << " | NO_AUTO_CR";
                    if (h.mode & nt::console::outmode::lvb_grid   ) s << " | LVB_GRID_WORLDWIDE";
                }
                return s;
            }
        };

        struct info
        {
            ui32 iconid{};
            ui32 hotkey{};
            ui32 config{};
            ui16 colors{};
            ui16 format{};
            twod scroll{};
            rect window{};
            bool cliapp{};
            bool expose{};
            text header{};
            text curexe{};
            text curdir{};
        };

        using list = std::list<hndl>;

        list tokens; // clnt: Taked handles.
        ui32 procid; // clnt: Process id.
        ui32 thread; // clnt: Process thread id.
        ui32 pgroup; // clnt: Process group id.
        info detail; // clnt: Process details.
        memo inputs; // clnt: Input history.
    };
    
    using hndl = clnt::hndl;

    struct base
    {
        ui64  taskid;
        clnt* client;
        hndl* target;
        ui32  fxtype;
        ui32  packsz;
        ui32  echosz;
        ui32  _pad_1;
    };

    struct task : base
    {
        ui32 callfx;
        ui32 arglen;
        byte argbuf[96];
    };

    template<class Payload>
    struct wrap
    {
        template<class T>
        static auto& cast(T& buffer)
        {
            static_assert(sizeof(T) >= sizeof(Payload));
            return *reinterpret_cast<Payload*>(&buffer);
        }
        template<class T>
        static auto& cast(T* buffer)
        {
            static_assert(sizeof(T) >= sizeof(Payload));
            return *reinterpret_cast<Payload*>(buffer);
        }
        static auto& cast(text& buffer)
        {
            buffer.resize(sizeof(Payload));
            return *reinterpret_cast<Payload*>(buffer.data());
        }
    };

    template<class Payload>
    struct drvpacket : base, wrap<Payload>
    {
        ui32 callfx;
        ui32 arglen;
    };

    struct cdrw
    {
        using cptr = void const*;
        using stat = NTSTATUS;

        struct order
        {
            ui64 taskid;
            cptr buffer;
            ui32 length;
            ui32 offset;
        };

        ui64 taskid;
        stat status;
        ui32 _pad_1;
        ui64 report;
        cptr buffer;
        ui32 length;
        ui32 offset;

        //auto readoffset() const { return static_cast<ui32>(length ? length + sizeof(ui32) * 2 /*sizeof(drvpacket payload)*/ : 0); }
        //auto sendoffset() const { return length; }
        auto readoffset() const { return _pad_1; }
        auto sendoffset() const { return length; }
        template<class T>
        auto recv_data(fd_t condrv, T&& buffer)
        {
            auto request = order
            {
                .taskid = taskid,
                .buffer = buffer.data(),
                .length = static_cast<decltype(order::length)>(buffer.size() * sizeof(buffer.front())),
                .offset = readoffset(),
            };
            auto rc = nt::ioctl(nt::console::op::read_input, condrv, request);
            if (rc != ERROR_SUCCESS)
            {
                log("\tabort: read_input (condrv, request) rc=", rc);
                status = nt::status::unsuccessful;
                return faux;
            }
            return true;
        }
        template<bool Complete = faux, class T>
        auto send_data(fd_t condrv, T&& buffer, bool inc_nul_terminator = faux)
        {
            auto result = order
            {
                .taskid = taskid,
                .buffer = buffer.data(),
                .length = static_cast<decltype(order::length)>((buffer.size() + inc_nul_terminator) * sizeof(buffer.front())),
                .offset = sendoffset(),
            };
            auto rc = nt::ioctl(nt::console::op::write_output, condrv, result);
            if (rc != ERROR_SUCCESS)
            {
                log("\tnt::console::op::write_output returns unexpected result ", utf::to_hex(rc));
                status = nt::status::unsuccessful;
                result.length = 0;
            }
            report = result.length;
            if constexpr (Complete) //Note: Be sure that packet.reply.bytes or count is set.
            {
                auto rc = nt::ioctl(nt::console::op::complete_io, condrv, *this);
            }
        }
        auto interrupt(fd_t condrv)
        {
            status = nt::status::invalid_handle;
            auto rc = nt::ioctl(nt::console::op::complete_io, condrv, *this);
            log("\tpending operation aborted");
        }
    };

    struct event_list
    {
        using jobs = netxs::jobs<std::tuple<cdrw, decltype(base{}.target), bool>>;
        using fire = netxs::os::fire;
        using lock = std::recursive_mutex;
        using sync = std::condition_variable_any;
        using vect = std::vector<INPUT_RECORD>;

        static constexpr ui32 shift_pressed = SHIFT_PRESSED;
        static constexpr ui32 alt___pressed = LEFT_ALT_PRESSED  | RIGHT_ALT_PRESSED;
        static constexpr ui32 ctrl__pressed = LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED;
        static constexpr ui32 altgr_pressed = alt___pressed | ctrl__pressed;

        struct nttask
        {
            ui32 procid;
            ui32 _pad_1;
            ui64 _pad_2;
            si32 action;
            ui32 _pad_3;
        };

        consrv& server; // events_t: Console server reference.
        vect    buffer; // events_t: Input event list.
        vect    recbuf; // events_t: Temporary buffer for copying event records.
        sync    signal; // events_t: Input event append signal.
        lock    locker; // events_t: Input event buffer mutex.
        cook    cooked; // events_t: Cooked input string.
        jobs    worker; // events_t: Background task executer.
        bool    closed; // events_t: Console server was shutdown.
        fire    ondata; // events_t: Signal on input buffer data.
        wide    wcpair; // events_t: Surrogate pair buffer.

        event_list(consrv& serv)
            :  server{ serv },
               closed{ faux },
               ondata{ true }
        { }

        auto count()
        {
            auto lock = std::lock_guard{ locker };
            return static_cast<ui32>(buffer.size());
        }
        void abort(hndl* handle_ptr)
        {
            auto lock = std::lock_guard{ locker };
            worker.cancel([&](auto& token)
            {
                auto& answer = std::get<0>(token);
                auto& target = std::get<1>(token);
                auto& cancel = std::get<2>(token);
                if (target == handle_ptr)
                {
                    cancel = true;
                    answer.interrupt(server.condrv);
                }
            });
            signal.notify_all();
        }
        void alert(si32 what, ui32 pgroup = 0)
        {
            log(server.prompt, what == CTRL_C_EVENT     ? "Ctrl+C"
                             : what == CTRL_BREAK_EVENT ? "Ctrl+Break"
                             : what == CTRL_CLOSE_EVENT ? "CtrlClose"
                                                        : "unknown", " event");
            for (auto& client : server.joined)
            {
                if (!pgroup || pgroup == client.pgroup)
                {
                    auto task = nttask{ .procid = client.procid, .action = what };
                    auto stat = os::nt::UserConsoleControl((ui32)sizeof("Ending"), &task, (ui32)sizeof(task));
                    log("\tclient proc id ", client.procid,  ", control status 0x", utf::to_hex(stat));
                }
            }
        }
        void stop()
        {
            auto lock = std::lock_guard{ locker };
            alert(CTRL_CLOSE_EVENT);
            closed = true;
            signal.notify_all();
        }
        void clear()
        {
            auto lock = std::lock_guard{ locker };
            ondata.flush();
            buffer.clear();
        }
        auto generate(wchr c, ui32 s = 0, ui16 v = 0)
        {
            buffer.emplace_back(INPUT_RECORD
            {
                .EventType = KEY_EVENT,
                .Event =
                {
                    .KeyEvent =
                    {
                        .bKeyDown               = 1,
                        .wRepeatCount           = 1,
                        .wVirtualKeyCode        = v,
                        .wVirtualScanCode       = 0,
                        .uChar = { .UnicodeChar = c },
                        .dwControlKeyState      = s,
                    }
                }
            });
            return true;
        }
        auto generate(wchr c1, wchr c2)
        {
            buffer.reserve(buffer.size() + 2);
            generate(c1);
            generate(c2);
            return true;
        }
        auto generate(wiew wstr, ui32 s = 0)
        {
            buffer.reserve(wstr.size());
            for (auto c : wstr) generate(c, s);
            return true;
        }
        auto generate(view ustr)
        {
            server.toWIDE.clear();
            utf::to_utf(ustr, server.toWIDE);
            return generate(server.toWIDE);
        }
        auto write(view ustr)
        {
            auto lock = std::lock_guard{ locker };
            generate(ustr);
            if (!buffer.empty())
            {
                ondata.reset();
                signal.notify_one();
            }
        }
        void mouse(input::hids& gear, bool moved, bool wheel)
        {
            auto state = os::ms_kbstate(gear.ctlstate);
            auto bttns = ui32{};
            if (gear.buttons[input::sysmouse::left  ].pressed) bttns |= FROM_LEFT_1ST_BUTTON_PRESSED;
            if (gear.buttons[input::sysmouse::right ].pressed) bttns |= RIGHTMOST_BUTTON_PRESSED;
            if (gear.buttons[input::sysmouse::middle].pressed) bttns |= FROM_LEFT_2ND_BUTTON_PRESSED;
            if (gear.buttons[input::sysmouse::wheel ].pressed) bttns |= FROM_LEFT_3RD_BUTTON_PRESSED;
            if (gear.buttons[input::sysmouse::win   ].pressed) bttns |= FROM_LEFT_4TH_BUTTON_PRESSED;
            auto flags = ui32{};
            if (moved) flags |= MOUSE_MOVED; // DOUBLE_CLICK   not used.
            if (wheel)                       // MOUSE_HWHEELED not used.
            {
                flags |= MOUSE_WHEELED;
                bttns |= gear.whldt << 16;
            }

            auto lock = std::lock_guard{ locker };
            buffer.emplace_back(INPUT_RECORD
            {
                .EventType = MOUSE_EVENT,
                .Event =
                {
                    .MouseEvent =
                    {
                        .dwMousePosition =
                        {
                            .X = (si16)std::min<si32>(gear.coord.x + server.uiterm.origin.x, std::numeric_limits<si16>::max()),
                            .Y = (si16)std::min<si32>(gear.coord.y + server.uiterm.origin.y, std::numeric_limits<si16>::max()),
                        },
                        .dwButtonState     = bttns,
                        .dwControlKeyState = state,
                        .dwEventFlags      = flags,
                    }
                }
            });
            ondata.reset();
            signal.notify_one();
        }
        void winsz(twod const& winsize)
        {
            auto lock = std::lock_guard{ locker };
            buffer.emplace_back(INPUT_RECORD // Ignore ENABLE_WINDOW_INPUT - we only signal a viewport change.
            {
                .EventType = WINDOW_BUFFER_SIZE_EVENT,
                .Event =
                {
                    .WindowBufferSizeEvent =
                    {
                        .dwSize = 
                        {
                            .X = (si16)std::min<si32>(winsize.x, std::numeric_limits<si16>::max()),
                            .Y = (si16)std::min<si32>(winsize.y, std::numeric_limits<si16>::max()),
                        }
                    }
                }
            });
            ondata.reset();
            signal.notify_one();
        }
        template<char C>
        static auto takevkey()
        {
            struct vkey { si16 key, vkey; ui32 base; };
            static auto x = ::VkKeyScanW(C);
            static auto k = vkey{ x, x & 0xff, x & 0xff |((x & 0x0100 ? shift_pressed : 0)
                                                        | (x & 0x0200 ? ctrl__pressed : 0)
                                                        | (x & 0x0400 ? alt___pressed : 0)) << 8 };
            return k;
        }
        template<char C>
        static auto truechar(ui16 v, ui32 s)
        {
            static auto x = takevkey<C>();
            static auto need_shift = !!(x.key & 0x100);
            static auto need__ctrl = !!(x.key & 0x200);
            static auto need___alt = !!(x.key & 0x400);
            return v == x.vkey && need_shift == !!(s & shift_pressed)
                               && need__ctrl == !!(s & ctrl__pressed)
                               && need___alt == !!(s & alt___pressed);
        }
        static auto mapvkey(wchr& c, ui16 v)
        {
            return v != VK_CONTROL && v != VK_SHIFT && (c = ::MapVirtualKeyW(v, MAPVK_VK_TO_CHAR) & 0xffff);
        }
        auto vtencode(input::hids& gear, bool decckm)
        {
            static auto truenull = takevkey<'\0'>().vkey;
            static auto alonekey = std::unordered_map<ui16, wide>
            {
                { VK_BACK,   L"\x7f"     },
                { VK_TAB,    L"\x09"     },
                { VK_PAUSE,  L"\x1a"     },
                { VK_ESCAPE, L"\033"     },
                { VK_PRIOR,  L"\033[5~"  },
                { VK_NEXT,   L"\033[6~"  },
                { VK_END,    L"\033[F"   },
                { VK_HOME,   L"\033[H"   },
                { VK_LEFT,   L"\033[D"   },
                { VK_UP,     L"\033[A"   },
                { VK_RIGHT,  L"\033[C"   },
                { VK_DOWN,   L"\033[B"   },
                { VK_INSERT, L"\033[2~"  },
                { VK_DELETE, L"\033[3~"  },
                { VK_F1,     L"\033OP"   },
                { VK_F2,     L"\033OQ"   },
                { VK_F3,     L"\033OR"   },
                { VK_F4,     L"\033OS"   },
                { VK_F5,     L"\033[15~" },
                { VK_F6,     L"\033[17~" },
                { VK_F7,     L"\033[18~" },
                { VK_F8,     L"\033[19~" },
                { VK_F9,     L"\033[20~" },
                { VK_F10,    L"\033[21~" },
                { VK_F11,    L"\033[23~" },
                { VK_F12,    L"\033[24~" },
            };
            static auto shiftkey = std::unordered_map<ui16, wide>
            {
                { VK_PRIOR,  L"\033[5; ~"  },
                { VK_NEXT,   L"\033[6; ~"  },
                { VK_END,    L"\033[1; F"  },
                { VK_HOME,   L"\033[1; H"  },
                { VK_LEFT,   L"\033[1; D"  },
                { VK_UP,     L"\033[1; A"  },
                { VK_RIGHT,  L"\033[1; C"  },
                { VK_DOWN,   L"\033[1; B"  },
                { VK_INSERT, L"\033[2; ~"  },
                { VK_DELETE, L"\033[3; ~"  },
                { VK_F1,     L"\033[1; P"  },
                { VK_F2,     L"\033[1; Q"  },
                { VK_F3,     L"\033[1; R"  },
                { VK_F4,     L"\033[1; S"  },
                { VK_F5,     L"\033[15; ~" },
                { VK_F6,     L"\033[17; ~" },
                { VK_F7,     L"\033[18; ~" },
                { VK_F8,     L"\033[19; ~" },
                { VK_F9,     L"\033[20; ~" },
                { VK_F10,    L"\033[21; ~" },
                { VK_F11,    L"\033[23; ~" },
                { VK_F12,    L"\033[24; ~" },
            };
            static auto specials = std::unordered_map<ui32, wide>
            {
                { VK_BACK              | ctrl__pressed << 8, { L"\x08"      }},
                { VK_BACK              | alt___pressed << 8, { L"\033\x7f"  }},
                { VK_BACK              | altgr_pressed << 8, { L"\033\x08"  }},
                { VK_TAB               | ctrl__pressed << 8, { L"\t"        }},
                { VK_TAB               | shift_pressed << 8, { L"\033[Z"    }},
                { VK_TAB               | alt___pressed << 8, { L"\033[1;3I" }},
                { VK_ESCAPE            | alt___pressed << 8, { L"\033\033"  }},
                { '1'                  | ctrl__pressed << 8, { L"1"         }},
                { '3'                  | ctrl__pressed << 8, { L"\x1b"      }},
                { '4'                  | ctrl__pressed << 8, { L"\x1c"      }},
                { '5'                  | ctrl__pressed << 8, { L"\x1d"      }},
                { '6'                  | ctrl__pressed << 8, { L"\x1e"      }},
                { '7'                  | ctrl__pressed << 8, { L"\x1f"      }},
                { '8'                  | ctrl__pressed << 8, { L"\x7f"      }},
                { '9'                  | ctrl__pressed << 8, { L"9"         }},
                { VK_DIVIDE            | ctrl__pressed << 8, { L"\x1f"      }},
                { takevkey<'?'>().base | altgr_pressed << 8, { L"\033\x7f"  }},
                { takevkey<'?'>().base | ctrl__pressed << 8, { L"\x7f"      }},
                { takevkey<'/'>().base | altgr_pressed << 8, { L"\033\x1f"  }},
                { takevkey<'/'>().base | ctrl__pressed << 8, { L"\x1f"      }},
            };

            if (server.inpmod & nt::console::inmode::vt && gear.pressed)
            {
                auto& s = gear.winctrl;
                auto& v = gear.virtcod;
                auto& c = gear.winchar;

                if (s & LEFT_CTRL_PRESSED && s & RIGHT_ALT_PRESSED) // This combination is already translated.
                {
                    s &= ~(LEFT_CTRL_PRESSED | RIGHT_ALT_PRESSED);
                }

                auto shift = s & shift_pressed ? shift_pressed : 0;
                auto alt   = s & alt___pressed ? alt___pressed : 0; 
                auto ctrl  = s & ctrl__pressed ? ctrl__pressed : 0;
                if (shift || alt || ctrl)
                {
                    if (ctrl && alt) // c == 0 for ctrl+alt+key combinationsons on windows.
                    {
                        auto a = c ? c : v; // Chars and vkeys for ' '(0x20),'A'-'Z'(0x41-5a) are the same on windows.
                             if (a == 0x20 || (a >= 0x41 && a <= 0x5a)) return generate('\033', (wchr)( a  & 0b00011111)); // Alt causes to prepend '\033'. Ctrl trims by 0b00011111.
                        else if (c == 0x00 && v == truenull           ) return generate('\033', (wchr)('@' & 0b00011111)); // Map ctrl+alt+@ to ^[^@;
                    }

                    if (auto iter = shiftkey.find(v); iter != shiftkey.end())
                    {
                        auto& mods = *++(iter->second.rbegin());
                        mods = '1';
                        if (shift) mods += 1;
                        if (alt  ) mods += 2;
                        if (ctrl ) mods += 4;
                        return generate(iter->second);
                    }
                    else if (auto iter = specials.find(v | (shift | alt | ctrl) << 8); iter != specials.end())
                    {
                        return generate(iter->second);
                    }
                    else if (!ctrl &&  alt && c) return generate('\033', c);
                    else if ( ctrl && !alt)
                    {
                             if (c == 0x20 || (c == 0x00 && v == truenull)) return generate('@' & 0b00011111, s, truenull); // Detect ctrl+@ and ctrl+space.
                        else if (c == 0x00 && mapvkey(c, v))                return generate( c  & 0b00011111); // Emulate ctrl+key mapping to C0 if current kb layout does not contain it.
                    }
                }

                if (auto iter = alonekey.find(v); iter != alonekey.end())
                {
                    if (v >= VK_END && v <= VK_DOWN) iter->second[1] = decckm ? 'O' : '[';
                    return generate(iter->second);
                }
                else if (c) return generate(c); //todo check surrogate pairs
            }
            return faux;
        }
        void keybd(input::hids& gear, bool decckm)
        {
            auto lock = std::lock_guard{ locker };
            if (!vtencode(gear, decckm))
            {
                buffer.emplace_back(INPUT_RECORD
                {
                    .EventType = KEY_EVENT,
                    .Event =
                    {
                        .KeyEvent =
                        {
                            .bKeyDown               = gear.pressed,
                            .wRepeatCount           = gear.imitate,
                            .wVirtualKeyCode        = gear.virtcod,
                            .wVirtualScanCode       = gear.scancod,
                            .uChar = { .UnicodeChar = gear.winchar },
                            .dwControlKeyState      = gear.winctrl,
                        }
                    }
                });
            }

            if (gear.keybd::winchar == ansi::C0_ETX)
            {
                if (gear.keybd::scancod == ansi::ctrl_break)
                {
                    alert(CTRL_BREAK_EVENT);
                    buffer.pop_back();
                }
                else
                {
                    if (server.inpmod & nt::console::inmode::preprocess)
                    {
                        if (gear.pressed) alert(CTRL_C_EVENT);
                        //todo revise
                        //buffer.pop_back();
                    }
                }
            }

            if (!buffer.empty())
            {
                ondata.reset();
                signal.notify_one();
            }
        }
        template<class L>
        auto readline(L& lock, bool& cancel, bool utf16, bool EOFon, ui32 stops, memo& hist)
        {
            auto mode = testy<bool>{ !!(server.inpmod & nt::console::inmode::insert) };
            auto buff = text{};
            auto line = para{ cooked.ustr };
            auto done = faux;
            auto crlf = 0;
            auto burn = [&]
            {
                if (buff.size())
                {
                    hist.save(line);
                    line.insert(buff, mode);
                    buff.clear();
                }
            };

            do
            {
                auto coor = line.caret;
                auto size = line.length();
                auto pops = 0_sz;
                for (auto& rec : buffer)
                {
                    if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown)
                    {
                        log(" ============================",
                            "\n cooked.ctrl ", utf::to_hex(cooked.ctrl),
                            "\n rec.Event.KeyEvent.uChar.UnicodeChar ", (int)rec.Event.KeyEvent.uChar.UnicodeChar,
                            "\n rec.Event.KeyEvent.wVirtualKeyCode   ", (int)rec.Event.KeyEvent.wVirtualKeyCode,
                            "\n rec.Event.KeyEvent.wVirtualScanCode  ", (int)rec.Event.KeyEvent.wVirtualScanCode,
                            "\n rec.Event.KeyEvent.Pressed           ", rec.Event.KeyEvent.bKeyDown ? "true" : "faux");
                    }

                    if (rec.EventType == KEY_EVENT
                     && rec.Event.KeyEvent.bKeyDown)
                    {
                        auto& v = rec.Event.KeyEvent.wVirtualKeyCode;
                        auto& c = rec.Event.KeyEvent.uChar.UnicodeChar;
                        auto& n = rec.Event.KeyEvent.wRepeatCount;
                        cooked.ctrl = rec.Event.KeyEvent.dwControlKeyState;
                        auto contrl = cooked.ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
                        switch (v)
                        {
                            case VK_CONTROL:
                            case VK_SHIFT:
                            case VK_MENU:/*Alt*/
                            case VK_PAUSE:
                            case VK_LWIN:
                            case VK_RWIN:
                            case VK_APPS:
                            case VK_NUMLOCK:
                            case VK_CAPITAL:
                            case VK_SCROLL:
                            case VK_CLEAR:/*NUMPAD 5*/
                            case VK_F2:  //todo menu
                            case VK_F4:  //todo menu
                            case VK_F7:  //todo menu
                            case VK_F9:  //todo menu
                            case VK_F10: //todo clear exes aliases
                            case VK_F11:
                            case VK_F12:
                                break;
                            case VK_INSERT: burn(); mode(!mode);                                                                   break;
                            case VK_F6:     burn(); hist.save(line);               line.insert(cell{}.c0_to_txt('Z' - '@'), mode); break;
                            case VK_ESCAPE: burn(); hist.save(line);               line.wipe();                                    break;
                            case VK_HOME:   burn(); hist.save(line);               line.move_to_home(contrl);                      break;
                            case VK_END:    burn(); hist.save(line);               line.move_to_end (contrl);                      break;
                            case VK_BACK:   burn(); hist.save(line); while (n-- && line.wipe_rev(contrl)) { }                      break;
                            case VK_DELETE: burn(); hist.save(line); while (n-- && line.wipe_fwd(contrl)) { }                      break;
                            case VK_LEFT:   burn();                  while (n-- && line.step_rev(contrl)) { }                      break;
                            case VK_F1:     contrl = faux;
                            case VK_RIGHT:  burn(); hist.save(line); while (n-- && line.step_fwd(contrl, hist.fallback())) { }     break;
                            case VK_F3:     burn(); hist.save(line); while (       line.step_fwd(faux,   hist.fallback())) { }     break;
                            case VK_F8:     burn();                  while (n-- && hist.find(line)) { };                           break;
                            case VK_PRIOR:  burn(); hist.pgup(line);                                                               break;
                            case VK_NEXT:   burn(); hist.pgdn(line);                                                               break;
                            case VK_F5:
                            case VK_UP:     burn(); hist.prev(line);                                                               break;
                            case VK_DOWN:   burn(); hist.next(line);                                                               break;
                            default:
                            {
                                n--;
                                if (c < ' ')
                                {
                                    auto cook = [&](auto c, auto crlf_value)
                                    {
                                        done = true;
                                        crlf = crlf_value;
                                        burn();
                                        cooked.ustr.clear();
                                        if (crlf_value)
                                        {
                                            line.lyric->utf8(cooked.ustr);
                                            cooked.ustr.push_back('\r');
                                            cooked.ustr.push_back('\n');
                                        }
                                        else
                                        {
                                            hist.save(line);
                                            line.move_to_end(true);
                                            line.lyric->utf8(cooked.ustr);
                                            cooked.ustr.push_back((char)c);
                                        }
                                        if (n == 0) pops++;
                                    };
                                         if (stops & 1 << c)                { cook(c, 0); hist.save(line);                                }
                                    else if (c == '\r'     )                { cook(c, 1); hist.done(line);                                }
                                    else if (c == 'Z' - '@')                {             hist.swap(line, faux);                          }
                                    else if (c == 'Y' - '@')                {             hist.swap(line, true);                          }
                                    else if (c == 'I' - '@' && v == VK_TAB) { burn();     hist.save(line); line.insert("        ", mode); }
                                    else if (c == 'C' - '@')
                                    {
                                        hist.save(line);
                                        cooked.ustr = "\n";
                                        done = true;
                                        crlf = 2;
                                        line.insert(cell{}.c0_to_txt(c), mode);
                                        if (n == 0) pops++;
                                    }
                                    else
                                    {
                                        burn();
                                        hist.save(line); 
                                        line.insert(cell{}.c0_to_txt(c), mode);
                                    }
                                }
                                else
                                {
                                    auto grow = utf::to_utf(c, wcpair, buff);
                                    if (grow && n)
                                    {
                                        auto temp = view{ buff.data() + grow, buff.size() - grow };
                                        while (n--)
                                        {
                                            buff += temp;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    if (!done) pops++;
                }

                if (pops)
                {
                    auto head = buffer.begin();
                    auto tail = head + pops;
                    buffer.erase(head, tail);
                    pops = {};
                }

                burn();

                if (server.inpmod & nt::console::inmode::echo)
                {
                    lock.unlock();
                    server.uiterm.update([&]
                    {
                        static auto zero = cell{ '\0' }.wdt(1);
                        auto& term = *server.uiterm.target;
                        auto& data = line.content();
                        data.crop(line.length() + 1, zero); // To avoid pendind cursor.
                        term.move(-coor);
                        term.data(data);
                        using erase = std::decay_t<decltype(server.uiterm)>::commands::erase;
                        term.el(erase::line::wraps);

                        if (done && crlf)
                        {
                            term.cr();
                            term.lf(crlf);
                        }
                        else term.move(line.caret - line.length());

                        if (mode.reset()) server.uiterm.cursor.toggle();
                        data.crop(line.length() - 1);
                    });
                    lock.lock();
                }
            }
            while (!done && ((void)signal.wait(lock, [&]{ return buffer.size() || closed || cancel; }), !closed && !cancel));

            if (EOFon)
            {
                static constexpr auto EOFkey = 'Z' - '@';
                auto EOFpos = cooked.ustr.find(EOFkey);
                if (EOFpos != text::npos) cooked.ustr.resize(EOFpos);
            }

            cooked.save(utf16);
            if (buffer.empty()) ondata.flush();

            server.inpmod = (server.inpmod & ~nt::console::inmode::insert) | (mode ? nt::console::inmode::insert : 0);
        }
        template<class L>
        auto readchar(L& lock, bool& cancel, bool utf16)
        {
            do
            {
                for (auto& rec : buffer) if (rec.EventType == KEY_EVENT)
                {
                    auto& s = rec.Event.KeyEvent.dwControlKeyState;
                    auto& d = rec.Event.KeyEvent.bKeyDown;
                    auto& n = rec.Event.KeyEvent.wRepeatCount;
                    auto& v = rec.Event.KeyEvent.wVirtualKeyCode;
                    auto& c = rec.Event.KeyEvent.uChar.UnicodeChar;
                    cooked.ctrl = s;
                    if (n-- && (d && (c || truechar<'\0'>(v, s))
                            || !d &&  c && v == VK_MENU))
                    {
                        auto grow = utf::to_utf(c, wcpair, cooked.ustr);
                        if (grow && n)
                        {
                            auto temp = view{ cooked.ustr.data() + cooked.ustr.size() - grow, grow };
                            while (n--) cooked.ustr += temp;
                        }
                    }
                }
                buffer.clear();
            }
            while (cooked.ustr.empty() && ((void)signal.wait(lock, [&]{ return buffer.size() || closed || cancel; }), !closed && !cancel));

            cooked.save(utf16);
            ondata.flush();
        }
        template<bool Complete = faux, class Payload>
        auto reply(Payload& packet, cdrw& answer, ui32 readstep)
        {
            auto size = std::min((ui32)cooked.rest.size(), readstep);
            auto data = view{ cooked.rest.data(), size };
            log("\thandle 0x", utf::to_hex(packet.target), ": read rest line: ", utf::debase(packet.input.utf16 ? utf::to_utf((wchr*)data.data(), data.size() / sizeof(wchr))
                                                                                                                : data));
            cooked.rest.remove_prefix(size);
            packet.reply.ctrls = cooked.ctrl;
            packet.reply.bytes = size;
            answer.send_data<Complete>(server.condrv, data);
        }
        template<class Payload>
        auto placeorder(Payload& packet, wiew nameview, view initdata, ui32 readstep)
        {
            auto lock = std::lock_guard{ locker };
            if (cooked.rest.empty())
            {
                // Notes:
                //  - input buffer is shared across the process tree
                //  - input history menu takes keypresses from the shared input buffer
                //  - '\n' is added to the '\r'

                auto token = jobs::token(server.answer, packet.target, faux);
                worker.add(token, [&, readstep, packet, initdata = text{ initdata }, nameview = utf::to_utf(nameview)](auto& token) mutable
                {
                    auto lock = std::unique_lock{ locker };
                    auto& answer = std::get<0>(token);
                    auto& cancel = std::get<2>(token);
                    auto& client = *packet.client;
                    answer.buffer = &packet.input; // Restore after copy.

                    if (closed || cancel) return;
                    
                    cooked.ustr.clear();
                    if (server.inpmod & nt::console::inmode::cooked)
                    {
                        if (packet.input.utf16) utf::to_utf((wchr*)initdata.data(), initdata.size() / 2, cooked.ustr);
                        else                    cooked.ustr = initdata;
                        readline(lock, cancel, packet.input.utf16, packet.input.EOFon, packet.input.stops, client.inputs);
                    }
                    else readchar(lock, cancel, packet.input.utf16);

                    if (cooked.ustr.size())
                    {
                        log("\thandle 0x", utf::to_hex(packet.target), ": read line: ", utf::debase(cooked.ustr));
                    }

                    if (closed || cancel)
                    {
                        cooked.drop();
                        return;
                    }

                    reply<true>(packet, answer, readstep);
                });
                server.answer = {};
            }
            else reply(packet, server.answer, readstep);
        }
        template<bool Complete = faux, class Payload>
        auto readevents(Payload& packet, cdrw& answer)
        {
            if (!server.size_check(packet.echosz, answer.sendoffset())) return;
            auto avail = packet.echosz - answer.sendoffset();
            auto limit = std::min<ui32>(count(), avail / sizeof(recbuf.front()));
            log("\tuser limit: ", limit);
            recbuf.resize(limit);
            auto srcit = buffer.begin();
            auto dstit = recbuf.begin();
            auto endit = recbuf.end();
            while (dstit != endit)
            {
                *dstit++ = *srcit++;
            }
            if (!(packet.input.flags & Payload::peek))
            {
                buffer.erase(buffer.begin(), srcit);
                if (buffer.empty()) ondata.flush();
            }
            packet.reply.count = limit;
            answer.send_data<Complete>(server.condrv, recbuf);
        }
        template<class Payload>
        auto take(Payload& packet)
        {
            auto lock = std::lock_guard{ locker };
            if (buffer.empty())
            {
                log("\tevents buffer is empty");
                if (packet.input.flags & Payload::fast)
                {
                    log("\treply.count = 0");
                    packet.reply.count = 0;
                    return;
                }
                else
                {
                    auto token = jobs::token(server.answer, packet.target, faux);
                    worker.add(token, [&, packet](auto& token) mutable
                    {
                        auto lock = std::unique_lock{ locker };
                        auto& answer = std::get<0>(token);
                        auto& cancel = std::get<2>(token);
                        answer.buffer = &packet.input; // Restore after copy.

                        if (closed || cancel) return;
                        while ((void)signal.wait(lock, [&]{ return buffer.size() || closed || cancel; }), !closed && !cancel && buffer.empty())
                        { }
                        if (closed || cancel) return;

                        readevents<true>(packet, answer);
                        log("\tdeferred task ", utf::to_hex(packet.taskid), " completed, reply.count ", packet.reply.count);
                    });
                    server.answer = {};
                }
            }
            else
            {
                readevents(packet, server.answer);
                log("\treply.count = ", packet.reply.count);
            }
        }
    };

    enum type : ui32 { undefined, trueUTF_8, realUTF16, attribute, fakeUTF16 };

    auto api_unsupported                     ()
    {
        log(prompt, "unsupported consrv request code ", upload.fxtype);
        answer.status = nt::status::illegal_function;
    }
    auto api_system_langid_get               ()
    {
        log(prompt, "GetConsoleLangId");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui16 langid;
            }
            reply;
        };
        auto& packet = payload::cast(upload);
        answer.status = nt::status::not_supported;
        log("\tlangid not supported");
    }
    auto api_system_mouse_buttons_get_count  ()
    {
        log(prompt, "GetNumberOfConsoleMouseButtons");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 count;
            }
            reply;
        };
        auto& packet = payload::cast(upload);
        packet.reply.count = ::GetSystemMetrics(SM_CMOUSEBUTTONS);
        log("\treply.count ", packet.reply.count);
    }
    auto api_process_attach                  ()
    {
        log(prompt, "attach process to console");
        struct payload : wrap<payload>
        {
            ui64 taskid;
            ui32 procid;
            ui32 _pad_1;
            ui32 thread;
            ui32 _pad_2;
        };
        auto& packet = payload::cast(upload);
        struct exec_info : wrap<exec_info>
        {
            ui32 win_icon_id;
            ui32 used_hotkey;
            ui32 start_flags;
            ui16 fill_colors;
            ui16 window_mode;
            si16 scroll_sz_x, scroll_sz_y;
            si16 window_sz_x, window_sz_y;
            si16 window_at_x, window_at_y;
            ui32 app_groupid;
            byte climode_app;
            byte win_visible;
            ui16 header_size;
            wchr header_data[261];
            ui16 curexe_size;
            wchr curexe_data[128];
            ui16 curdir_size;
            wchr curdir_data[261];
        };
        auto& details = exec_info::cast(buffer);
        if (!answer.recv_data(condrv, buffer)) return;

        details.curexe_size = std::min<ui16>(details.curexe_size, sizeof(details.curexe_data));
        details.header_size = std::min<ui16>(details.header_size, sizeof(details.header_data));
        details.curdir_size = std::min<ui16>(details.curdir_size, sizeof(details.curdir_data));

        auto iter = std::find_if(joined.begin(), joined.end(), [&](auto& client) { return client.procid == packet.procid; });
        auto& client = iter != joined.end() ? *iter
                                            : joined.emplace_front();
        auto& inphndl = client.tokens.emplace_front(client, inpmod, hndl::type::events, &events);
        auto& outhndl = client.tokens.emplace_front(client, outmod, hndl::type::scroll, &uiterm.target);
        client.procid = packet.procid;
        client.thread = packet.thread;
        client.pgroup = details.app_groupid;
        client.detail.iconid = details.win_icon_id;
        client.detail.hotkey = details.used_hotkey;
        client.detail.config = details.start_flags;
        client.detail.colors = details.fill_colors;
        client.detail.format = details.window_mode;
        client.detail.cliapp = details.climode_app;
        client.detail.expose = details.win_visible;
        client.detail.scroll = twod{ details.scroll_sz_x, details.scroll_sz_y };
        client.detail.window = rect{{ details.window_at_x, details.window_at_y }, { details.window_sz_x, details.window_sz_y }};
        client.detail.header = utf::to_utf(details.header_data, details.header_size / sizeof(wchr));
        client.detail.curexe = utf::to_utf(details.curexe_data, details.curexe_size / sizeof(wchr));
        client.detail.curdir = utf::to_utf(details.curdir_data, details.curdir_size / sizeof(wchr));
        log("\tprocid ", client.procid, "\n",
            "\tthread ", client.thread, "\n",
            "\tpgroup ", client.pgroup, "\n",
            "\ticonid ", client.detail.iconid, "\n",
            "\thotkey ", client.detail.hotkey, "\n",
            "\tconfig ", client.detail.config, "\n",
            "\tcolors ", client.detail.colors, "\n",
            "\tformat ", client.detail.format, "\n",
            "\tscroll ", client.detail.scroll, "\n",
            "\tcliapp ", client.detail.cliapp, "\n",
            "\texpose ", client.detail.expose, "\n",
            "\twindow ", client.detail.window, "\n",
            "\theader ", client.detail.header, "\n",
            "\tapname ", client.detail.curexe, "\n",
            "\tcurdir ", client.detail.curdir);

        struct connect_info : wrap<connect_info>
        {
            clnt* client_id;
            hndl* events_id;
            hndl* scroll_id;
        };
        auto& info = connect_info::cast(buffer);
        info.client_id = &client;
        info.events_id = &inphndl;
        info.scroll_id = &outhndl;

        answer.buffer = &info;
        answer.length = sizeof(info);
        answer.report = sizeof(info);
        //auto rc = nt::ioctl(nt::console::op::complete_io, condrv, answer);
        //answer = {};
    }
    auto api_process_detach                  ()
    {
        struct payload : drvpacket<payload>
        { };
        auto& packet = payload::cast(upload);
        auto client_ptr = packet.client;
        log(prompt, "detach process from console 0x", utf::to_hex(client_ptr));
        auto iter = std::find_if(joined.begin(), joined.end(), [&](auto& client){ return client_ptr == &client; });
        if (iter != joined.end())
        {
            auto& client = *client_ptr;
            log("\tproc id ", client.procid);
            for (auto& handle : client.tokens)
            {
                auto handle_ptr = &handle;
                log("\tdeactivate handle 0x", utf::to_hex(handle_ptr));
                events.abort(handle_ptr);
            }
            client.tokens.clear();
            joined.erase(iter);
            log("\tprocess 0x", utf::to_hex(client_ptr), " detached");
        }
        else log("\trequested process 0x", utf::to_hex(client_ptr), " not found");
    }
    auto api_process_enlist                  ()
    {
        log(prompt, "GetConsoleProcessList");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 count;
            }
            reply;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_process_create_handle           ()
    {
        log(prompt, "create console handle");
        enum type : ui32
        {
            undefined,
            dupevents,
            dupscroll,
            newscroll,
            seerights,
        };
        GENERIC_READ | GENERIC_WRITE;
        struct payload : base, wrap<payload>
        {
            struct
            {
                type action;
                ui32 shared; // Unused
                ui32 rights; // GENERIC_READ | GENERIC_WRITE
                ui32 _pad_1;
                ui32 _pad_2;
                ui32 _pad_3;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        auto& client = *packet.client;
        log("\tclient procid ", client.procid);
        auto create = [&](auto type, auto msg)
        {
            auto& h = type == hndl::type::events ? client.tokens.emplace_back(client, inpmod, hndl::type::events, &uiterm)
                                                 : client.tokens.emplace_back(client, outmod, hndl::type::scroll, &uiterm.target);
            answer.report = reinterpret_cast<ui64>(&h);
            log(msg, &h);
        };
        switch (packet.input.action)
        {
            case type::dupevents: create(hndl::type::events, "\tdup events handle "); break;
            case type::dupscroll: create(hndl::type::scroll, "\tdup scroll handle "); break;
            case type::newscroll: create(hndl::type::scroll, "\tnew scroll handle "); break;
            default: if (packet.input.rights & GENERIC_READ) create(hndl::type::events, "\tdup (GENERIC_READ) events handle ");
                     else                                    create(hndl::type::scroll, "\tdup (GENERIC_WRITE) scroll handle ");
        }
    }
    auto api_process_delete_handle           ()
    {
        struct payload : drvpacket<payload>
        { };
        auto& packet = payload::cast(upload);
        log(prompt, "delete console handle");
        auto handle_ptr = packet.target;
        if (handle_ptr == nullptr)
        {
            log("\tabort: handle_ptr = invalid_value (0)");
            answer.status = nt::status::invalid_handle;
            return;
        }
        auto client_ptr = &handle_ptr->boss;
        auto client_iter = std::find_if(joined.begin(), joined.end(), [&](auto& client){ return client_ptr == &client; });
        if (client_iter == joined.end())
        {
            log("\tbad handle 0x", utf::to_hex(handle_ptr));
            answer.status = nt::status::invalid_handle;
            return;
        }
        auto& client = handle_ptr->boss;
        auto iter = std::find_if(client.tokens.begin(), client.tokens.end(), [&](auto& token){ return handle_ptr == &token; });
        if (iter != client.tokens.end())
        {
            log("\tdeactivate handle 0x", utf::to_hex(handle_ptr));
            events.abort(handle_ptr);
            client.tokens.erase(iter);
        }
        else log("\trequested handle 0x", utf::to_hex(handle_ptr), " not found");
    }
    auto api_process_codepage_get            ()
    {
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 code_page;
            }
            reply;
            struct
            {
                byte is_output;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        log(prompt, packet.input.is_output ? "GetConsoleOutputCP"
                                           : "GetConsoleCP");
        packet.reply.code_page = CP_UTF8;
        log("\treply.code_page ", CP_UTF8);
    }
    auto api_process_codepage_set            ()
    {
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 code_page;
                byte is_output;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        log(prompt, packet.input.is_output ? "SetConsoleOutputCP"
                                           : "SetConsoleCP");
        log("\tinput.code_page ", packet.input.code_page);
        //wsl depends on this - always reply success
        //answer.status = nt::status::not_supported;
    }
    auto api_process_mode_get                ()
    {
        log(prompt, "GetConsoleMode");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 mode;
            }
            reply;
        };
        auto& packet = payload::cast(upload);
        auto handle_ptr = packet.target;
        if (handle_ptr == nullptr)
        {
            log("\tabort: handle_ptr = invalid_value (0)");
            answer.status = nt::status::invalid_handle;
            return;
        }
        auto& handle = *handle_ptr;
        packet.reply.mode = handle.mode;
        log("\treply.mode ", handle);
    }
    auto api_process_mode_set                ()
    {
        log(prompt, "SetConsoleMode");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 mode;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        auto handle_ptr = packet.target;
        if (handle_ptr == nullptr)
        {
            log("\tabort: handle_ptr = invalid_value (0)");
            answer.status = nt::status::invalid_handle;
            return;
        }
        auto& handle = *handle_ptr;
        if (handle.kind == hndl::type::scroll)
        {
            auto cur_mode = handle.mode       & nt::console::outmode::no_auto_cr;
            auto new_mode = packet.input.mode & nt::console::outmode::no_auto_cr;
            if (cur_mode != new_mode)
            {
                auto autocr = !new_mode;
                uiterm.normal.set_autocr(autocr);
                log("\tauto_crlf ", autocr ? "enabled" : "disabled");
            }
        }
        else if (handle.kind == hndl::type::events)
        {
            auto mouse_mode = packet.input.mode & nt::console::inmode::mouse;
            if (mouse_mode)
            {
                uiterm.mtrack.enable (decltype(uiterm.mtrack)::all_movements);
                uiterm.mtrack.setmode(decltype(uiterm.mtrack)::w32);
            }
            else uiterm.mtrack.disable(decltype(uiterm.mtrack)::all_movements);
            log("\tmouse_input ", mouse_mode ? "enabled" : "disabled");
        }
        handle.mode = packet.input.mode;
        log("\tinput.mode ", handle);
    }
    template<bool RawRead = faux>
    auto api_events_read_as_text             ()
    {
        log(prompt, "ReadConsole");
        struct payload : drvpacket<payload>
        {
            struct
            {
                byte utf16;
                byte EOFon;
                ui16 exesz;
                ui32 affix;
                ui32 stops;
            }
            input;
            struct
            {
                ui32 ctrls;
                ui32 bytes;
            }
            reply;
        };
        auto& packet = payload::cast(upload);
        if constexpr (RawRead)
        {
            log("\traw read (ReadFile emulation)");
            packet.input = { .EOFon = 1 };
        }
        auto namesize = static_cast<ui32>(packet.input.exesz * sizeof(wchr));
        if (!size_check(packet.echosz,  packet.input.affix)
         || !size_check(packet.echosz,  answer.sendoffset())) return;
        auto readstep = packet.echosz - answer.sendoffset();
        auto datasize = namesize + packet.input.affix;
        buffer.resize(datasize);
        if (answer.recv_data(condrv, buffer) == faux) return;

        auto nameview = wiew(reinterpret_cast<wchr*>(buffer.data()), packet.input.exesz);
        auto initdata = view(buffer.data() + namesize, packet.input.affix);
        log("\tnamesize: ", namesize);
        log("\tnameview: ", utf::debase(utf::to_utf(nameview)));
        log("\treadstep: ", readstep);
        log("\tinitdata: ", packet.input.utf16 ? "utf-16: " + utf::debase(utf::to_utf(wiew((wchr*)initdata.data(), initdata.size() / 2)))
                                               : "utf-8: "  + utf::debase(initdata));
        events.placeorder(packet, nameview, initdata, readstep);
    }
    auto api_events_clear                    ()
    {
        log(prompt, "FlushConsoleInputBuffer");
        events.clear();
    }
    auto api_events_count_get                ()
    {
        log(prompt, "GetNumberOfInputEvents");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 count;
            }
            reply;
        };
        auto& packet = payload::cast(upload);
        packet.reply.count = events.count();
        log("\treply.count ", packet.reply.count);
    }
    auto api_events_get                      ()
    {
        struct payload : drvpacket<payload>
        {
            enum
            {
                take,
                peek,
                fast,
            };
            struct
            {
                ui32 count;
            }
            reply;
            struct
            {
                ui16 flags;
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        log(prompt, packet.input.flags & payload::peek ? "PeekConsoleInput"
                                                       : "ReadConsoleInput");
        log("\tinput.flags ", utf::to_hex(packet.input.flags));

        auto client_ptr = packet.client;
        if (client_ptr == nullptr)
        {
            log("\tabort: packet.client = invalid_value (0)");
            answer.status = nt::status::invalid_handle;
            return;
        }
        auto& client = packet.client;
        //todo validate client
        auto events_handle_ptr = packet.target;
        if (events_handle_ptr == nullptr)
        {
            log("\tabort: events_handle_ptr = invalid_value (0)");
            answer.status = nt::status::invalid_handle;
            return;
        }
        //todo validate events_handle
        auto& events_handle = *events_handle_ptr;

        //todo filter events by client's input mode
        //  ENABLE_MOUSE_INPUT 
        //  ENABLE_WINDOW_INPUT
        //  
        events.take(packet);
    }
    auto api_events_add                      ()
    {
        log(prompt, "WriteConsoleInput");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 count;
            }
            reply;
            struct
            {
                byte utf16;
                byte totop;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_events_generate_ctrl_event      ()
    {
        log(prompt, "GenerateConsoleCtrlEvent");
        enum type : ui32
        {
            ctrl_c      = CTRL_C_EVENT,
            ctrl_break  = CTRL_BREAK_EVENT,
            close       = CTRL_CLOSE_EVENT,
            logoff      = CTRL_LOGOFF_EVENT,
            shutdown    = CTRL_SHUTDOWN_EVENT,
        };
        struct payload : drvpacket<payload>
        {
            struct
            {
                type event;
                ui32 group;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        events.alert(packet.input.event, packet.input.group);
    }
    template<bool RawWrite = faux>
    auto api_scrollback_write_text           ()
    {
        log(prompt, "WriteConsole");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 count;
            }
            reply;
            struct
            {
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        if constexpr (RawWrite)
        {
            log("\traw write emulation");
            packet.input = {};
        }

        auto client_ptr = packet.client;
        if (client_ptr == nullptr)
        {
            log("\tabort: packet.client = invalid_value (0)");
            answer.status = nt::status::invalid_handle;
            return;
        }
        auto& client = *client_ptr;

        auto scroll_handle_ptr = packet.target;
        if (scroll_handle_ptr == nullptr)
        {
            log("\tabort: packet.target = invalid_value (0)");
            answer.status = nt::status::invalid_handle;
            return;
        }
        //todo implement scrollback buffer selection
        auto& scroll_handle = *scroll_handle_ptr;

        if (!size_check(packet.packsz,  answer.readoffset())) return;
        auto datasize = packet.packsz - answer.readoffset();
        auto initsize = scroll_handle.rest.size();
        if (packet.input.utf16)
        {
            toWIDE.resize(datasize / sizeof(wchr));
            if (answer.recv_data(condrv, toWIDE) == faux) return;
            utf::to_utf(toWIDE, scroll_handle.rest);
        }
        else
        {
            scroll_handle.rest.resize(initsize + datasize);
            if (answer.recv_data(condrv, view{ scroll_handle.rest.data() + initsize, datasize }) == faux) return;
        }

        if (auto crop = ansi::purify(scroll_handle.rest))
        {
            //todo ENABLE_WRAP_AT_EOL_OUTPUT
            uiterm.ondata(crop);
            log(packet.input.utf16 ? "\tUTF-16: ":"\tUTF-8: ", utf::debase<faux, faux>(crop));
            scroll_handle.rest.erase(0, crop.size()); // Delete processed data.
        }
        packet.reply.count = datasize;
        answer.report = packet.reply.count;
    }
    auto api_scrollback_write_data           ()
    {
        struct payload : drvpacket<payload>
        {
            struct
            {
                si16 coorx, coory;
                type etype;
            }
            input;
            struct
            {
                ui32 count;
            }
            reply;
        };
        auto& packet = payload::cast(upload);
        log(prompt, packet.input.etype == type::attribute ? "WriteConsoleOutputAttribute"
                                                          : "WriteConsoleOutputCharacter");
    }
    auto api_scrollback_write_block          ()
    {
        log(prompt, "WriteConsoleOutput");
        struct payload : drvpacket<payload>
        {
            union
            {
                struct
                {
                    si16 rectL, rectT, rectR, rectB;
                    byte utf16;
                }
                input;
                struct
                {
                    si16 rectL, rectT, rectR, rectB;
                    byte _pad1;
                }
                reply;
            };
        };
        auto& packet = payload::cast(upload);

    }
    auto api_scrollback_attribute_set        ()
    {
        log(prompt, "SetConsoleTextAttribute");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui16 color;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_scrollback_fill                 ()
    {
        struct payload : drvpacket<payload>
        {
            union
            {
                struct
                {
                    si16 coorx, coory;
                    type etype;
                    ui16 piece;
                    ui32 count;
                }
                input;
                struct
                {
                    si16 _pad1, _pad2;
                    ui32 _pad3;
                    ui16 _pad4;
                    ui32 count;
                }
                reply;
            };
        };
        auto& screen = *uiterm.target;
        auto& packet = payload::cast(upload);
        auto& colors = uiterm.ctrack.color;
        auto count = packet.input.count;
        auto coord = std::clamp(twod{ packet.input.coorx, packet.input.coory }, dot_00, screen.panel - dot_11);
        auto piece = packet.input.piece;
        auto maxsz = screen.panel.x * (screen.panel.y - coord.y) - coord.x;
        auto saved = screen.coord;
        screen.set_coord(coord);
        if (packet.input.etype == type::attribute)
        {
            log(prompt, "FillConsoleOutputAttribute",
                        "\tcoord ", coord,
                        "\tcount ", count);
            auto c = cell{ whitespace }.fgc(colors[netxs::swap_bits<0, 2>(piece      & 0x000Fu)]) // FOREGROUND_ . . .
                                       .bgc(colors[netxs::swap_bits<0, 2>(piece >> 4 & 0x000Fu)]) // BACKGROUND_ . . .
                                       .inv(!!(piece & COMMON_LVB_REVERSE_VIDEO))
                                       .und(!!(piece & COMMON_LVB_UNDERSCORE));
            log("\tfill using attributes 0x", utf::to_hex(piece));
            if ((si32)count > maxsz) count = std::max(0, maxsz);
            filler.kill();
            filler.crop(count, c);
            uiterm.direct(count, filler.pick(), cell::shaders::meta);
        }
        else
        {
            log(prompt, "FillConsoleOutputCharacter",
                        "\tcoord ", coord,
                        "\tcount ", count,
                        "\tvalue ", piece);

            if (piece <  ' ' || piece == 0x7F) piece = ' ';
            if (piece == ' ' && (si32)count > maxsz)
            {
                log("\terase below");
                screen.ed(0 /*commands::erase::display::below*/);
            }
            else
            {
                toUTF8.clear();
                utf::to_utf((wchr)piece, toUTF8);
                log("\tfill using char ", toUTF8);
                auto c = cell{ toUTF8 };
                auto w = c.wdt();
                if (w == 1 || w == 2)
                {
                    if ((si32)count > maxsz) count = std::max(0, maxsz);
                    filler.kill();
                    filler.crop(count / w, c);
                    uiterm.direct(count, filler.pick(), cell::shaders::text);
                }
            }
        }
        screen.set_coord(saved);
        packet.reply.count = count;
    }
    auto api_scrollback_read_data            ()
    {
        struct payload : drvpacket<payload>
        {
            struct
            {
                si16 coorx, coory;
                type etype;
            }
            input;
            struct
            {
                ui32 count;
            }
            reply;
        };
        auto& packet = payload::cast(upload);
        log(prompt, packet.input.etype == type::attribute ? "ReadConsoleOutputAttribute"
                                                          : "ReadConsoleOutputCharacter");
    }
    auto api_scrollback_read_block           ()
    {
        log(prompt, "ReadConsoleOutput");
        struct payload : drvpacket<payload>
        {
            union
            {
                struct
                {
                    si16 rectL, rectT, rectR, rectB;
                    byte utf16;
                }
                input;
                struct
                {
                    si16 rectL, rectT, rectR, rectB;
                    byte _pad1;
                }
                reply;
            };
        };
        auto& packet = payload::cast(upload);

    }
    auto api_scrollback_set_active           ()
    {
        log(prompt, "SetConsoleActiveScreenBuffer");
        struct payload : drvpacket<payload>
        { };
        auto& packet = payload::cast(upload);

    }
    auto api_scrollback_cursor_coor_set      ()
    {
        struct payload : drvpacket<payload>
        {
            struct
            {
                si16 coorx, coory;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        log(prompt, "SetConsoleCursorPosition ");
        auto caretpos = twod{ packet.input.coorx, packet.input.coory };
        log("\tinput.cursor_coor ", caretpos);
        uiterm.target->cup(caretpos + dot_11);
    }
    auto api_scrollback_cursor_info_get      ()
    {
        log(prompt, "GetConsoleCursorInfo");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 style;
                byte alive;
            }
            reply;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_scrollback_cursor_info_set      ()
    {
        log(prompt, "SetConsoleCursorInfo");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 style;
                byte alive;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_scrollback_info_get             ()
    {
        log(prompt, "GetConsoleScreenBufferInfo");
        struct payload : drvpacket<payload>
        {
            struct
            {
                si16 buffersz_x, buffersz_y;
                si16 cursorposx, cursorposy;
                si16 windowposx, windowposy;
                ui16 attributes;
                si16 windowsz_x, windowsz_y;
                si16 maxwinsz_x, maxwinsz_y;
                ui16 popupcolor;
                byte fullscreen;
                ui32 rgbpalette[16];
            }
            reply;
        };
        auto& packet = payload::cast(upload);
        auto viewport = uiterm.target->panel;
        auto caretpos = uiterm.target->coord;
        packet.reply.cursorposx = caretpos.x;
        packet.reply.cursorposy = caretpos.y;
        packet.reply.buffersz_x = viewport.x;
        packet.reply.buffersz_y = viewport.y;
        packet.reply.windowsz_x = viewport.x;
        packet.reply.windowsz_y = viewport.y;
        packet.reply.maxwinsz_x = viewport.x;
        packet.reply.maxwinsz_y = viewport.y;
        packet.reply.windowposx = 0;
        packet.reply.windowposy = 0;
        packet.reply.fullscreen = faux;
        packet.reply.popupcolor = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        auto& rgbpalette = packet.reply.rgbpalette;
        auto mark = uiterm.target->brush;
        auto frgb = mark.fgc().token;
        auto brgb = mark.bgc().token;
        auto head = std::begin(uiterm.ctrack.color);
        auto fgcx = 8_sz; // Fallback for true colors.
        auto bgcx = 0_sz;
        for (auto i = 0; i < 16; i++)
        {
            auto const& c = *head++;
            auto m = netxs::swap_bits<0, 2>(i); // ANSI<->DOS color scheme reindex.
            rgbpalette[m] = c;
            if (c == frgb) fgcx = m;
            if (c == brgb) bgcx = m + 1;
        }
        if (!bgcx--) // Reset background if true colors are used.
        {
            bgcx = 0;
            uiterm.ctrack.color[bgcx] = brgb;
            rgbpalette         [bgcx] = brgb;
        }
        packet.reply.attributes = static_cast<ui16>(fgcx + (bgcx << 4));
        if (mark.inv()) packet.reply.attributes |= COMMON_LVB_REVERSE_VIDEO;
        if (mark.und()) packet.reply.attributes |= COMMON_LVB_UNDERSCORE;
        log("\treply.attributes 0x", utf::to_hex(packet.reply.attributes),
            "\n\treply.cursor_coor ", caretpos,
            "\n\treply.window_size ", viewport);
    }
    auto api_scrollback_info_set             ()
    {
        log(prompt, "SetConsoleScreenBufferInfo");
        struct payload : drvpacket<payload>
        {
            struct
            {
                si16 buffersz_x, buffersz_y;
                si16 cursorposx, cursorposy;
                si16 windowposx, windowposy;
                ui16 attributes;
                si16 windowsz_x, windowsz_y;
                si16 maxwinsz_x, maxwinsz_y;
                ui16 popupcolor;
                byte fullscreen;
                ui32 rgbpalette[16];
            }
            input;
        };
        auto& packet = payload::cast(upload);
        auto caretpos = twod{ packet.input.cursorposx, packet.input.cursorposy };
        uiterm.target->cup(caretpos + dot_11);
        log("\tbuffer size ", twod{ packet.input.buffersz_x, packet.input.buffersz_y });
        log("\tcursor coor ", twod{ packet.input.cursorposx, packet.input.cursorposy });
        log("\twindow coor ", twod{ packet.input.windowposx, packet.input.windowposy });
        log("\tattributes x", utf::to_hex(packet.input.attributes));
        log("\twindow size ", twod{ packet.input.windowsz_x, packet.input.windowsz_y });
        log("\tmaxwin size ", twod{ packet.input.maxwinsz_x, packet.input.maxwinsz_y });
        log("\tpopup color ", packet.input.popupcolor);
        log("\tfull screen ", packet.input.fullscreen);
        log("\trgb palette ");
        auto i = 0;
        for (auto c : packet.input.rgbpalette)
        {
            log("\t\t", utf::to_hex(i), " ", rgba{ c });
        }
        //todo set palette
        //todo set attributes

    }
    auto api_scrollback_size_set             ()
    {
        log(prompt, "SetConsoleScreenBufferSize");
        struct payload : drvpacket<payload>
        {
            struct
            {
                si16 buffersz_x, buffersz_y;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_scrollback_viewport_get_max_size()
    {
        log(prompt, "GetLargestConsoleWindowSize");
        struct payload : drvpacket<payload>
        {
            struct
            {
                si16 maxwinsz_x, maxwinsz_y;
            }
            reply;
        };
        auto& packet = payload::cast(upload);
        auto viewport = uiterm.target->panel;
        packet.reply.maxwinsz_x = viewport.x;
        packet.reply.maxwinsz_y = viewport.y;
        log("\tmaxwin size ", viewport);
    }
    auto api_scrollback_viewport_set         ()
    {
        log(prompt, "SetConsoleWindowInfo");
        struct payload : drvpacket<payload>
        {
            struct
            {
                byte isabsolute;
                si16 rectL, rectT, rectR, rectB;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_scrollback_scroll               ()
    {
        log(prompt, "ScrollConsoleScreenBuffer");
        struct payload : drvpacket<payload>
        {
            struct
            {
                si16 scrlL, scrlT, scrlR, scrlB;
                si16 clipL, clipT, clipR, clipB;
                byte trunc;
                byte utf16;
                si16 destx, desty;
                union
                {
                wchr wchar;
                char ascii;
                };
                ui16 color;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_scrollback_selection_info_get   ()
    {
        log(prompt, "GetConsoleSelectionInfo");
        static constexpr ui32 mouse_down   = CONSOLE_MOUSE_DOWN;
        static constexpr ui32 use_mouse    = CONSOLE_MOUSE_SELECTION;
        static constexpr ui32 no_selection = CONSOLE_NO_SELECTION;
        static constexpr ui32 in_progress  = CONSOLE_SELECTION_IN_PROGRESS;
        static constexpr ui32 not_empty    = CONSOLE_SELECTION_NOT_EMPTY;
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 flags;
                si16 headx, heady;
                si16 rectL, rectT, rectR, rectB;
            }
            reply;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_window_title_get                ()
    {
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 count;
            }
            reply;
            struct
            {
                byte utf16;
                byte prime;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        log(prompt, packet.input.prime ? "GetConsoleOriginalTitle"
                                       : "GetConsoleTitle");
        //todo differentiate titles by category
        auto& title = uiterm.wtrack.get(ansi::OSC_TITLE);
        log("\treply title (", title.size(), "): ", title);
        if (packet.input.utf16)
        {
            toWIDE.clear();
            utf::to_utf(title, toWIDE);
            packet.reply.count = static_cast<ui32>(toWIDE.size() + 1 /*null terminator*/);
            answer.send_data(condrv, toWIDE, true);
        }
        else
        {
            packet.reply.count = static_cast<ui32>(title.size() + 1 /*null terminator*/);
            answer.send_data(condrv, title, true);
        }
    }
    auto api_window_title_set                ()
    {
        log(prompt, "SetConsoleTitle");
        struct payload : drvpacket<payload>
        {
            struct
            {
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        if (!size_check(packet.packsz,  answer.readoffset())) return;
        auto datasize = packet.packsz - answer.readoffset();
        buffer.resize(datasize);
        if (answer.recv_data(condrv, buffer) == faux) return;
        if (packet.input.utf16)
        {
            toUTF8.clear();
            utf::to_utf(reinterpret_cast<wchr*>(buffer.data()), buffer.size() / sizeof(wchr), toUTF8);
            uiterm.wtrack.set(ansi::OSC_TITLE, toUTF8);
            log("\tUTF-16 title: ", utf::debase(toUTF8));
        }
        else
        {
            uiterm.wtrack.set(ansi::OSC_TITLE, buffer);
            log("\tUTF-8 title: ", utf::debase(buffer));
        }
    }
    auto api_window_font_size_get            ()
    {
        log(prompt, "GetConsoleFontSize");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 index;
            }
            input;
            struct
            {
                si16 sizex, sizey;
            }
            reply;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_window_font_get                 ()
    {
        log(prompt, "GetCurrentConsoleFont");
        struct payload : drvpacket<payload>
        {
            struct
            {
                byte fullscreen;
            }
            input;
            struct
            {
                ui32 index;
                si16 sizex, sizey;
                ui32 pitch;
                ui32 heavy;
                wchr brand[LF_FACESIZE];
            }
            reply;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_window_font_set                 ()
    {
        log(prompt, "SetConsoleCurrentFont");
        struct payload : drvpacket<payload>
        {
            union
            {
                struct
                {
                    byte fullscreen;
                    ui32 index;
                    si16 sizex, sizey;
                    ui32 pitch;
                    ui32 heavy;
                    wchr brand[LF_FACESIZE];
                }
                input;
                struct
                {
                    byte _pad1;
                    ui32 index;
                    si16 sizex, sizey;
                    ui32 pitch;
                    ui32 heavy;
                    wchr brand[LF_FACESIZE];
                }
                reply;
            };
        };
        auto& packet = payload::cast(upload);

    }
    auto api_window_mode_get                 ()
    {
        log(prompt, "GetConsoleDisplayMode");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 flags;
            }
            reply;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_window_mode_set                 ()
    {
        log(prompt, "SetConsoleDisplayMode");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 flags;
            }
            input;
            struct
            {
                si16 buffersz_x, buffersz_y;
            }
            reply;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_window_handle_get               ()
    {
        log(prompt, "GetConsoleWindow");
        struct payload : drvpacket<payload>
        {
            struct
            {
                HWND handle;
            }
            reply;
        };
        auto& packet = payload::cast(upload);
        packet.reply.handle = std::numeric_limits<HWND>::max(); // Fake window handle to tell powershell that everything is under console control.
        log("\tfake window handle 0x", utf::to_hex(packet.reply.handle));
    }
    auto api_window_xkeys                    ()
    {
        log(prompt, "SetConsoleKeyShortcuts");
        struct payload : drvpacket<payload>
        {
            struct
            {
                byte enabled;
                byte keyflag;
            }
            input;
        };
        auto& packet = payload::cast(upload);
        log("\trequest ", packet.input.enabled ? "set" : "unset", " xkeys\n",
                          packet.input.keyflag & 0x01 ? "\t\tAlt+Tab\n"   : "",
                          packet.input.keyflag & 0x02 ? "\t\tAlt+Esc\n"   : "",
                          packet.input.keyflag & 0x04 ? "\t\tAlt+Space\n" : "",
                          packet.input.keyflag & 0x08 ? "\t\tAlt+Enter\n" : "",
                          packet.input.keyflag & 0x10 ? "\t\tAlt+Prtsc\n" : "",
                          packet.input.keyflag & 0x20 ? "\t\tPrtsc\n"     : "",
                          packet.input.keyflag & 0x40 ? "\t\tCtrl+Esc\n"  : "");
    }
    auto api_alias_get                       ()
    {
        log(prompt, "GetConsoleAlias");
        struct payload : drvpacket<payload>
        {
            union
            {
                struct
                {
                    ui16 srcsz;
                    ui16 _pad1;
                    ui16 exesz;
                    byte utf16;
                }
                input;
                struct
                {
                    ui16 _pad1;
                    ui16 dstsz;
                    ui16 _pad2;
                    byte _pad3;
                }
                reply;
            };
        };
        auto& packet = payload::cast(upload);

    }
    auto api_alias_add                       ()
    {
        log(prompt, "AddConsoleAlias");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui16 srcsz;
                ui16 dstsz;
                ui16 exesz;
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_alias_exes_get_volume           ()
    {
        log(prompt, "GetConsoleAliasExesLength");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 bytes;
            }
            reply;
            struct
            {
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_alias_exes_get                  ()
    {
        log(prompt, "GetConsoleAliasExes");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 bytes;
            }
            reply;
            struct
            {
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_aliases_get_volume              ()
    {
        log(prompt, "GetConsoleAliasesLength");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 bytes;
            }
            reply;
            struct
            {
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_aliases_get                     ()
    {
        log(prompt, "GetConsoleAliases");
        struct payload : drvpacket<payload>
        {
            struct
            {
                byte utf16;
            }
            input;
            struct
            {
                ui32 bytes;
            }
            reply;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_input_history_clear             ()
    {
        log(prompt, "ClearConsoleCommandHistory");
        struct payload : drvpacket<payload>
        {
            struct
            {
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_input_history_limit_set         ()
    {
        log(prompt, "SetConsoleNumberOfCommands");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 count;
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_input_history_get_volume        ()
    {
        log(prompt, "GetConsoleCommandHistoryLength");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 count;
            }
            reply;
            struct
            {
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_input_history_get               ()
    {
        log(prompt, "GetConsoleCommandHistory");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 bytes;
            }
            reply;
            struct
            {
                byte utf16;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_input_history_info_get          ()
    {
        log(prompt, "GetConsoleHistory");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 limit;
                ui32 count;
                ui32 flags;
            }
            reply;
        };
        auto& packet = payload::cast(upload);

    }
    auto api_input_history_info_set          ()
    {
        log(prompt, "SetConsoleHistory");
        struct payload : drvpacket<payload>
        {
            struct
            {
                ui32 limit;
                ui32 count;
                ui32 flags;
            }
            input;
        };
        auto& packet = payload::cast(upload);

    }

    using apis = std::vector<void(consrv::*)()>;
    using list = std::list<clnt>;

    Term&       uiterm; // consrv: Terminal reference.
    fd_t&       condrv; // consrv: Console driver handle.
    event_list  events; // consrv: Input event list.
    view        prompt; // consrv: Log prompt.
    list        joined; // consrv: Attached processes list.
    std::thread server; // consrv: Main thread.
    cdrw        answer; // consrv: Reply cue.
    task        upload; // consrv: Console driver request.
    text        buffer; // consrv: Temp buffer.
    text        toUTF8; // consrv: Buffer for UTF-16-UTF-8 conversion.
    wide        toWIDE; // consrv: Buffer for UTF-8-UTF-16 conversion.
    rich        filler; // consrv: Buffer for filling operations.
    apis        apimap; // consrv: Fx reference.
    ui32        inpmod; // consrv: Events mode flag set.
    ui32        outmod; // consrv: Scrollbuffer mode flag set.

    void start()
    {
        server = std::thread{ [&]
        {
            while (condrv != INVALID_FD)
            {
                auto rc = nt::ioctl(nt::console::op::read_io, condrv, answer, upload);
                answer = { .taskid = upload.taskid, .status = nt::status::success };
                switch (rc)
                {
                    case ERROR_SUCCESS:
                    {
                        if (upload.fxtype == nt::console::fx::subfx)
                        {
                            upload.fxtype = upload.callfx / 0x55555 + upload.callfx;
                            answer.buffer = upload.argbuf;
                            answer.length = upload.arglen;
                            answer._pad_1 = upload.arglen + sizeof(ui32) * 2 /*sizeof(drvpacket payload)*/;
                        }
                        auto proc = apimap[upload.fxtype & 255];
                        uiterm.update([&] { (this->*proc)(); });
                        break;
                    }
                    case ERROR_IO_PENDING:         log(prompt, "operation has not completed"); ::WaitForSingleObject(condrv, 0); break;
                    case ERROR_PIPE_NOT_CONNECTED: log(prompt, "client disconnected"); return;
                    default:                       log(prompt, "unexpected nt::ioctl result ", rc); break;
                }
            }
            log(prompt, "condrv main loop ended");
        }};
    }
    void resize(twod const& newsize)
    {
        events.winsz(newsize); // We inform about viewport resize only.
    }
    void stop()
    {
        events.stop();
        if (server.joinable())
        {
            server.join();
        }
        log(prompt, "stop()");
    }
    template<class T>
    auto size_check(T upto, T from)
    {
        auto test = upto >= from;
        if (!test)
        {
            log("\tabort: negative size");
            answer.status = nt::status::unsuccessful;
        }
        return test;
    }
    auto reset()
    {
        inpmod = nt::console::inmode::preprocess
               | nt::console::inmode::cooked
               | nt::console::inmode::echo
               | nt::console::inmode::mouse
               | nt::console::inmode::insert
               | nt::console::inmode::quickedit;
        outmod = nt::console::outmode::preprocess
               | nt::console::outmode::wrap_at_eol
               | nt::console::outmode::vt;
        uiterm.normal.set_autocr(!(outmod & nt::console::outmode::no_auto_cr));
    }

    consrv(Term& uiterm, fd_t& condrv)
        : uiterm{ uiterm },
          condrv{ condrv },
          events{ *this  },
          answer{        },
          prompt{ " pty: consrv: " }
    {
        reset();
        using _ = consrv;
        apimap.resize(0xFF, &_::api_unsupported);
        apimap[0x38] = &_::api_system_langid_get;
        apimap[0x91] = &_::api_system_mouse_buttons_get_count;
        apimap[0x01] = &_::api_process_attach;
        apimap[0x02] = &_::api_process_detach;
        apimap[0xB9] = &_::api_process_enlist;
        apimap[0x03] = &_::api_process_create_handle;
        apimap[0x04] = &_::api_process_delete_handle;
        apimap[0x30] = &_::api_process_codepage_get;
        apimap[0x64] = &_::api_process_codepage_set;
        apimap[0x31] = &_::api_process_mode_get;
        apimap[0x32] = &_::api_process_mode_set;
        apimap[0x06] = &_::api_events_read_as_text<true>;
        apimap[0x35] = &_::api_events_read_as_text;
        apimap[0x08] = &_::api_events_clear;
        apimap[0x63] = &_::api_events_clear;
        apimap[0x33] = &_::api_events_count_get;
        apimap[0x34] = &_::api_events_get;
        apimap[0x70] = &_::api_events_add;
        apimap[0x61] = &_::api_events_generate_ctrl_event;
        apimap[0x05] = &_::api_scrollback_write_text<true>;
        apimap[0x36] = &_::api_scrollback_write_text;
        apimap[0x72] = &_::api_scrollback_write_data;
        apimap[0x71] = &_::api_scrollback_write_block;
        apimap[0x6D] = &_::api_scrollback_attribute_set;
        apimap[0x60] = &_::api_scrollback_fill;
        apimap[0x6F] = &_::api_scrollback_read_data;
        apimap[0x73] = &_::api_scrollback_read_block;
        apimap[0x62] = &_::api_scrollback_set_active;
        apimap[0x6A] = &_::api_scrollback_cursor_coor_set;
        apimap[0x65] = &_::api_scrollback_cursor_info_get;
        apimap[0x66] = &_::api_scrollback_cursor_info_set;
        apimap[0x67] = &_::api_scrollback_info_get;
        apimap[0x68] = &_::api_scrollback_info_set;
        apimap[0x69] = &_::api_scrollback_size_set;
        apimap[0x6B] = &_::api_scrollback_viewport_get_max_size;
        apimap[0x6E] = &_::api_scrollback_viewport_set;
        apimap[0x6C] = &_::api_scrollback_scroll;
        apimap[0xB8] = &_::api_scrollback_selection_info_get;
        apimap[0x74] = &_::api_window_title_get;
        apimap[0x75] = &_::api_window_title_set;
        apimap[0x93] = &_::api_window_font_size_get;
        apimap[0x94] = &_::api_window_font_get;
        apimap[0xBC] = &_::api_window_font_set;
        apimap[0xA1] = &_::api_window_mode_get;
        apimap[0x9D] = &_::api_window_mode_set;
        apimap[0xAF] = &_::api_window_handle_get;
        apimap[0xAC] = &_::api_window_xkeys;
        apimap[0xA3] = &_::api_alias_get;
        apimap[0xA2] = &_::api_alias_add;
        apimap[0xA5] = &_::api_alias_exes_get_volume;
        apimap[0xA7] = &_::api_alias_exes_get;
        apimap[0xA4] = &_::api_aliases_get_volume;
        apimap[0xA6] = &_::api_aliases_get;
        apimap[0xA8] = &_::api_input_history_clear;
        apimap[0xA9] = &_::api_input_history_limit_set;
        apimap[0xAA] = &_::api_input_history_get_volume;
        apimap[0xAB] = &_::api_input_history_get;
        apimap[0xBA] = &_::api_input_history_info_get;
        apimap[0xBB] = &_::api_input_history_info_set;
    }
};

#if not defined(_DEBUG)
    #undef log
#endif

#endif // NETXS_CONSRV_HPP