// Copyright (c) Dmitry Sapozhnikov
// Licensed under the MIT license.

#pragma once

#if defined(__clang__) || defined(__APPLE__)
    #pragma clang diagnostic ignored "-Wunused-variable"
    #pragma clang diagnostic ignored "-Wunused-function"
#endif

#include "console.hpp"
#include "system.hpp"
#include "scripting.hpp"
#include "gui.hpp"

#include <fstream>

namespace netxs::app
{
    namespace fs = std::filesystem;
    using namespace std::placeholders;
    using namespace netxs::ui;
}

namespace netxs::app::shared
{
    static const auto version = "v0.9.99.61";
    static const auto repository = "https://github.com/directvt/vtm";
    static const auto usr_config = "~/.config/vtm/settings.xml"s;
    static const auto sys_config = "/etc/vtm/settings.xml"s;

    const auto closing_on_quit = [](auto& boss)
    {
        boss.LISTEN(tier::anycast, e2::form::proceed::quit::any, fast)
        {
            boss.base::riseup(tier::release, e2::form::proceed::quit::one, fast);
        };
    };
    const auto closing_by_gesture = [](auto& boss)
    {
        boss.LISTEN(tier::release, hids::events::mouse::button::click::leftright, gear)
        {
            auto backup = boss.This();
            boss.base::riseup(tier::release, e2::form::proceed::quit::one, true);
            gear.dismiss();
        };
        boss.LISTEN(tier::release, hids::events::mouse::button::click::middle, gear)
        {
            auto backup = boss.This();
            boss.base::riseup(tier::release, e2::form::proceed::quit::one, true);
            gear.dismiss();
        };
    };
    const auto scroll_bars = [](auto master)
    {
        auto sb = ui::fork::ctor();
        auto bt = sb->attach(slot::_1, ui::fork::ctor(axis::Y));
        auto hz = bt->attach(slot::_2, ui::grip<axis::X>::ctor(master));
        auto vt = sb->attach(slot::_2, ui::grip<axis::Y>::ctor(master));
        return sb;
    };
    const auto underlined_hz_scrollbar = [](auto scrlrail)
    {
        auto grip = ui::gripfx<axis::X, ui::drawfx::underline>::ctor(scrlrail)
            ->alignment({ snap::both, snap::tail })
            ->invoke([&](auto& boss)
            {
                boss.base::hidden = true;
                scrlrail->LISTEN(tier::release, e2::form::state::mouse, active, -, (grip_shadow = ptr::shadow(boss.This())))
                {
                    if (auto grip_ptr = grip_shadow.lock())
                    {
                        grip_ptr->base::hidden = !active;
                        grip_ptr->base::reflow();
                    }
                };
            });
        return grip;
    };
    const auto scroll_bars_term = [](auto master)
    {
        auto scroll_bars = ui::fork::ctor();
        auto scroll_head = scroll_bars->attach(slot::_1, ui::fork::ctor(axis::Y));
        auto hz = scroll_head->attach(slot::_1, ui::grip<axis::X>::ctor(master));
        auto vt = scroll_bars->attach(slot::_2, ui::grip<axis::Y>::ctor(master));
        return scroll_bars;
    };
    const auto set_title = [](base& boss, input::hids& gear, bias alignment = bias::left)
    {
        auto old_title = boss.base::riseup(tier::request, e2::form::prop::ui::header);
        gear.owner.base::riseup(tier::request, hids::events::clipboard, gear);
        auto& data = gear.board::cargo;
        if (data.utf8.empty())
        {
            log("%%Clipboard is empty or contains non-text data", prompt::desk);
        }
        else
        {
            if (utf::is_plain(data.utf8) || alignment != bias::left) // Reset aligning to the center if text is plain.
            {
                auto align = ansi::jet(alignment);
                boss.base::riseup(tier::preview, e2::form::prop::ui::header, align);
            }
            // Copy clipboard data to title.
            auto title = data.utf8;
            boss.base::riseup(tier::preview, e2::form::prop::ui::header, title);
            if (old_title.size()) // Copy old title to clipboard.
            {
                gear.set_clipboard(dot_00, old_title, mime::ansitext);
            }
        }
    };
    const auto base_kb_navigation = [](ui::pro::keybd& keybd, netxs::sptr<base> scroll, base& boss)
    {
        auto& scroll_inst = *scroll;
        auto esc_pressed = ptr::shared(faux);
        keybd.proc("WindowClose", [&, esc_pressed](hids& gear)
        {
            if (*esc_pressed)
            {
                boss.bell::signal(tier::anycast, e2::form::proceed::quit::one, true);
                gear.set_handled();
            }
        });
        keybd.proc("WindowClosePreview", [&, esc_pressed](hids& /*gear*/)
        {
            if (std::exchange(*esc_pressed, true) != *esc_pressed)
            {
                boss.bell::signal(tier::anycast, e2::form::state::keybd::command::close, *esc_pressed);
            }
        });
        keybd.proc("CancelWindowClose", [&, esc_pressed](hids& /*gear*/)
        {
            if (std::exchange(*esc_pressed, faux) != *esc_pressed)
            {
                boss.bell::signal(tier::anycast, e2::form::state::keybd::command::close, *esc_pressed);
            }
        });
        keybd.proc("ScrollPageUp"    , [&](hids& gear){ gear.set_handled(); scroll_inst.base::riseup(tier::preview, e2::form::upon::scroll::bypage::y, { .vector = { 0, 1 }}); });
        keybd.proc("ScrollPageDown"  , [&](hids& gear){ gear.set_handled(); scroll_inst.base::riseup(tier::preview, e2::form::upon::scroll::bypage::y, { .vector = { 0,-1 }}); });
        keybd.proc("ScrollLineUp"    , [&](hids& gear){ gear.set_handled(); scroll_inst.base::riseup(tier::preview, e2::form::upon::scroll::bystep::y, { .vector = { 0, 3 }}); });
        keybd.proc("ScrollLineDown"  , [&](hids& gear){ gear.set_handled(); scroll_inst.base::riseup(tier::preview, e2::form::upon::scroll::bystep::y, { .vector = { 0,-3 }}); });
        keybd.proc("ScrollCharLeft"  , [&](hids& gear){ gear.set_handled(); scroll_inst.base::riseup(tier::preview, e2::form::upon::scroll::bystep::x, { .vector = { 3, 0 }}); });
        keybd.proc("ScrollCharRight" , [&](hids& gear){ gear.set_handled(); scroll_inst.base::riseup(tier::preview, e2::form::upon::scroll::bystep::x, { .vector = {-3, 0 }}); });
        keybd.proc("ScrollTop"       , [&](hids& gear){ gear.set_handled(); scroll_inst.base::riseup(tier::preview, e2::form::upon::scroll::to_top::y); });
        keybd.proc("ScrollEnd"       , [&](hids& gear){ gear.set_handled(); scroll_inst.base::riseup(tier::preview, e2::form::upon::scroll::to_end::y); });
        keybd.proc("ToggleMaximize"  , [&](hids& gear){ gear.set_handled(); scroll_inst.bell::enqueue(boss.This(), [&, gear_id = gear.id](auto& /*boss*/){ if (auto gear_ptr = boss.bell::getref<hids>(gear_id)) scroll_inst.base::riseup(tier::preview, e2::form::size::enlarge::maximize,   *gear_ptr); }); }); // Refocus-related operations require execution outside of keyboard eves.
        keybd.proc("ToggleFullscreen", [&](hids& gear){ gear.set_handled(); scroll_inst.bell::enqueue(boss.This(), [&, gear_id = gear.id](auto& /*boss*/){ if (auto gear_ptr = boss.bell::getref<hids>(gear_id)) scroll_inst.base::riseup(tier::preview, e2::form::size::enlarge::fullscreen, *gear_ptr); }); });

        keybd.bind( "Esc", "DropAutoRepeat"    , true);
        keybd.bind( "Esc", "WindowClosePreview", true);
        keybd.bind("-Esc", "WindowClose"       , true);
        keybd.bind( "Any", "CancelWindowClose" , true);

        keybd.bind("PageUp"    , "ScrollPageUp"    );
        keybd.bind("PageDown"  , "ScrollPageDown"  );
        keybd.bind("UpArrow"   , "ScrollLineUp"    );
        keybd.bind("DownArrow" , "ScrollLineDown"  );
        keybd.bind("LeftArrow" , "ScrollCharLeft"  );
        keybd.bind("RightArrow", "ScrollCharRight" );
        keybd.bind("Home"      , "DropAutoRepeat"  );
        keybd.bind("Home"      , "ScrollTop"       );
        keybd.bind("End"       , "DropAutoRepeat"  );
        keybd.bind("End"       , "ScrollEnd"       );
        keybd.bind("F11"       , "DropAutoRepeat"  );
        keybd.bind("F11"       , "ToggleMaximize"  );
        keybd.bind("F12"       , "DropAutoRepeat"  );
        keybd.bind("F12"       , "ToggleFullscreen");
    };

