// Copyright (c) Dmitry Sapozhnikov
// Licensed under the MIT license.

#pragma once
#include "system.hpp"

namespace netxs::scripting
{
    using namespace ui;

    struct events
    {
        EVENTPACK( events, netxs::events::userland::root::scripting )
        {
            EVENT_XS( invoke, eccc ), // Invoke script.
        };
    };

    namespace path
    {
        static constexpr auto scripting = "/config/scripting/";
    }
    namespace attr
    {
        static constexpr auto env = "env";
        static constexpr auto cwd = "cwd";
        static constexpr auto cmd = "cmd";
        static constexpr auto run = "run";
        static constexpr auto tty = "usetty";
        static constexpr auto rse = "engine"; // Runtime Scripting Engine.
    }

    struct nullterm
    {
        bool  io_log{}; // nullterm: Stdio logging flag.
        bool  bpmode{}; // nullterm: Bracketed paste mode flag.
        bool  altbuf{}; // nullterm: Altbuf buffer stub.
        bool  normal{}; // nullterm: Normal buffer stub.
        bool* target{ &normal }; // nullterm: Current buffer stub.
    };

    struct host
        : public nullterm, public pro::skill
    {
        using s11n = directvt::binary::s11n;
        using pidt = os::pidt;
        using vtty = netxs::sptr<os::runspace::base_tty<scripting::host>>;
        using skill::boss,
              skill::memo;

        // scripting::host: Event handler.
        class xlat
            : public s11n
        {
            ui::host& owner; // xlat: Boss object reference.
            subs      token; // xlat: Subscription tokens.

        public:
            void disable()
            {
                s11n::stop();
                token.clear();
            }

            void handle(s11n::xs::fullscrn          /*lock*/)
            {
                //...
            }
            void handle(s11n::xs::sysfocus          /*lock*/)
            {
                //...
            }
            void handle(s11n::xs::syskeybd          /*lock*/)
            {
                //...
            };

            xlat(ui::host& owner)
                : s11n{ *this },
                 owner{ owner }
            {
                this->owner.LISTEN(tier::release, hids::events::device::mouse::any, gear, token)
                {
                    //...
                };
                this->owner.LISTEN(tier::general, hids::events::die, gear, token)
                {
                    //...
                };
            }
        };

        xlat stream; // scripting::host: Event tracker.
        eccc appcfg; // scripting::host: Application startup config.
        flag active; // scripting::host: Scripting engine lifetime.
        vtty engine; // scripting::host: Scripting engine instance.

        // scripting::host: Proceed input.
        auto ondata_direct(view data)
        {
            log<faux>(ansi::fgc(greenlt).add(data).nil());
            return faux;
        }
        // scripting::host: Proceed input.
        void ondata(view data)
        {
            ondata_direct(data);
        }
        // scripting::host: Cooked read input.
        void data(rich& /*data*/)
        {
            if (active)
            {
                auto lock = boss.bell::sync();
                // It is a powershell readline echo.
                //log<faux>(ansi::fgc(cyanlt).add(data.utf8()).nil());
            }
        }
        // scripting::host: Shutdown callback handler.
        void onexit(si32 /*code*/, view /*msg*/ = {}, bool /*exit_after_sighup*/ = faux)
        {
            //todo initiate global shutdown

            //owner.bell::enqueue(owner_wptr, [&, code](auto& boss) mutable
            //{
            //    if (code) log(ansi::bgc(reddk).fgc(whitelt).add('\n', prompt::repl, "Exit code ", utf::to_hex_0x(code), ' ').nil());
            //    else      log(prompt::repl, "Exit code 0");
            //    //backup.reset(); // Call scripting::host::dtor.
            //});
        }
        // scripting::host: .
        template<class P>
        void update(P api_proc)
        {
            if (active)
            {
                auto lock = boss.bell::sync();
                api_proc();
            }
        }
        // scripting::host: Write client data.
        template<bool Echo = true>
        void write(text data)
        {
            if (!engine) return;
            if constexpr (Echo)
            {
                log(ansi::fgc(yellowlt).add(data).nil());
            }
            engine->write(data + '\n');
        }
        // scripting::host: Start a new process.
        void runapp(eccc cfg)
        {
            if (!engine) return;
            appcfg = cfg;
            if (!engine->connected())
            {
                engine->runapp(appcfg, [&](auto utf8_shadow){ ondata(utf8_shadow); },
                                       [&](auto code, auto msg){ onexit(code, msg); });
            }
        }
        void shut()
        {
            active = faux;
            if (!engine) return;
            if (engine->connected()) engine->shut();
        }

        host(ui::host& owner)
            :  skill{ owner },
              stream{ owner },
              active{ true  }
        {
            auto& config = owner.config;
            if (config.take(path::scripting, faux))
            {
                config.pushd(path::scripting);
                auto rse = config.take(attr::rse, ""s);
                config.cd(rse);
                auto env = config.take(attr::env, ""s);
                auto cwd = config.take(attr::cwd, ""s);
                auto cmd = config.take(attr::cmd, ""s);
                auto run = config.take(attr::run, ""s);
                auto tty = config.take(attr::tty, faux);
                auto win = os::ttysize;
                auto cfg = eccc{ .env = env,
                                 .cwd = cwd,
                                 .cmd = cmd,
                                 .win = win };
                if (tty)
                {
                    engine = ptr::shared<os::runspace::tty<scripting::host>>(*this);
                }
                else
                {
                    engine = ptr::shared<os::runspace::raw<scripting::host>>(*this);
                }
                runapp(cfg);
                //todo run integration script
                if (run.size()) write(run);
                config.popd();
            }
            owner.LISTEN(tier::release, scripting::events::invoke, script, skill::memo)
            {
                if (engine)
                {
                    write(script.cmd);
                    owner.bell::expire(tier::release);
                }
            };
        }
    };
}