// Copyright (c) NetXS Group.
// Licensed under the MIT license.

#pragma once

#undef GetGlyphIndices
#include <DWrite_2.h>
#include <memory_resource>
#pragma comment(lib, "Gdi32")
#pragma comment(lib, "dwrite")

#define ok2(...) [&](){ auto hr = __VA_ARGS__; if (hr != S_OK) log(utf::to_hex(hr), " ", #__VA_ARGS__); return hr == S_OK; }()

namespace netxs::gui
{
    using namespace input;

    //test strings
    auto canvas_text = ansi::wrp(wrap::on).itc(true).fgc(tint::cyanlt).add("\nvtm GUI frontend").itc(faux).fgc(tint::purered).bld(true).add(" is currently under development.").nil()
        .fgc(tint::cyanlt).add(" You can try it on any versions/editions of Windows platforms starting from Windows 8.1"
                               " (with colored emoji!), including Windows Server Core. 😀😬😁😂😃😄😅😆 👌🐞😎👪.\n\n")
        .fgc(tint::greenlt).add("Press Esc or Right click to close.\n");
    auto header_text = ansi::fgc(tint::purewhite).add("Windows Command Prompt - 😎 - C:\\Windows\\System32\\").nop().pushsgr().chx(0).jet(bias::right).fgc(argb::vt256[4]).add("\0▀"sv).nop().popsgr();
    auto footer_text = ansi::wrp(wrap::on).jet(bias::right).fgc(tint::purewhite).add("4/40000 80:25");
    auto canvas_page = ui::page{ canvas_text + canvas_text + canvas_text + canvas_text + canvas_text};
    auto header_page = ui::page{ header_text };
    auto footer_page = ui::page{ footer_text };

    struct font
    {
        struct style
        {
            static constexpr auto normal      = 0;
            static constexpr auto italic      = 1;
            static constexpr auto bold        = 2;
            static constexpr auto bold_italic = bold | italic;
        };
        struct fontcat
        {
            static constexpr auto valid      = 1 << 0;
            static constexpr auto monospaced = 1 << 1;
            static constexpr auto color      = 1 << 2;
            static constexpr auto loaded     = 1 << 3;
        };
        struct typeface
        {
            std::vector<IDWriteFontFace1*>    fontface;
            DWRITE_FONT_METRICS               metrics{};
            si32                              baseline{};
            si32                              emheight{};
            twod                              facesz; // Typeface cell size.
            ui32                              index{ ~0u };

            auto load(auto& faceinst, auto barefont, auto weight, auto stretch, auto style)
            {
                auto fontfile = (IDWriteFont2*)nullptr;
                barefont->GetFirstMatchingFont(weight, stretch, style, (IDWriteFont**)&fontfile);
                fontfile->CreateFontFace((IDWriteFontFace**)&faceinst);
                if (!metrics.designUnitsPerEm)
                {
                    faceinst->GetMetrics(&metrics);
                    emheight = metrics.ascent + metrics.descent;
                    facesz.x = metrics.designUnitsPerEm / 2;
                    facesz.y = emheight + metrics.lineGap;
                    baseline = metrics.ascent + metrics.lineGap / 2;
                    // formats 8, 10 and 12 with 32-bit encoding
                    // format 13 - last-resort
                    // format 14 for Unicode variation sequences
                    struct cmap_table
                    {
                        enum platforms
                        {
                            Unicode   = 0, // Various
                            Macintosh = 1, // Script manager code
                            ISO       = 2, // ISO encoding
                            Windows   = 3, // Windows encoding
                            Custom    = 4, // Custom encoding
                        };
                        enum encodings
                        {
                            Symbol            = 0, // Symbols
                            Windows_1_0       = 1, // Unicode BMP
                            Unicode_2_0       = 3, // Unicode 2.0 and onwards semantics, Unicode BMP only
                            Unicode_2_0f      = 4, // Unicode 2.0 and onwards semantics, Unicode full repertoire
                            Unicode_Variation = 5, // Unicode Variation Sequences—for use with subtable format 14
                            Unicode_full      = 6, // Unicode full repertoire—for use with subtable format 13
                            Windows_2_0       = 10,// Unicode full repertoire
                        };
                        struct rect
                        {
                            ui16 platformID;
                            ui16 encodingID;
                            ui32 subtableOffset; // Byte offset from beginning of table to the subtable for this encoding.
                        };
                        ui16 version;
                        ui16 numTables;
                        //rect records[];
                    };
                    struct colr_table
                    {
                        struct baseGlyphRecord
                        {
                            ui16 glyphID;         // Glyph ID of the base glyph.
                            ui16 firstLayerIndex; // Index (base 0) into the layerRecords array.
                            ui16 numLayers;       // Number of color layers associated with this glyph.
                        };
                        struct layerRecord
                        {
                            ui16 glyphID;      // Glyph ID of the glyph used for a given layer.
                            ui16 paletteIndex; // Index (base 0) for a palette entry in the CPAL table.
                        };
                        ui16 version;                   // Table version number—set to 0.
                        ui16 numBaseGlyphRecords;       // Number of base glyphs.
                        ui32 baseGlyphRecordsOffset;    // Offset to baseGlyphRecords array.
                        ui32 layerRecordsOffset;        // Offset to layerRecords array.
                        ui16 numLayerRecords;           // Number of Layer records.
                        //baseGlyphRecord baseGlyphRecs[];
                        //layerRecord     layersRecs[];
                    };

                    //auto cmap_data = (void const*)nullptr;
                    //auto cmap_size = ui32{};
                    //auto cmap_ctx = (void*)nullptr;
                    //auto exists = BOOL{};
                    //fontface->TryGetFontTable(DWRITE_MAKE_OPENTYPE_TAG('c','m','a','p'), &cmap_data, &cmap_size, &cmap_ctx, &exists);
                    // 1. cmap: codepoints -> indices
                    // 2. GSUB: indices -> indices
                    // 3. BASE: take font-wise metrics
                    // 4. GPOS: glyph positions
                    // 5. COLR+CPAL: multicolored glyphs (version 0)
                }
                fontfile->Release();
            }
            void load(IDWriteFontFamily* barefont)
            {
                fontface.resize(4);
                load(fontface[style::normal     ], barefont, DWRITE_FONT_WEIGHT_NORMAL,    DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
                load(fontface[style::italic     ], barefont, DWRITE_FONT_WEIGHT_NORMAL,    DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC);
                load(fontface[style::bold       ], barefont, DWRITE_FONT_WEIGHT_DEMI_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL);
                load(fontface[style::bold_italic], barefont, DWRITE_FONT_WEIGHT_DEMI_BOLD, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_ITALIC);
                auto names = (IDWriteLocalizedStrings*)nullptr;
                barefont->GetFamilyNames(&names);
                auto buff = wide(100, 0);
                names->GetString(0, buff.data(), (ui32)buff.size());
                log("%%Using font '%fontname%'.", prompt::gui, utf::to_utf(buff.data()));
                names->Release();
            }

            typeface(typeface&&) = default;
            typeface(IDWriteFontFamily* barefont, ui32 index)
                : index{ index }
            {
                load(barefont);
            }
            typeface(view family_utf8, IDWriteFontCollection* fontlist)
            {
                auto found = BOOL{};   
                auto family_utf16 = utf::to_utf(family_utf8);
                fontlist->FindFamilyName(family_utf16.data(), &index, &found);
                if (found)
                {
                    auto barefont = (IDWriteFontFamily*)nullptr;
                    fontlist->GetFontFamily(index, &barefont);
                    load(barefont);
                    barefont->Release();                    
                }
                else log("%%Font '%fontname%' is not installed in the system.", prompt::gui, family_utf8);
            }
            ~typeface()
            {
                for (auto f : fontface) if (f) f->Release();
            }
            explicit operator bool () { return index != ~0u; }
        };