    using builder_t = std::function<ui::sptr(eccc, xmls&)>;

    namespace win
    {
        using state = gui::window::state;

        namespace type
        {
            static const auto undefined = "undefined"s;
            static const auto normal    = "normal"s;
            static const auto minimized = "minimized"s;
            static const auto maximized = "maximized"s;
        }

        static auto options = std::unordered_map<text, si32>
           {{ type::undefined, state::normal    },
            { type::normal,    state::normal    },
            { type::minimized, state::minimized },
            { type::maximized, state::maximized }};
    }

    namespace menu
    {
        namespace attr
        {
            static constexpr auto menuitem = "menu/item";
            static constexpr auto type     = "type";
            static constexpr auto label    = "label";
            static constexpr auto tooltip  = "tooltip";
            static constexpr auto action   = "action";
            static constexpr auto data     = "data";
        }
        namespace type
        {
            static constexpr auto _counter = __COUNTER__ + 1;
            static constexpr auto Splitter = __COUNTER__ - _counter;
            static constexpr auto Command  = __COUNTER__ - _counter;
            static constexpr auto Option   = __COUNTER__ - _counter;
            static constexpr auto Repeat   = __COUNTER__ - _counter;
        }

        static auto type_options = std::unordered_map<text, si32>
            {{ "Splitter", menu::type::Splitter },
             { "Command",  menu::type::Command  },
             { "Option",   menu::type::Option   },
             { "Repeat",   menu::type::Repeat   }};

        struct item
        {
            struct look
            {
                text label{};
                text tooltip{};
                text data{};
                si32 value{};
                cell hover{};
                cell focus{};
            };

            using umap = std::unordered_map<ui64, si32>;
            using list = std::vector<look>;

            si32 type{};
            bool alive{};
            si32 taken{}; // Active label index.
            list views{};
            umap index{};

            void select(ui64 i)
            {
                auto iter = index.find(i);
                taken = iter == index.end() ? 0 : iter->second;
            }
            template<class Type = si32, class P>
            void reindex(P take)
            {
                auto count = (si32)(views.size());
                for (auto i = 0; i < count; i++)
                {
                    auto& l = views[i];
                    auto hash = ui64{};
                    if constexpr (std::is_same_v<Type, twod>)
                    {
                        auto twod_value = take(l.data);
                        hash = (ui64)((twod_value.y << 32) | twod_value.x);
                    }
                    else if constexpr (std::is_same_v<Type, text>)
                    {
                        auto text_value = take(l.data);
                        hash = (ui64)(qiew::hash{}(text_value));
                    }
                    else
                    {
                        l.value = (si32)(take(l.data));
                        hash = (ui64)(l.value);
                    }
                    index[hash] = i;
                }
            }
        };

        using action_map_t = std::unordered_map<text, std::function<void(ui::item&, menu::item&)>, qiew::hash, qiew::equal>;
        using link = std::tuple<item, std::function<void(ui::item&, item&)>>;
        using list = std::list<link>;