        IDWriteFactory2*       factory2;
        IDWriteFontCollection* fontlist;
        typeface               basefont;
        std::vector<byte>      fontstat;
        std::vector<typeface>  fallback;

        font(view family_name)
            : factory2{ (IDWriteFactory2*)[]{ auto f = (IUnknown*)nullptr; ::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), &f); return f; }() },
              fontlist{ [&]{ auto c = (IDWriteFontCollection*)nullptr; factory2->GetSystemFontCollection(&c, TRUE); return c; }() },
              basefont{ family_name, fontlist },
              fontstat(fontlist->GetFontFamilyCount(), 0)
        {
            if (basefont) fontstat[basefont.index] |= fontcat::loaded;
            for (auto i = 0u; i < fontstat.size(); i++)
            {
                auto barefont = (IDWriteFontFamily*)nullptr;
                auto fontfile = (IDWriteFont2*)nullptr;
                fontlist->GetFontFamily(i, &barefont);
                barefont->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, (IDWriteFont**)&fontfile);
                if (fontfile)
                {
                                                      fontstat[i] |= fontcat::valid;
                    if (fontfile->IsColorFont())      fontstat[i] |= fontcat::color;
                    if (fontfile->IsMonospacedFont()) fontstat[i] |= fontcat::monospaced;
                }
                else continue;
                fontfile->Release();
            }
        }
        ~font()
        {
            if (fontlist) fontlist->Release();
            if (factory2) factory2->Release();
        }
        auto& take_font(utfx codepoint)
        {
            auto hittest = [&](auto& fontface)
            {
                if (!fontface) return faux;
                auto glyphindex = ui16{};
                fontface->GetGlyphIndices(&codepoint, 1, &glyphindex);
                return !!glyphindex;
            };
            if (hittest(basefont.fontface[0])) return basefont;
            else
            {
                for (auto& f : fallback) if (hittest(f.fontface[0])) return f;
                auto try_font = [&](auto i)
                {
                    auto barefont = (IDWriteFontFamily*)nullptr;
                    auto fontfile = (IDWriteFont2*)nullptr;
                    fontlist->GetFontFamily(i, &barefont);
                    barefont->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, (IDWriteFont**)&fontfile);
                    auto fontface = (IDWriteFontFace*)nullptr;
                    fontfile->CreateFontFace(&fontface);
                    auto hit = hittest(fontface);
                    if (hit)
                    {
                        fontstat[i] |= fontcat::loaded;
                        fallback.emplace_back(barefont, i);
                    }
                    fontface->Release();
                    fontfile->Release();
                    barefont->Release();
                    return hit;
                };
                auto try_font_by_cat = [&](auto category)
                {
                    for (auto i = 0u; i < fontstat.size(); i++)
                    {
                        if (fontstat[i] == category && try_font(i)) return true;
                    }
                    return faux;
                };
                if (try_font_by_cat(fontcat::valid | fontcat::monospaced | fontcat::color)) return fallback.back();
                if (try_font_by_cat(fontcat::valid | fontcat::monospaced))                  return fallback.back();
                if (try_font_by_cat(fontcat::valid | fontcat::color))                       return fallback.back();
                if (try_font_by_cat(fontcat::valid))                                        return fallback.back();
            }
            return basefont;
        }
    };

    struct glyf
    {
        using irgb = netxs::irgb<si32>;
        using vect = std::pmr::vector<byte>;
        struct alpha_mask
        {
            static constexpr auto undef = 0;
            static constexpr auto alpha = 1; // Grayscale AA glyph alphamix. byte-based. fx: pixel = blend(pixel, fgc, byte).
            static constexpr auto color = 2; // irgb-colored glyph colormix. irgb-based. fx: pixel = blend(blend(pixel, irgb.alpha(irgb.chan.a & 0xff)), fgc, irgb.chan.a >> 8).

            vect bits; // Contains: type=alpha: bytes [0-255]; type=color: irgb<si32>.
            rect area;
            si32 type;
            alpha_mask(auto& pool)
                : bits{ &pool },
                  type{ undef }
            { }
        };
        struct color_layer
        {
            vect bits;
            rect area;
            irgb fill;
            color_layer(auto& pool)
                : bits{ &pool }
            { }
        };

        using gmap = std::unordered_map<ui64, alpha_mask>;

        font& fcache; // glyf: Font cache.
        twod  cellsz; // glyf: Narrow glyph black-box size in pixels.
        bool  aamode; // glyf: Enable AA.
        gmap  glyphs; // glyf: Glyph map.
        std::pmr::unsynchronized_pool_resource buffer_pool;
        std::pmr::monotonic_buffer_resource    mono_buffer;
        std::pmr::vector<ui32>                 cpbuff; // : Temp buffer for glyph codepoints.
        std::pmr::vector<ui16>                 gindex; // : Temp buffer for glyph indices.

        glyf(font& fcache, twod cellsz, bool aamode)
            : fcache{ fcache },
              cellsz{ cellsz },
              aamode{ aamode },
              cpbuff{ &buffer_pool },
              gindex{ &buffer_pool }
        { }
        void rasterize(alpha_mask& glyph_mask, cell const& c)
        {
            glyph_mask.type = alpha_mask::alpha;
            if (c.wdt() == 0) return;
            auto code_iter = utf::cpit{ c.txt() };
            cpbuff.resize(0);
            while (code_iter) cpbuff.push_back(code_iter.next().cdpoint);
            if (cpbuff.empty()) return;

            auto format = font::style::normal;
            if (c.itc()) format |= font::style::italic;
            if (c.bld()) format |= font::style::bold;
            auto& f = fcache.take_font(cpbuff.front());
            auto font_face = f.fontface[format];
            if (!font_face) return;

            gindex.resize(cpbuff.size());
            auto hr = font_face->GetGlyphIndices(cpbuff.data(), (ui32)cpbuff.size(), gindex.data());
            auto transform = std::min((fp32)cellsz.x / f.facesz.x, (fp32)cellsz.y / f.facesz.y);
            auto base_line = fp2d{ 0, f.baseline * transform };
            auto font_size = f.emheight * transform * 0.75f; // CreateGlyphRunAnalysis2 operates with 72dpi, so 72/96 = 0.75.
            auto glyph_run = DWRITE_GLYPH_RUN{ .fontFace     = font_face,
                                               .fontEmSize   = font_size,
                                               .glyphCount   = (ui32)gindex.size(),
                                               .glyphIndices = gindex.data() };
            auto colored_glyphs = (IDWriteColorGlyphRunEnumerator*)nullptr;
            auto measuring_mode = DWRITE_MEASURING_MODE_NATURAL;
            hr = fcache.factory2->TranslateColorGlyphRun(0, 0, &glyph_run, nullptr, measuring_mode, nullptr, 0, &colored_glyphs);
            auto rendering_mode = aamode || colored_glyphs ? DWRITE_RENDERING_MODE_NATURAL : DWRITE_RENDERING_MODE_ALIASED;
            auto pixel_fit_mode = DWRITE_GRID_FIT_MODE_ENABLED; //DWRITE_GRID_FIT_MODE_DEFAULT
            auto aaliasing_mode = DWRITE_TEXT_ANTIALIAS_MODE_GRAYSCALE; //DWRITE_TEXT_ANTIALIAS_MODE_CLEARTYPE
            auto create_texture = [&](auto& run, auto& mask)
            {
                auto r = RECT{};
                auto rasterizer = (IDWriteGlyphRunAnalysis*)nullptr;
                fcache.factory2->CreateGlyphRunAnalysis(&run, nullptr, rendering_mode, measuring_mode, pixel_fit_mode, aaliasing_mode, base_line.x, base_line.y, &rasterizer);
                hr = rasterizer->GetAlphaTextureBounds(DWRITE_TEXTURE_ALIASED_1x1, &r);
                mask.area = {{ r.left, r.top }, { r.right - r.left, r.bottom - r.top }};
                mask.bits.resize(mask.area.size.x * mask.area.size.y);
                hr = rasterizer->CreateAlphaTexture(DWRITE_TEXTURE_ALIASED_1x1, &r, mask.bits.data(), (ui32)mask.bits.size());
                rasterizer->Release();
            };
            if (colored_glyphs)
            {
                glyph_mask.bits.clear();
                glyph_mask.type = alpha_mask::color;
                auto exist = BOOL{ true };
                auto layer = (DWRITE_COLOR_GLYPH_RUN const*)nullptr;
                auto masks = std::pmr::vector<color_layer>{ &buffer_pool }; // Minimize allocations.
                while (colored_glyphs->MoveNext(&exist), exist && S_OK == colored_glyphs->GetCurrentRun(&layer))
                {
                    auto& m = masks.emplace_back(buffer_pool);
                    create_texture(layer->glyphRun, m);
                    m.fill = layer->paletteIndex != -1 ? argb{ layer->runColor }.swap_rb() : argb{}; // runColor.a could be nan != 0.
                }
                glyph_mask.area = {};
                for (auto& m : masks) glyph_mask.area |= m.area;
                auto l = glyph_mask.area.size.x * glyph_mask.area.size.y;
                glyph_mask.bits.resize(l * sizeof(irgb));
                auto raster = netxs::raster{ std::span{ (irgb*)glyph_mask.bits.data(), (size_t)l }, glyph_mask.area };
                for (auto& m : masks)
                {
                    auto alpha_mask = netxs::raster{ m.bits, m.area };
                    if (m.fill.a) // Fixed color.
                    {
                        netxs::onbody(raster, alpha_mask, [c = m.fill](irgb& dst, byte& alpha)
                        {
                            if (dst.a & 0xFF'00) // Update fgc layer if it is.
                            {
                                auto fgc_alpha = ((dst.a >> 8) * alpha) & 0xFF'00;
                                dst.a &= 0xFF; 
                                dst.blend_nonpma(c, alpha);
                                dst.a |= fgc_alpha;
                            }
                            else dst.blend_nonpma(c, alpha);
                        });
                    }
                    else // Foreground color unknown in advance. Side-effect: fully transparent glyph layers will be colored with the fgc color.
                    {
                        netxs::onbody(raster, alpha_mask, [](irgb& dst, byte& alpha)
                        {
                            if (alpha == 255) dst.a |= 0xFF'00;
                            else if (alpha != 0)
                            {
                                auto a = alpha + (((256 - alpha) * (dst.a >> 8)) >> 8);
                                dst.a = (dst.a & 0xFF) | (a << 8);
                            }
                        });
                    }
                }
                colored_glyphs->Release();
            }
            else if (hr == DWRITE_E_NOCOLOR) create_texture(glyph_run, glyph_mask);
        }
        void draw_cell(auto& canvas, twod coor, cell const& c)
        {
            auto placeholder = rect{ coor, cellsz };
            if (c.und())
            {

            }
            if (c.stk())
            {

            }
            if (c.ovr())
            {

            }
            if (c.inv())
            {

            }
            auto w = c.wdt();
            if (w == 0) return;
            auto token = c.tkn() & ~3;
            if (c.itc()) token |= font::style::italic;
            if (c.bld()) token |= font::style::bold;
            auto iter = glyphs.find(token);
            if (iter == glyphs.end())
            {
                iter = glyphs.emplace(token, mono_buffer).first;
                rasterize(iter->second, c);
            }
            auto& glyph_mask = iter->second;
            if (!glyph_mask.area) return;
            canvas.clip(placeholder);
            auto box = glyph_mask.area.shift(w != 3 ? coor : coor - twod{ cellsz.x, 0 });
            if (glyph_mask.type == alpha_mask::color)
            {
                auto fx = [fgc = c.fgc()](argb& dst, irgb src)
                {
                    if (src.a & 0xFF'00)
                    {
                        dst.blend_pma({ src.r, src.g, src.b, src.a & 0xFF });
                        dst.blend_nonpma(fgc, src.a >> 8);
                    }
                    else dst.blend_pma(src);
                };
                auto raster = netxs::raster{ std::span{ (irgb*)glyph_mask.bits.data(), (size_t)glyph_mask.area.length() }, box };
                netxs::onclip(canvas, raster, fx);
            }
            else
            {
                auto fx = [fgc = c.fgc()](argb& dst, byte src){ dst.blend_nonpma(fgc, src); };
                auto raster = netxs::raster{ glyph_mask.bits, box };
                netxs::onclip(canvas, raster, fx);
            }
        }
        void fill_grid(auto& canvas, rect region, auto& grid_cells)
        {
            canvas.step(-region.coor);
            auto coor = dot_00;
            for (auto& c : grid_cells)
            {
                draw_cell(canvas, coor, c);
                coor.x += cellsz.x;
                if (coor.x >= region.size.x)
                {
                    coor.x = 0;
                    coor.y += cellsz.y;
                    if (coor.y >= region.size.y) break;
                }
            }
            canvas.step(region.coor);
        }
    };

    struct surface
    {
        using bits = netxs::raster<std::span<argb>, rect>;

        HDC   hdc;
        HWND hWnd;
        bool sync;
        rect prev;
        rect area;
        twod size;
        bits data;

        surface(surface const&) = default;
        surface(surface&&) = default;
        surface(HWND hWnd)
            :  hdc{ ::CreateCompatibleDC(NULL)}, // Only current thread owns hdc.
              hWnd{ hWnd },
              sync{ faux },
              prev{ .coor = dot_mx },
              area{ dot_00, dot_00 },
              size{ dot_00 }
        { }
        void reset() // We don't use custom copy/move ctors.
        {
            if (hdc) ::DeleteDC(hdc);
        }
        void set_dpi(auto /*dpi*/) // We are do not rely on dpi. Users should configure all metrics in pixels.
        { }
        auto canvas(bool wipe = faux)
        {
            if (area)
            {
                if (area.size != size)
                {
                    auto ptr = (void*)nullptr;
                    auto bmi = BITMAPINFO{ .bmiHeader = { .biSize        = sizeof(BITMAPINFOHEADER),
                                                          .biWidth       = area.size.x,
                                                          .biHeight      = -area.size.y,
                                                          .biPlanes      = 1,
                                                          .biBitCount    = 32,
                                                          .biCompression = BI_RGB }};
                    if (auto hbm = ::CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &ptr, 0, 0)) // 0.050 ms
                    {
                        ::DeleteObject(::SelectObject(hdc, hbm));
                        wipe = faux;
                        size = area.size;
                        data = bits{ std::span<argb>{ (argb*)ptr, (sz_t)size.x * size.y }, rect{ area.coor, size }};
                    }
                    else log("%%Compatible bitmap creation error: %ec%", prompt::gui, ::GetLastError());
                }
                if (wipe) std::memset(data.data(), 0, (sz_t)size.x * size.y * sizeof(argb));
                sync = faux;
            }
            data.move(area.coor);
            return data;
        }
        void present()
        {
            if (sync || !hdc) return;
            //netxs::misc::fill(canvas(), [](argb& c){ c.pma(); });
            static auto blend_props = BLENDFUNCTION{ .BlendOp = AC_SRC_OVER, .SourceConstantAlpha = 255, .AlphaFormat = AC_SRC_ALPHA };
            auto scr_coor = POINT{};
            auto old_size =  SIZE{ prev.size.x, prev.size.y };
            auto old_coor = POINT{ prev.coor.x, prev.coor.y };
            auto win_size =  SIZE{      size.x,      size.y };
            auto win_coor = POINT{ area.coor.x, area.coor.y };
            //auto sized = prev.size(     size);
            auto moved = prev.coor(area.coor);
            auto rc = ::UpdateLayeredWindow(hWnd,   // 1.5 ms (syscall, copy bitmap to hardware)
                                            HDC{},                       // No color palette matching.  HDC hdcDst,
                                            moved ? &win_coor : nullptr, // POINT         *pptDst,
                                            &win_size,                   // SIZE          *psize,
                                            hdc,                         // HDC           hdcSrc,
                                            &scr_coor,                   // POINT         *pptSrc,
                                            {},                          // COLORREF      crKey,
                                            &blend_props,                // BLENDFUNCTION *pblend,
                                            ULW_ALPHA);                  // DWORD         dwFlags
            if (!rc) log("%%UpdateLayeredWindow returns unexpected result ", prompt::gui, rc);
            sync = true;
        }
    };

    struct manager
    {
        using wins = std::vector<surface>;

        enum bttn
        {
            left   = 1 << 0,
            right  = 1 << 1,
            middle = 1 << 2,
        };

        font fcache; // manager: Font cache.
        glyf gcache; // manager: Glyph cache.
        bool isfine; // manager: All is ok.
        wins layers; // manager: ARGB layers.
        wide locale; // manager: Locale.

        manager(view font_name_utf8, twod cellsz, bool antialiasing)
            : fcache{ font_name_utf8 },
              gcache{ fcache, cellsz, antialiasing },
              isfine{ true },
              locale(LOCALE_NAME_MAX_LENGTH, '\0')
        {
            set_dpi_awareness();
            if (!::GetUserDefaultLocaleName(locale.data(), (si32)locale.size())) // Return locale length or 0.
            {
                locale = L"en-US";
                log("%%Using default locale 'en-US'.", prompt::gui);
            }
        }
        ~manager()
        {
            for (auto& w : layers) w.reset();
        }
        auto& operator [] (si32 layer) { return layers[layer]; }
        explicit operator bool () const { return isfine; }

        void set_dpi_awareness()
        {
            auto proc = (LONG(_stdcall *)(si32))::GetProcAddress(::GetModuleHandleA("user32.dll"), "SetProcessDpiAwarenessInternal");
            if (proc)
            {
                auto hr = proc(2/*PROCESS_PER_MONITOR_DPI_AWARE*/);
                if (hr != S_OK || hr != E_ACCESSDENIED) log("%%Set DPI awareness failed %hr% %ec%", prompt::gui, utf::to_hex(hr), ::GetLastError());
            }
        }
        auto set_dpi(auto new_dpi)
        {
            //for (auto& w : layers) w.set_dpi(new_dpi);
            log("%%DPI changed to %dpi%", prompt::gui, new_dpi);
        }
        auto moveby(twod delta)
        {
            for (auto& w : layers) w.area.coor += delta;
        }
        template<bool JustMove = faux>
        void present()
        {
            if constexpr (JustMove)
            {
                auto lock = ::BeginDeferWindowPos((si32)layers.size());
                for (auto& w : layers)
                {
                    lock = ::DeferWindowPos(lock, w.hWnd, 0, w.area.coor.x, w.area.coor.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
                    if (!lock) { log("%%DeferWindowPos returns unexpected result: %ec%", prompt::gui, ::GetLastError()); }
                    w.prev.coor = w.area.coor;
                }
                ::EndDeferWindowPos(lock);
            }
            else for (auto& w : layers) w.present(); // 3.000 ms
        }
        void dispatch()
        {
            auto msg = MSG{};
            while (::GetMessageW(&msg, 0, 0, 0) > 0)
            {
                ::DispatchMessageW(&msg);
            }
        }
        void shown_event(bool shown, arch reason)
        {
            log(shown ? "shown" : "hidden", " ", reason == SW_OTHERUNZOOM   ? "The window is being uncovered because a maximize window was restored or minimized."
                                               : reason == SW_OTHERZOOM     ? "The window is being covered by another window that has been maximized."
                                               : reason == SW_PARENTCLOSING ? "The window's owner window is being minimized."
                                               : reason == SW_PARENTOPENING ? "The window's owner window is being restored."
                                                                            : "unknown reason");
        }
        void show(si32 win_state)
        {
            if (win_state == 0 || win_state == 2) //todo fullscreen mode (=2). 0 - normal, 1 - minimized, 2 - fullscreen
            {
                auto mode = SW_SHOWNORMAL;
                for (auto& w : layers) { ::ShowWindow(w.hWnd, mode); }
            }
        }
        void mouse_capture()
        {
            if (!layers.empty()) ::SetCapture(layers.front().hWnd);
        }
        void mouse_release()
        {
            ::ReleaseCapture();
        }
        void close()
        {
            if (!layers.empty()) ::SendMessageW(layers.front().hWnd, WM_CLOSE, NULL, NULL);
        }
        void activate()
        {
            log("activated");
            if (!layers.empty()) ::SetActiveWindow(layers.front().hWnd);
        }
        void state_event(bool activated, bool minimized)
        {
            log(activated ? "activated" : "deactivated", " ", minimized ? "minimized" : "restored");
        }

        virtual void update() = 0;
        virtual void mouse_shift(twod coord) = 0;
        virtual void focus_event(bool state) = 0;
        virtual void mouse_press(si32 index, bool pressed) = 0;
        virtual void mouse_wheel(si32 delta, bool hzwheel) = 0;
        virtual void keybd_press(arch vkey, arch lParam) = 0;

        auto add(manager* host_ptr = nullptr)
        {
            auto window_proc = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
            {
                //log("\tmsW=", utf::to_hex(msg), " wP=", utf::to_hex(wParam), " lP=", utf::to_hex(lParam), " hwnd=", utf::to_hex(hWnd));
                //auto layer = (si32)::GetWindowLongPtrW(hWnd, sizeof(LONG_PTR));
                auto w = (manager*)::GetWindowLongPtrW(hWnd, GWLP_USERDATA);
                if (!w) return ::DefWindowProcW(hWnd, msg, wParam, lParam);
                auto stat = LRESULT{};
                static auto hi = [](auto n){ return (si32)(si16)((n >> 16) & 0xffff); };
                static auto lo = [](auto n){ return (si32)(si16)((n >> 0 ) & 0xffff); };
                static auto hover_win = testy<HWND>{};
                static auto hover_rec = TRACKMOUSEEVENT{ .cbSize = sizeof(TRACKMOUSEEVENT), .dwFlags = TME_LEAVE, .dwHoverTime = HOVER_DEFAULT };
                static auto h = 0;
                static auto f = 0;
                switch (msg)
                {
                    case WM_MOUSEMOVE:
                        if (hover_win(hWnd)) ::TrackMouseEvent((++h, hover_rec.hwndTrack = hWnd, &hover_rec));
                        if (auto r = RECT{}; ::GetWindowRect(hWnd, &r)) w->mouse_shift({ r.left + lo(lParam), r.top + hi(lParam) });
                        break;
                    case WM_MOUSELEAVE:  if (!--h) w->mouse_shift(dot_mx), hover_win = {}; break; //todo reimplement mouse leaving
                    case WM_ACTIVATEAPP: if (!(wParam ? f++ : --f)) w->focus_event(f); break; // Focus between apps.
                    case WM_ACTIVATE:      w->state_event(!!lo(wParam), !!hi(wParam)); break; // Window focus within the app.
                    case WM_MOUSEACTIVATE: w->activate(); stat = MA_NOACTIVATE;        break; // Suppress window activation with a mouse click.
                    case WM_LBUTTONDOWN:   w->mouse_press(bttn::left,   true);         break;
                    case WM_MBUTTONDOWN:   w->mouse_press(bttn::middle, true);         break;
                    case WM_RBUTTONDOWN:   w->mouse_press(bttn::right,  true);         break;
                    case WM_LBUTTONUP:     w->mouse_press(bttn::left,   faux);         break;
                    case WM_MBUTTONUP:     w->mouse_press(bttn::middle, faux);         break;
                    case WM_RBUTTONUP:     w->mouse_press(bttn::right,  faux);         break;
                    case WM_MOUSEWHEEL:    w->mouse_wheel(hi(wParam), faux);           break;
                    case WM_MOUSEHWHEEL:   w->mouse_wheel(hi(wParam), true);           break;
                    case WM_SHOWWINDOW:    w->shown_event(!!wParam, lParam);           break; //todo revise
                    //case WM_GETMINMAXINFO: w->maximize(wParam, lParam);              break; // The system is about to maximize the window.
                    //case WM_SYSCOMMAND:  w->sys_command(wParam, lParam);             break; //todo taskbar ctx menu to change the size and position
                    case WM_KEYDOWN:
                    case WM_KEYUP:
                    case WM_SYSKEYDOWN:  // WM_CHAR/WM_SYSCHAR and WM_DEADCHAR/WM_SYSDEADCHAR are derived messages after translation.
                    case WM_SYSKEYUP:      w->keybd_press(wParam, lParam);             break;
                    case WM_DPICHANGED:    w->set_dpi(lo(wParam));                     break;
                    case WM_DESTROY:       ::PostQuitMessage(0);                       break;
                    //dx3d specific
                    //case WM_PAINT:   /*w->check_dx3d_state();*/ stat = ::DefWindowProcW(hWnd, msg, wParam, lParam); break;
                    default:                                    stat = ::DefWindowProcW(hWnd, msg, wParam, lParam); break;
                }
                w->update();
                return stat;
            };
            static auto wc_defwin = WNDCLASSW{ .lpfnWndProc = ::DefWindowProcW, .lpszClassName = L"vtm_decor" };
            static auto wc_window = WNDCLASSW{ .lpfnWndProc = window_proc, /*.cbWndExtra = 2 * sizeof(LONG_PTR),*/ .hCursor = ::LoadCursorW(NULL, (LPCWSTR)IDC_ARROW), .lpszClassName = L"vtm" };
            static auto reg = ::RegisterClassW(&wc_defwin) && ::RegisterClassW(&wc_window);
            if (!reg)
            {
                isfine = faux;
                log("%%window class registration error: %ec%", prompt::gui, ::GetLastError());
            }
            auto& wc = host_ptr ? wc_window : wc_defwin;
            auto owner = layers.empty() ? HWND{} : layers.front().hWnd;
            auto hWnd = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_LAYERED | (wc.hCursor ? 0 : WS_EX_TRANSPARENT),
                                          wc.lpszClassName, owner ? nullptr : wc.lpszClassName, // Title.
                                          WS_POPUP /*todo | owner ? WS_SYSMENU : 0  taskbar ctx menu*/, 0, 0, 0, 0, owner, 0, 0, 0);
            auto layer = (si32)layers.size();
            if (!hWnd)
            {
                isfine = faux;
                log("%%Window creation error: %ec%", prompt::gui, ::GetLastError());
            }
            else if (host_ptr)
            {
                ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)host_ptr);
                //::SetWindowLongPtrW(hWnd, 0, (LONG_PTR)host_ptr);
                //::SetWindowLongPtrW(hWnd, sizeof(LONG_PTR), (LONG_PTR)layer);
            }
            layers.emplace_back(hWnd);
            return layer;
        }
    };

    struct window : manager
    {
        using gray = netxs::raster<std::vector<byte>, rect>;
        using shad = netxs::misc::shadow<gray>;
        using grip = netxs::misc::szgrips;

        struct task
        {
            static constexpr auto _counter = 1 + __COUNTER__;
            static constexpr auto moved  = 1 << (__COUNTER__ - _counter);
            static constexpr auto sized  = 1 << (__COUNTER__ - _counter);
            static constexpr auto grips  = 1 << (__COUNTER__ - _counter);
            static constexpr auto hover  = 1 << (__COUNTER__ - _counter);
            static constexpr auto inner  = 1 << (__COUNTER__ - _counter);
            static constexpr auto header = 1 << (__COUNTER__ - _counter);
            static constexpr auto footer = 1 << (__COUNTER__ - _counter);
            static constexpr auto all = -1;
        };

        ui::face main_grid;
        ui::face head_grid;
        ui::face foot_grid;

        twod gridsz; // window: Grid size in cells.
        twod cellsz; // window: Cell size in pixels.
        twod gripsz; // window: Resizing grips size in pixels.
        dent border; // window: Border around window for resizing grips (dent in pixels).
        shad shadow; // window: Shadow generator.
        grip szgrip; // window: Resizing grips UI-control.
        twod mcoord; // window: Mouse cursor coord.
        si32 mbttns; // window: Mouse button state.
        si32 reload; // window: Changelog for update.
        si32 client; // window: Surface index for Client.
        si32 grip_l; // window: Surface index for Left resizing grip.
        si32 grip_r; // window: Surface index for Right resizing grip.
        si32 grip_t; // window: Surface index for Top resizing grip.
        si32 grip_b; // window: Surface index for Bottom resizing grip.
        si32 header; // window: Surface index for Header.
        si32 footer; // window: Surface index for Footer.
        bool drop_shadow{ true };

        static constexpr auto shadow_dent = dent{ 1,1,1,1 } * 3;

        window(rect win_coor_px_size_cell, text font, twod cell_size = { 10, 20 }, si32 win_mode = 0, twod grip_cell = { 2, 1 }, bool antialiasing = faux)
            : manager{ font, cell_size, antialiasing },
              gridsz{ std::max(dot_11, win_coor_px_size_cell.size) },
              cellsz{ cell_size },
              gripsz{ grip_cell * cell_size },
              border{ gripsz.x, gripsz.x, gripsz.y, gripsz.y },
              shadow{ 0.44f/*bias*/, 116.5f/*alfa*/, gripsz.x, dot_00, dot_11, cell::shaders::full },
              mbttns{},
              reload{ task::all },
              client{ add(this) },
              grip_l{ add(this) },
              grip_r{ add(this) },
              grip_t{ add(this) },
              grip_b{ add(this) },
              header{ add() },
              footer{ add() }
        {
            if (!*this) return;
            layers[client].area = { win_coor_px_size_cell.coor, gridsz * cellsz };
            recalc_layout();
            //todo temp
            main_grid.size(layers[client].area.size / cellsz);
            main_grid.cup(dot_00);
            main_grid.output(canvas_page);
            head_grid.size(layers[header].area.size / cellsz);
            head_grid.cup(dot_00);
            head_grid.output(header_page);
            foot_grid.size(layers[footer].area.size / cellsz);
            foot_grid.cup(dot_00);
            foot_grid.output(footer_page);
            update();
            manager::show(win_mode);
        }
        void recalc_layout()
        {
            auto base_rect = layers[client].area;
            layers[grip_l].area = base_rect + dent{ gripsz.x, -base_rect.size.x, gripsz.y, gripsz.y };
            layers[grip_r].area = base_rect + dent{ -base_rect.size.x, gripsz.x, gripsz.y, gripsz.y };
            layers[grip_t].area = base_rect + dent{ 0, 0, gripsz.y, -base_rect.size.y };
            layers[grip_b].area = base_rect + dent{ 0, 0, -base_rect.size.y, gripsz.y };
            auto h_size = base_rect.size / cellsz;
            auto f_size = base_rect.size / cellsz;
            head_grid.calc_page_height(header_page, h_size);
            head_grid.calc_page_height(footer_page, f_size);
            auto header_height = cellsz.y * h_size.y;
            auto footer_height = cellsz.y * f_size.y;
            layers[header].area = base_rect + dent{ 0, 0, header_height, -base_rect.size.y } + shadow_dent;
            layers[footer].area = base_rect + dent{ 0, 0, -base_rect.size.y, footer_height } + shadow_dent;
            layers[header].area.coor.y -= shadow_dent.b;
            layers[footer].area.coor.y += shadow_dent.t;
        }
        auto move_window(twod coor_delta)
        {
            manager::moveby(coor_delta);
            reload |= task::moved;
        }
        auto size_window(twod size_delta)
        {
            layers[client].area.size += size_delta;
            recalc_layout();
            reload |= task::sized;
            //todo temp
            main_grid.size(layers[client].area.size / cellsz);
            main_grid.cup(dot_00);
            main_grid.output<true>(canvas_page);
            head_grid.size(layers[header].area.size / cellsz);
            head_grid.cup(dot_00);
            head_grid.output(header_page);
            foot_grid.size(layers[footer].area.size / cellsz);
            foot_grid.cup(dot_00);
            foot_grid.output(footer_page);
        }
        auto resize_window(twod size_delta)
        {
            auto old_client = layers[client].area;
            auto new_gridsz = std::max(dot_11, (old_client.size + size_delta) / cellsz);
            size_delta = dot_00;
            if (gridsz != new_gridsz)
            {
                gridsz = new_gridsz;
                size_delta = gridsz * cellsz - old_client.size;
                size_window(size_delta);
            }
            return size_delta;
        }
        auto warp_window(dent warp_delta)
        {
            auto old_client = layers[client].area;
            auto new_client = old_client + warp_delta;
            auto new_gridsz = std::max(dot_11, new_client.size / cellsz);
            if (gridsz != new_gridsz)
            {
                gridsz = new_gridsz;
                auto size_delta = gridsz * cellsz - old_client.size;
                auto coor_delta = new_client.coor - old_client.coor;
                size_window(size_delta);
                move_window(coor_delta);
            }
            return layers[client].area - old_client;
        }
        void fill_back(auto& canvas)
        {
            auto region = canvas.area();
            auto r  = rect{ .size = cellsz };
            auto lt = dent{ 1, 0, 1, 0 };
            auto rb = dent{ 0, 1, 0, 1 };
            canvas.step(-region.coor);
            auto rtc = argb{ tint::pureblue  };//.alpha(0.5f);
            auto ltc = argb{ tint::pureblack };
            auto rbc = argb{ tint::pureblack };
            auto lbc = argb{ tint::pureblue  };//.alpha(0.5f);
            auto white = argb{ tint::whitedk };
            auto black = argb{ tint::blackdk };

            auto line_y = region.size.x * (cellsz.y - 1);
            auto offset = 0;
            auto stride = region.size.x - cellsz.x;
            auto step_x = cellsz.x;
            auto step_y = cellsz.x + line_y;
            for (r.coor.y = 0; r.coor.y < region.size.y; r.coor.y += cellsz.y)
            {
                auto fy = (fp32)r.coor.y / (region.size.y - 1);
                auto lc = argb::transit(ltc, lbc, fy);
                auto rc = argb::transit(rtc, rbc, fy);
                for (r.coor.x = 0; r.coor.x < region.size.x; r.coor.x += cellsz.x)
                {
                    auto fx = (fp32)r.coor.x / (region.size.x - 1);
                    auto p = argb::transit(lc, rc, fx);
                    netxs::inrect(canvas.begin() + offset, step_x, step_y, stride, cell::shaders::full(p));
                    //netxs::misc::cage(canvas, r, lt, cell::shaders::full(white));
                    //netxs::misc::cage(canvas, r, rb, cell::shaders::full(black));
                    offset += step_x;
                }
                offset += line_y;
            }
            canvas.step(region.coor);
        }
        bool hit_grips()
        {
            auto inner_rect = layers[client].area;
            auto outer_rect = layers[client].area + border;
            auto hit = !szgrip.zoomon && (szgrip.seized || (outer_rect.hittest(mcoord) && !inner_rect.hittest(mcoord)));
            return hit;
        }
        void fill_grips(rect area, auto fx)
        {
            for (auto g : { grip_l, grip_r, grip_t, grip_b })
            {
                auto& layer = layers[g];
                if (auto r = layer.area.trim(area))
                {
                    auto canvas = layer.canvas();
                    fx(canvas, r);
                }
            }
        }
        void draw_grips()
        {
            static auto trans = 0x01'00'00'00;
            static auto shade = 0x5F'3f'3f'3f;
            static auto black = 0x3F'00'00'00;
            auto inner_rect = layers[client].area;
            auto outer_rect = layers[client].area + border;
            fill_grips(outer_rect, [](auto& canvas, auto r){ netxs::misc::fill(canvas, r, cell::shaders::full(trans)); });
            if (hit_grips())
            {
                auto s = szgrip.sector;
                auto [side_x, side_y] = szgrip.layout(outer_rect);
                auto dent_x = dent{ s.x < 0, s.x > 0, s.y > 0, s.y < 0 };
                auto dent_y = dent{ s.x > 0, s.x < 0, 1, 1 };
                fill_grips(side_x, [&](auto& canvas, auto r)
                {
                    netxs::misc::fill(canvas, r, cell::shaders::full(shade));
                    netxs::misc::cage(canvas, side_x, dent_x, cell::shaders::full(black)); // 1-px dark contour around.
                });
                fill_grips(side_y, [&](auto& canvas, auto r)
                {
                    netxs::misc::fill(canvas, r, cell::shaders::full(shade));
                    netxs::misc::cage(canvas, side_y, dent_y, cell::shaders::full(black)); // 1-px dark contour around.
                });
            }
            if (drop_shadow) fill_grips(outer_rect, [&](auto& canvas, auto r)
            {
                shadow.render(canvas, r, inner_rect, cell::shaders::alpha);
            });
        }
        void draw_title(si32 index, auto& facedata) //todo just output ui::core
        {
            auto canvas = layers[index].canvas(true);
            auto r = canvas.area().moveto(dot_00) - shadow_dent;
            gcache.fill_grid(canvas, r, facedata);
            netxs::misc::contour(canvas); // 1ms
        }
        void draw_header() { draw_title(header, head_grid); }
        void draw_footer() { draw_title(footer, foot_grid); }
        void update()
        {
            if (!reload) return;
            auto what = reload;
            reload = {};
                 if (what == task::moved) manager::present<true>();
            else if (what)
            {
                if (what & (task::sized | task::inner))
                {
                    auto canvas = layers[client].canvas();
                    fill_back(canvas);
                    gcache.fill_grid(canvas, canvas.area(), main_grid); // 0.500 ms);
                }
                if (what & (task::sized | task::hover | task::grips)) draw_grips(); // 0.150 ms
                if (what & (task::sized | task::header)) draw_header();
                if (what & (task::sized | task::footer)) draw_footer();
                //if (layers[client].area.hittest(mcoord))
                //{
                //    auto cursor = rect{ mcoord - (mcoord - layers[client].area.coor) % cellsz, cellsz };
                //    netxs::onrect(layers[client].canvas(), cursor, cell::shaders::full(0x7F'00'3f'00));
                //}
                manager::present();
            }
        }
        auto& kbs()
        {
            static auto state_kb = 0;
            return state_kb;
        }
        auto keybd_state()
        {
            //todo unify
            auto state = hids::LShift   * !!::GetAsyncKeyState(VK_LSHIFT)
                       | hids::RShift   * !!::GetAsyncKeyState(VK_RSHIFT)
                       | hids::LWin     * !!::GetAsyncKeyState(VK_LWIN)
                       | hids::RWin     * !!::GetAsyncKeyState(VK_RWIN)
                       | hids::LAlt     * !!::GetAsyncKeyState(VK_LMENU)
                       | hids::RAlt     * !!::GetAsyncKeyState(VK_RMENU)
                       | hids::LCtrl    * !!::GetAsyncKeyState(VK_LCONTROL)
                       | hids::RCtrl    * !!::GetAsyncKeyState(VK_RCONTROL)
                       | hids::ScrlLock * !!::GetKeyState(VK_SCROLL)
                       | hids::NumLock  * !!::GetKeyState(VK_NUMLOCK)
                       | hids::CapsLock * !!::GetKeyState(VK_CAPITAL);
            return state;
        }
        void mouse_wheel(si32 delta, bool /*hz*/)
        {
            auto wheeldt = delta / 120;
            auto kb = kbs();
            //     if (kb & (hids::LCtrl | hids::LAlt)) netxs::_k2 += wheeldt > 0 ? 1 : -1; // LCtrl + Alt t +Wheel.
            //else if (kb & hids::LCtrl)                netxs::_k0 += wheeldt > 0 ? 1 : -1; // LCtrl+Wheel.
            //else if (kb & hids::anyAlt)               netxs::_k1 += wheeldt > 0 ? 1 : -1; // Alt+Wheel.
            //else if (kb & hids::RCtrl)                netxs::_k3 += wheeldt > 0 ? 1 : -1; // RCtrl+Wheel.
            //shadow = build_shadow_corner(cellsz.x);
            //reload |= task::sized;
            //netxs::_k0 += wheeldt > 0 ? 1 : -1;
            //log("wheel ", wheeldt, " k0= ", _k0, " k1= ", _k1, " k2= ", _k2, " k3= ", _k3, " keybd ", utf::to_bin(kb));

            if ((kb & hids::anyCtrl) && !(kb & hids::ScrlLock))
            {
                if (!szgrip.zoomon)
                {
                    szgrip.zoomdt = {};
                    szgrip.zoomon = true;
                    szgrip.zoomsz = layers[client].area;
                    szgrip.zoomat = mcoord;
                    mouse_capture();
                }
            }
            else if (szgrip.zoomon)
            {
                szgrip.zoomon = faux;
                mouse_release();
            }
            if (szgrip.zoomon)
            {
                auto warp = dent{ gripsz.x, gripsz.x, gripsz.y, gripsz.y };
                auto step = szgrip.zoomdt + warp * wheeldt;
                auto next = szgrip.zoomsz + step;
                next.size = std::max(dot_00, next.size);
                ///auto viewport = ...get max win size (multimon)
                //next.trimby(viewport);
                if (warp_window(next - layers[client].area)) szgrip.zoomdt = step;
            }
        }
        void mouse_shift(twod coord)
        {
            auto kb = kbs();// keybd_state();
            auto inner_rect = layers[client].area;
            if (hit_grips() || szgrip.seized)
            {
                if (mbttns & bttn::left)
                {
                    if (!szgrip.seized) // drag start
                    {
                        szgrip.grab(inner_rect, mcoord, border, cellsz);
                    }
                    auto zoom = kb & hids::anyCtrl;
                    auto [preview_area, size_delta] = szgrip.drag(inner_rect, coord, border, zoom, cellsz);
                    if (auto dxdy = resize_window(size_delta))
                    {
                        if (auto move_delta = szgrip.move(dxdy, zoom))
                        {
                            move_window(move_delta);
                        }
                    }
                }
                else if (szgrip.seized) // drag stop
                {
                    szgrip.drop();
                    reload |= task::grips;
                }
            }
            if (szgrip.zoomon && !(kb & hids::anyCtrl))
            {
                szgrip.zoomon = faux;
                mouse_release();
            }
            if (szgrip.calc(inner_rect, coord, border, dent{}, cellsz))
            {
                reload |= task::grips;
            }
            if (!szgrip.seized && mbttns & bttn::left)
            {
                if (auto dxdy = coord - mcoord)
                {
                    manager::moveby(dxdy);
                    reload |= task::moved;
                }
            }
            mcoord = coord;
            if (!mbttns)
            {
                static auto s = testy{ faux };
                reload |= s(hit_grips()) ? task::grips | task::inner
                                     : s ? task::grips : task::inner;
            }
        }
        void mouse_press(si32 button, bool pressed)
        {
            if (pressed && !mbttns) mouse_capture();
            pressed ? mbttns |= button
                    : mbttns &= ~button;
            if (!mbttns) mouse_release();
            if (!pressed & (button == bttn::right)) manager::close();
        }
        void keybd_press(arch vkey, arch lParam)
        {
            union key_state
            {
                ui32 token;
                struct
                {
                    ui32 repeat   : 16;// 0-15
                    ui32 scancode : 9; // 16-24 (24 - extended)
                    ui32 reserved : 5; // 25-29 (29 - context)
                    ui32 state    : 2; // 30-31: 0 - pressed, 1 - repeated, 2 - unknown, 3 - released
                } v;
            };
            auto param = key_state{ .token = (ui32)lParam };
            log("vkey: ", utf::to_hex(vkey),
                " scode: ", utf::to_hex(param.v.scancode),
                " state: ", param.v.state == 0 ? "pressed"
                          : param.v.state == 1 ? "rep"
                          : param.v.state == 3 ? "released" : "unknown");
            if (vkey == 0x1b) manager::close();
            kbs() = keybd_state();
            //auto s = keybd_state();
            //log("keybd ", utf::to_bin(s));
            //static auto keybd_state = std::array<byte, 256>{};
            //::GetKeyboardState(keybd_state.data());
            //auto l_shift = keybd_state[VK_LSHIFT];
            //auto r_shift = keybd_state[VK_RSHIFT];
            //auto l_win   = keybd_state[VK_LWIN];
            //auto r_win   = keybd_state[VK_RWIN];
            //bool alt     = keybd_state[VK_MENU];
            //bool l_alt   = keybd_state[VK_LMENU];
            //bool r_alt   = keybd_state[VK_RMENU];
            //bool l_ctrl  = keybd_state[VK_LCONTROL];
            //bool r_ctrl  = keybd_state[VK_RCONTROL];
            //log("keybd",
            //    "\n\t l_shift ", utf::to_hex(l_shift ),
            //    "\n\t r_shift ", utf::to_hex(r_shift ),
            //    "\n\t l_win   ", utf::to_hex(l_win   ),
            //    "\n\t r_win   ", utf::to_hex(r_win   ),
            //    "\n\t alt     ", utf::to_hex(alt     ),
            //    "\n\t l_alt   ", utf::to_hex(l_alt   ),
            //    "\n\t r_alt   ", utf::to_hex(r_alt   ),
            //    "\n\t l_ctrl  ", utf::to_hex(l_ctrl  ),
            //    "\n\t r_ctrl  ", utf::to_hex(r_ctrl  ));
        }
        void focus_event(bool focused)
        {
            log(focused ? "focused" : "unfocused");
        }
    };
}