        static auto mini(bool autohide, bool slimsize, si32 custom, list menu_items) // Menu bar (shrinkable on right-click).
        {
            //auto highlight_color = skin::color(tone::highlight);
            auto danger_color    = skin::color(tone::danger);
            //auto action_color    = skin::color(tone::action);
            //auto warning_color   = skin::color(tone::warning);
            //auto c6 = action_color;
            //auto c3 = highlight_color;
            //auto c2 = warning_color;
            auto c1 = danger_color;
            auto macstyle = skin::globals().macstyle;
            auto menuveer = ui::veer::ctor();
            auto menufork = ui::fork::ctor()
                //todo
                //->alignment({ snap::both, snap::both }, { macstyle ? snap::head : snap::tail, snap::both })
                ->active();
            auto makeitem = [&](auto& config)
            {
                auto& props = std::get<0>(config);
                auto& setup = std::get<1>(config);
                auto& alive = props.alive;
                auto& label = props.views.front().label;
                auto& tooltip = props.views.front().tooltip;
                auto& hover = props.views.front().hover;
                auto button = ui::item::ctor(label)->drawdots();
                button->active(); // Always active for tooltips.
                if (alive)
                {
                    if (hover.clr()) button->shader(hover                , e2::form::state::hover);
                    else             button->shader(cell::shaders::xlight, e2::form::state::hover);
                }
                button->template plugin<pro::notes>(tooltip)
                    ->setpad({ 2, 2, !slimsize, !slimsize })
                    ->invoke([&](auto& boss) // Store shared ptr to the menu item config.
                    {
                        auto props_shadow = ptr::shared(std::move(props));
                        setup(boss, *props_shadow);
                        boss.LISTEN(tier::release, e2::dtor, v, -, (props_shadow))
                        {
                            props_shadow.reset();
                        };
                    });
                return button;
            };
            auto ctrlslot = macstyle ? slot::_1 : slot::_2;
            auto menuslot = macstyle ? slot::_2 : slot::_1;
            auto ctrllist = menufork->attach(ctrlslot, ui::list::ctor(axis::X));
            if (custom) // Apply a custom menu controls.
            {
                while (custom--)
                {
                    auto button = makeitem(menu_items.back());
                    ctrllist->attach<sort::reverse>(button);
                    menu_items.pop_back();
                }
            }
            else // Add standard menu controls.
            {
                auto control = std::vector<link>
                {
                    { menu::item{ menu::type::Command, true, 0, std::vector<menu::item::look>{{ .label = "—", .tooltip = " Minimize " }}},//, .hover = c2 }}}, //toto too funky
                    [](auto& boss, auto& /*item*/)
                    {
                        boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear)
                        {
                            boss.base::riseup(tier::release, e2::form::size::minimize, gear);
                            gear.dismiss();
                        };
                    }},
                    { menu::item{ menu::type::Command, true, 0, std::vector<menu::item::look>{{ .label = "□", .tooltip = " Maximize " }}},//, .hover = c6 }}},
                    [](auto& boss, auto& /*item*/)
                    {
                        boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear)
                        {
                            boss.base::riseup(tier::preview, e2::form::size::enlarge::maximize, gear);
                            gear.dismiss();
                        };
                    }},
                    { menu::item{ menu::type::Command, true, 0, std::vector<menu::item::look>{{ .label = "×", .tooltip = " Close ", .hover = c1 }}},
                    [c1](auto& boss, auto& /*item*/)
                    {
                        boss.template shader<tier::anycast>(cell::shaders::color(c1), e2::form::state::keybd::command::close);
                        boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear)
                        {
                            auto backup = boss.This();
                            boss.bell::signal(tier::anycast, e2::form::proceed::quit::one, faux); // fast=faux: Show closing process.
                            gear.dismiss();
                        };
                    }},
                };
                if (macstyle)
                {
                    ctrllist->attach(makeitem(control[2]));
                    ctrllist->attach(makeitem(control[0]));
                    ctrllist->attach(makeitem(control[1]));
                }
                else
                {
                    ctrllist->attach(makeitem(control[0]));
                    ctrllist->attach(makeitem(control[1]));
                    ctrllist->attach(makeitem(control[2]));
                }
            }
            auto scrlarea = menufork->attach(menuslot, ui::cake::ctor());
            auto scrlrail = scrlarea->attach(ui::rail::ctor(axes::X_only, axes::all));
            auto scrllist = scrlrail->attach(ui::list::ctor(axis::X));

            auto scrlcake = ui::cake::ctor();
            auto scrlhint = scrlcake->attach(underlined_hz_scrollbar(scrlrail));
            auto scrlgrip = scrlarea->attach(scrlcake);

            for (auto& body : menu_items)
            {
                scrllist->attach(makeitem(body));
            }

            auto menucake = menuveer->attach(ui::cake::ctor()->branch(menufork))
                ->invoke([&](auto& boss)
                {
                    auto slim_status = ptr::shared(slimsize);
                    boss.LISTEN(tier::anycast, e2::form::upon::resized, new_area, -, (slim_status))
                    {
                        if (!*slim_status)
                        {
                            auto height = boss.base::min_sz.y;
                            if (new_area.size.y < 3)
                            {
                                if (height != new_area.size.y)
                                {
                                    boss.base::limits({ -1, new_area.size.y }, { -1, new_area.size.y });
                                }
                            }
                            else if (height != 3)
                            {
                                boss.base::limits({ -1, 3 }, { -1, 3 });
                            }
                        }
                    };
                    boss.LISTEN(tier::anycast, e2::form::prop::ui::slimmenu, slim, -, (slim_status))
                    {
                        *slim_status = slim;
                        auto height = slim ? 1 : 3;
                        boss.base::limits({ -1, height }, { -1, height });
                        boss.reflow();
                    };
                });
            auto menutent = menuveer->attach(ui::mock::ctor()->limits({ -1,1 }, { -1,1 }));
            if (autohide == faux) menuveer->roll();
            menuveer->limits({ -1, slimsize ? 1 : 3 }, { -1, slimsize ? 1 : 3 })
                ->invoke([&](auto& boss)
                {
                    auto menutent_shadow = ptr::shadow(menutent);
                    auto menucake_shadow = ptr::shadow(menucake);
                    auto autohide_shadow = ptr::shared(autohide);
                    boss.LISTEN(tier::release, e2::form::state::mouse, hits, -, (menucake_shadow, autohide_shadow, menutent_shadow))
                    {
                        if (*autohide_shadow)
                        if (auto menucake = menucake_shadow.lock())
                        {
                            auto menu_visible = boss.back() != menucake;
                            if (!!hits == menu_visible)
                            {
                                boss.roll();
                                boss.reflow();
                                if (auto menutent = menutent_shadow.lock())
                                {
                                    menutent->bell::signal(tier::release, e2::form::state::visible, menu_visible);
                                }
                            }
                        }
                    };
                });

            return std::tuple{ menuveer, menutent, menucake };
        };
        const auto create = [](xmls& config, list menu_items)
        {
            auto autohide = config.take("menu/autohide", faux);
            auto slimsize = config.take("menu/slim"    , true);
            return mini(autohide, slimsize, 0, menu_items);
        };
        const auto load = [](xmls& config, action_map_t const& proc_map)
        {
            auto list = menu::list{};
            auto defs = menu::item::look{};
            auto menudata = config.list(menu::attr::menuitem);
            for (auto data_ptr : menudata)
            {
                auto item = menu::item{};
                auto& data = *data_ptr;
                auto action = data.take(menu::attr::action, ""s);
                item.type = data.take(menu::attr::type, menu::type::Command, type_options);
                defs.tooltip = data.take(menu::attr::tooltip, ""s);
                defs.data = data.take(menu::attr::data, ""s);
                item.alive = !action.empty() && item.type != menu::type::Splitter;
                for (auto label : data.list(menu::attr::label))
                {
                    item.views.push_back(
                    {
                        .label = label->take_value(),
                        .tooltip = label->take(menu::attr::tooltip, defs.tooltip),
                        .data = label->take(menu::attr::data, defs.data),
                    });
                }
                if (item.views.empty()) continue; // Menu item without label.
                auto setup = [action, &proc_map](ui::item& boss, menu::item& item)
                {
                    if (item.type == menu::type::Option)
                    {
                        boss.LISTEN(tier::release, hids::events::mouse::button::click::left, gear)
                        {
                            item.taken = (item.taken + 1) % item.views.size();
                        };
                    }
                    auto iter = proc_map.find(action);
                    if (iter != proc_map.end())
                    {
                        auto& initproc = iter->second;
                        initproc(boss, item);
                    }
                };
                list.push_back({ item, setup });
            }
            return menu::create(config, list);
        };
        const auto demo = [](xmls& config)
        {
            //auto highlight_color = skin::color(tone::highlight);
            //auto c3 = highlight_color;
            auto items = list
            {
                { item{ type::Command, true, 0, std::vector<item::look>{{ .label = ansi::und(true).add("F").nil().add("ile"), .tooltip = " File menu item " }}}, [&](auto& /*boss*/, auto& /*item*/){ }},
                { item{ type::Command, true, 0, std::vector<item::look>{{ .label = ansi::und(true).add("E").nil().add("dit"), .tooltip = " Edit menu item " }}}, [&](auto& /*boss*/, auto& /*item*/){ }},
                { item{ type::Command, true, 0, std::vector<item::look>{{ .label = ansi::und(true).add("V").nil().add("iew"), .tooltip = " View menu item " }}}, [&](auto& /*boss*/, auto& /*item*/){ }},
                { item{ type::Command, true, 0, std::vector<item::look>{{ .label = ansi::und(true).add("D").nil().add("ata"), .tooltip = " Data menu item " }}}, [&](auto& /*boss*/, auto& /*item*/){ }},
                { item{ type::Command, true, 0, std::vector<item::look>{{ .label = ansi::und(true).add("H").nil().add("elp"), .tooltip = " Help menu item " }}}, [&](auto& /*boss*/, auto& /*item*/){ }},
            };
            auto [menu, cover, menu_data] = create(config, items);
            return menu;
        };
    }
    namespace
    {
        auto& creator()
        {
            static auto creator = std::map<text, builder_t>{};
            return creator;
        }
    }
    auto& builder(text app_typename)
    {
        static builder_t empty =
        [&](eccc, xmls&) -> ui::sptr
        {
            auto window = ui::cake::ctor()
                ->plugin<pro::focus>()
                //->plugin<pro::track>()
                ->plugin<pro::acryl>()
                ->invoke([&](auto& boss)
                {
                    closing_on_quit(boss);
                    closing_by_gesture(boss);
                    boss.LISTEN(tier::release, e2::form::upon::vtree::attached, parent)
                    {
                        auto title = "error"s;
                        boss.base::riseup(tier::preview, e2::form::prop::ui::header, title);
                    };
                });
            auto msg = ui::post::ctor()
                ->colors(whitelt, argb{ 0x7F404040 })
                ->upload(ansi::fgc(yellowlt).mgl(4).mgr(4).wrp(wrap::off) +
                    "\n"
                    "\nUnsupported application type"
                    "\n" + ansi::nil().wrp(wrap::on) +
                    "\nOnly the following application types are supported:"
                    "\n" + ansi::nil().wrp(wrap::off).fgc(whitedk) +
                    "\n   type = vtty"
                    "\n   type = term"
                    "\n   type = dtvt"
                    "\n   type = dtty"
                    "\n   type = tile"
                    "\n   type = site"
                    "\n   type = info"
                    "\n"
                    "\n" + ansi::nil().wrp(wrap::on).fgc(whitelt)
                    .add(prompt::apps, "See logs for details."));
            auto placeholder = ui::cake::ctor()
                ->colors(whitelt, argb{ 0x7F404040 })
                ->attach(msg->alignment({ snap::head, snap::head }));
            window->attach(ui::rail::ctor())
                ->attach(placeholder)
                ->active();
            return window;
        };
        auto& map = creator();
        const auto it = map.find(app_typename);
        if (it == map.end())
        {
            log("%%Unknown app type - '%app_typename%'", prompt::apps, app_typename);
            return empty;
        }
        else return it->second;
    };
    namespace load
    {
        auto log_load(view src_path)
        {
            log("%%Loading settings from %path%...", prompt::apps, src_path);
        }
        auto load_from_file(xml::document& config, qiew file_path)
        {
            auto [config_path, config_path_str] = os::path::expand(file_path);
            if (!config_path.empty())
            {
                log_load(config_path_str);
                auto ec = std::error_code{};
                auto config_file = fs::directory_entry(config_path, ec);
                if (!ec && (config_file.is_regular_file(ec) || config_file.is_symlink(ec)))
                {
                    auto file = std::ifstream(config_file.path(), std::ios::binary | std::ios::in);
                    if (!file.seekg(0, std::ios::end).fail())
                    {
                        auto size = file.tellg();
                        auto buff = text((size_t)size, '\0');
                        file.seekg(0, std::ios::beg);
                        file.read(buff.data(), size);
                        config.load(buff, config_path_str);
                        log("%%Loaded %count% bytes", prompt::pads, size);
                        return true;
                    }
                }
                log(prompt::pads, "Not found");
            }
            return faux;
        }
        auto attach_file_list(xml::document& defcfg, xml::document& cfg)
        {
            if (cfg)
            {
                auto file_list = cfg.take<true>("/file");
                if (file_list.size())
                {
                    log("%%Update settings source files from %src%", prompt::apps, cfg.page.file);
                    for (auto& file : file_list) if (file && !file->base) log("%%%file%", prompt::pads, file->take_value());
                    defcfg.attach("/", file_list);
                }
            }
        }
        auto overlay_config(xml::document& defcfg, xml::document& cfg)
        {
            if (cfg)
            {
                auto config_data = cfg.take("/config/");
                if (config_data.size())
                {
                    log(prompt::pads, "Merging settings from ", cfg.page.file);
                    defcfg.overlay(config_data.front(), "");
                }
            }
        }
        auto settings(qiew cliopt, bool print = faux)
        {
            static auto defaults = utf::replace_all(
                #include "../../vtm.xml"
                , "\n\n", "\n");
            auto envopt = os::env::get("VTM_CONFIG");
            auto defcfg = xml::document{ defaults };
            auto envcfg = xml::document{};
            auto dvtcfg = xml::document{};
            auto clicfg = xml::document{};

            auto show_cfg = [&](auto& cfg){ if (print && cfg) log("%source%:\n%config%", cfg.page.file, cfg.page.show()); };

            if (envopt.size()) // Load settings from the environment variable (plain xml data or file path).
            {
                log_load("$VTM_CONFIG=" + envopt);
                if (envopt.starts_with("<")) // The case with a plain xml data.
                {
                    envcfg.load(envopt, "settings from the environment variable");
                }
                else
                {
                    load_from_file(envcfg, envopt); // The case with a file path.
                }
                show_cfg(envcfg);
            }

            if (os::dtvt::config.size()) // Load settings from the received directvt packet.
            {
                log_load("the received DirectVT packet");
                dvtcfg.load(os::dtvt::config, "dtvt");
                show_cfg(dvtcfg);
            }

            if (cliopt.size()) // Load settings from the specified '-c' cli option (memory mapped source, plain xml data or file path).
            {
                log_load(utf::concat("the specified '-c ", cliopt, "'"));
                if (cliopt.starts_with("<")) // The case with a plain xml data.
                {
                    clicfg.load(cliopt, "settings from the specified '-c' cli option");
                }
                else if (cliopt.starts_with(":")) // Receive configuration via memory mapping.
                {
                    cliopt.remove_prefix(1);
                    auto utf8 = os::process::memory::get(cliopt);
                    clicfg.load(utf8, cliopt);
                }
                else load_from_file(clicfg, cliopt); // The case with a file path.
                show_cfg(clicfg);
            }

            attach_file_list(defcfg, envcfg);
            attach_file_list(defcfg, dvtcfg);
            attach_file_list(defcfg, clicfg);

            auto config_sources = defcfg.take("/file");
            for (auto& file_rec : config_sources) if (file_rec && !file_rec->base) // Overlay configs from the specified sources if it is.
            {
                auto src_file = file_rec->take_value();
                auto src_conf = xml::document{};
                load_from_file(src_conf, src_file);
                show_cfg(src_conf);
                overlay_config(defcfg, src_conf);
            }

            overlay_config(defcfg, envcfg);
            overlay_config(defcfg, dvtcfg);
            overlay_config(defcfg, clicfg);

            auto resultant = xmls{ defcfg };
            return resultant;
        }
    }

    struct initialize
    {
        initialize(text app_typename, builder_t builder)
        {
            auto& map = creator();
            map[app_typename] = builder;
        }
    };
    struct gui_config_t
    {
        si32 winstate{};
        bool aliasing{};
        span blinking{};
        twod wincoord{};
        twod gridsize{};
        si32 cellsize{};
        std::list<text> fontlist;
        input::key::keybind_list_t hotkeys;
    };

    auto get_gui_config(xmls& config)
    {
        auto gui_config = gui_config_t{ .winstate = config.take("/config/gui/winstate", win::state::normal, app::shared::win::options),
                                        .aliasing = config.take("/config/gui/antialiasing", faux),
                                        .blinking = config.take("/config/gui/blinkrate", span{ 400ms }),
                                        .wincoord = config.take("/config/gui/wincoor", dot_mx),
                                        .gridsize = config.take("/config/gui/gridsize", dot_mx),
                                        .cellsize = std::clamp(config.take("/config/gui/cellheight", si32{ 20 }), 0, 256) };
        if (gui_config.cellsize == 0) gui_config.cellsize = 20;
        if (gui_config.gridsize.x == 0 || gui_config.gridsize.y == 0) gui_config.gridsize = dot_mx;
        auto recs = config.list("/config/gui/fonts/font");
        for (auto& f : recs)
        {
            //todo implement 'fonts/font/file' - font file path/url
            gui_config.fontlist.push_back(f->take_value());
        }
        gui_config.hotkeys = ui::pro::keybd::load(config, "gui");
        return gui_config;
    }
    void splice(xipc client, gui_config_t& gc)
    {
        if (os::dtvt::active || !(os::dtvt::vtmode & ui::console::gui)) os::tty::splice(client);
        else
        {
            os::dtvt::client = client;
            auto connect = [&]
            {
                auto event_domain = netxs::events::auth{};
                auto window = event_domain.create<gui::window>(event_domain, gc.fontlist, gc.cellsize, gc.aliasing, gc.blinking, dot_21, gc.hotkeys);
                window->connect(gc.winstate, gc.wincoord, gc.gridsize);
            };
            if (os::stdout_fd != os::invalid_fd)
            {
                auto runcmd = directvt::binary::command{};
                auto readln = os::tty::readline([&](auto line){ runcmd.send(client, line); }, [&]{ if (client) client->shut(); });
                connect();
                readln.stop();
            }
            else
            {
                connect();
            }
        }
    }
    void start(text cmd, text aclass, xmls& config)
    {
        auto [client, server] = os::ipc::xlink();
        auto config_lock = ui::tui_domain().unique_lock(); // Sync multithreaded access to config.
        auto gui_config = app::shared::get_gui_config(config);
        auto thread = std::thread{ [&, &client = client] //todo clang 15.0.0 still disallows capturing structured bindings (wait for clang 16.0.0)
        {
            app::shared::splice(client, gui_config);
        }};
        auto domain = ui::host::ctor(server, config)->plugin<scripting::host>();
        auto appcfg = eccc{ .cmd = cmd };
        auto applet = app::shared::builder(aclass)(appcfg, config);
        config_lock.unlock();
        domain->invite(server, applet, os::dtvt::vtmode, os::dtvt::gridsz);
        domain->stop();
        server->shut();
        client->shut(); //todo revise deadlock when closing term inside desktop by X close button.
        thread.join();
    }
}
