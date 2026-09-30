#pragma once

// The kit shell: everything a kit shows, draws or plays.  One codebase; each
// kit links it with its own module, which supplies a KitDefinition.  The data
// side (SFC.OLE, the OLE server, settings) comes from c1kitlib.dll.

#include <afxwin.h>
#include <afxdlgs.h>
#include <afxcmn.h>
#include <afxole.h>

#include <memory>
#include <string>
#include <vector>

#include "c1kit/c1kit.hpp"
#include "c1kit/protocol.hpp"
#include "c1kitshell/kit_sound.hpp"

namespace c1kitshell {

class KitSheet;

// Supplied by the kit module (exactly one per executable).
struct KitDefinition {
    c1kit::KitIdentity identity;     // the kit's OLE server ProgID and CLSID
    int tool_slot = 0;               // Tool<N> slot
    const char* tool_value_prog_id = nullptr;  // ProgID as written in Tool<N>
    UINT tool_name_string = 0;       // Tool<N> menu name
    UINT tool_help_string = 0;       // Tool<N> status-bar help
    UINT ole_init_failed_string = 0; // shown when AfxOleInit fails
    // Set the working directory to the game's Main Directory at start-up
    // (every 1996 kit does this before anything else).
    bool use_main_directory = true;
    // Creates and returns the kit's main window (a KitSheet), or nullptr.
    KitSheet* (*create_main_window)(CFont& default_font) = nullptr;
};

const KitDefinition& kit_definition();

// Options > Skin: the kit's own look or the 1996 one (see ClassicArt).
enum class KitSkin : std::uint32_t { automatic = 0, libre = 1, classic = 2 };
KitSkin skin_preference();
void set_skin_preference(KitSkin skin);
// Whether the last ClassicArt::find found the original's art, whatever the
// preference: Options > Skin offers the 1996 look only then.
bool classic_art_available();
void note_classic_art_available(bool available);  // ClassicArt::find's record

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------

class KitApp : public CWinApp, private c1kit::KitEvents {
public:
    KitApp();

    BOOL InitInstance() override;
    int ExitInstance() override;

    CFont& default_font() { return default_font_; }

    // Options > Skin: builds the main window again in the chosen look and
    // closes `old` without telling the game the kit has quit.  The new
    // window keeps the game's tool id; it connects afresh.
    void rebuild_main_window(KitSheet& old);

private:
    bool on_communicate(std::int32_t header, std::int32_t payload) override;

    c1kit::KitServer* server_ = nullptr;
    CFont default_font_;
    std::vector<std::unique_ptr<KitSheet>> retired_windows_;
};

// ---------------------------------------------------------------------------
// Main window: a modeless property sheet connected to the game
// ---------------------------------------------------------------------------

class KitSheet : public CPropertySheet {
public:
    KitSheet(UINT caption_string, UINT paused_suffix_string);
    ~KitSheet() override;

    // Game -> kit.  kind 1 / code 3 records the tool id; the rest go to the
    // virtual hooks below.
    void handle_kit_message(const c1kit::KitMessage& message);

    // Kit -> game.
    c1kit::MacroTransport* transport() const { return transport_; }
    bool quitting() const { return quitting_; }
    int tool_id() const { return tool_id_; }
    void adopt_tool_id(int tool_id) { tool_id_ = tool_id; }

    // "inst,app: quit <tool id>,endm" through a fresh scheduler holder, or a
    // plain application exit if the kit never connected; then close
    // (Observation RequestGameQuit @ 0x004038a0).
    void request_game_quit();

protected:
    // ConnectToApplicationOle (Observation @ 0x00403320): connect, and on
    // failure report the automation error or "Can not communicate with
    // application".
    bool connect_to_game(std::size_t buffer_bytes);

    virtual void on_integer_message(std::int32_t) {}
    virtual void on_control_state(std::uint8_t) {}
    // Runs before request_game_quit sends "app: quit".  Save anything that
    // must survive here: when the game launched the kit itself (as it does
    // under Wine) it terminates the process during that call, before the
    // window is destroyed.
    virtual void before_game_quit() {}

    // Resizing (not in the 1996 kits, whose sheets were fixed at their page
    // templates' size).  Create the window with WS_THICKFRAME, then call
    // enable_resizing from OnInitDialog: the window grows so the page area is
    // at least `default_page_dlu` dialog units, and it can be shrunk no
    // further than its template size.  The tab control and the active page
    // then follow the window.
    void enable_resizing(CSize default_page_dlu);
    bool resizing_enabled() const { return resizable_; }
    // Sets the outer window size, no smaller than the template size.
    void set_window_size(CSize size);
    // Places the tab control and the active page in the client area.  Runs
    // on every size change and after every page switch; call it after
    // SetActivePage too.
    void layout_pages();

    // The kit's continuous sound: the game's Sounds\<sound>.wav, looped at
    // `volume` (hundredths of a decibel below full, the 1996 kit's) while
    // the kit is open and sounds are not muted.  Call from OnInitDialog,
    // after install_options.
    void enable_ambience(const char* sound, int volume);
    // For a fixed-size sheet: widens the window and its tab strip until
    // every tab fits on one row (the 1996 sheets had the room; a narrower
    // one would hide tabs behind scroll arrows).  Call from OnInitDialog,
    // before enable_ambience.
    void fit_tabs();

    BOOL OnCommand(WPARAM wparam, LPARAM lparam) override;
    LRESULT WindowProc(UINT message, WPARAM wparam, LPARAM lparam) override;
    BOOL OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) override;
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* info);
    afx_msg LRESULT OnDeferredLayout(WPARAM, LPARAM);
    DECLARE_MESSAGE_MAP()

    // Options and Help, the same in every kit: a menu bar on the kit's own
    // look, and the window's system menu on the 1996 look (which had no
    // menu bar).  Options holds Always on top, Mute sounds, Skin, and then
    // whatever add_kit_options adds; Help holds About.  Reads "On Top" and
    // "Mute Sounds" from `settings` (and the older "Keep on top" and "Mute
    // Ambient" once), sets the window's Z-order, and sets the sound volume
    // for the look.  Call from OnInitDialog once the settings are open.
    void install_options(c1kit::KitSettings* settings, bool classic,
                         bool on_top_by_default = false);
    bool always_on_top() const { return always_on_top_; }
    // Kit-specific Options items.  Ids from kKitOptionFirst, in steps of
    // kKitOptionStep (the system menu keeps the low four bits).
    static constexpr UINT kKitOptionFirst = 0x0200;
    static constexpr UINT kKitOptionStep = 0x10;
    virtual void add_kit_options(CMenu& /*options*/) {}
    virtual bool on_kit_option(UINT /*id*/) { return false; }
    // Check or grey a kit item on whichever menu holds it.
    void check_kit_option(UINT id, bool checked);
    void enable_kit_option(UINT id, bool enabled);
    // Saves the window's settings before a skin change rebuilds it.
    virtual void before_skin_change() {}
    // After Options > Mute sounds: a kit playing its own sounds (loops the
    // mute stopped) starts them again here when sounds come back on.
    virtual void on_sounds_muted_changed() {}
    // Help > About: the kit's name and build.  A kit with its own About
    // box (the 1996 Observation Kit's) shows that instead.
    virtual void show_about();

    // The paused title: caption + suffix, and back
    // (Begin/CompleteOverviewWindowDisplayTransition @ 0x00403ee0/0x00403fb0).
    void show_paused_title();
    void show_normal_title();

private:
    c1kit::MacroTransport* transport_ = nullptr;
    UINT caption_string_ = 0;
    UINT paused_suffix_string_ = 0;
    int tool_id_ = 0;
    bool quitting_ = false;
    bool resizable_ = false;
    CSize min_track_{0, 0};
    void apply_ambience();
    bool handle_option(UINT id);
    CMenu* options_menu();
    void set_always_on_top(bool on_top);
    void set_sounds_muted(bool muted);
    void change_skin(KitSkin skin);
    void refresh_option_checks();
    c1kit::KitSettings* option_settings_ = nullptr;
    bool classic_look_ = false;
    bool always_on_top_ = false;
    CMenu menu_bar_;
    std::unique_ptr<KitSound> ambience_;
    std::string ambience_sound_;
    int ambience_volume_ = 0;
    int ambience_channel_ = -1;

    CRect tab_margins_{0, 0, 0, 0};  // tab control inset from the client area
};

// ---------------------------------------------------------------------------
// Graphics: an 8-bit bitmap resource with its own palette
// ---------------------------------------------------------------------------

// The kits' bitmap resource object (Observation OverviewBitmapResource @
// 0x00402800): the DIB section plus a logical palette built from its colour
// table, so it can be realised before blitting.
class PaletteBitmap {
public:
    // From this kit's resources, or from `module` (a ClassicArt's).
    bool load(UINT resource_id, HMODULE module = nullptr);
    // From a .bmp file (the classic look's pictures in the game's folder).
    bool load_file(const CString& path);
    int width() const { return width_; }
    int height() const { return height_; }
    CBitmap& bitmap() { return bitmap_; }
    CPalette& palette() { return palette_; }
    // Selects and realises the palette, then blits at (x, y).
    void draw(CDC& dc, int x, int y);
    // Selects and realises the palette on a window's DC (the kits do this
    // once in OnInitDialog).
    void realize(CWnd& window);

private:
    bool adopt(HBITMAP handle);
    CBitmap bitmap_;
    CPalette palette_;
    int width_ = 0;
    int height_ = 0;
};

// Keeps a page's controls placed as the page grows: each control keeps its
// template position and size, then moves and/or stretches by however much
// the page is larger than its template.  (Not in the 1996 kits, whose
// windows could not be resized.)
class ControlAnchors {
public:
    enum : unsigned {
        kMoveX = 1,  // keep the distance to the right edge
        kMoveY = 2,  // keep the distance to the bottom edge
        kGrowX = 4,  // stretch with the width
        kGrowY = 8,  // stretch with the height
    };

    // Records the parent's current client size as the template size; call
    // from OnInitDialog before add().
    void capture(CWnd& parent);
    void add(CWnd& parent, UINT id, unsigned anchors);
    void apply(CWnd& parent) const;

private:
    struct Item {
        UINT id;
        CRect rect;
        unsigned anchors;
    };
    std::vector<Item> items_;
    CSize reference_{0, 0};
};

// ---------------------------------------------------------------------------
// The classic look: the 1996 art, from the player's own original kit
// ---------------------------------------------------------------------------

// A kit can wear its 1996 interface.  It can when the player's original
// kit sits beside it renamed "<name>.old" (or "<name>.exe.old") -- the 1996
// release, the later one, or GOG's -- and holds the art the classic pages
// use.  The art is read from that file as data (nothing in it runs), so
// this kit carries none of it.  Options > Skin chooses between the two;
// until the player picks, the file being there is the choice.
class ClassicArt {
public:
    // Opens the original beside this kit if it holds every bitmap and icon
    // listed (by number, and bitmaps by name); null otherwise, and the kit
    // keeps its modern interface.
    static std::unique_ptr<ClassicArt> find(std::initializer_list<UINT> bitmaps,
                                            std::initializer_list<const TCHAR*> named_bitmaps = {},
                                            std::initializer_list<UINT> icons = {});
    ~ClassicArt();
    ClassicArt(const ClassicArt&) = delete;
    ClassicArt& operator=(const ClassicArt&) = delete;

    HMODULE module() const { return module_; }
    const CString& path() const { return path_; }

private:
    ClassicArt(HMODULE module, const CString& path) : module_(module), path_(path) {}
    HMODULE module_;
    CString path_;
};

// The 1996 cover dialogs' picture frame.
constexpr UINT kCoverPictureFrame = 1140;

// The 1996 kits' first page: a picture filling the page.  Its tab shows
// only the kit's icon (icon 0x80, the original's), with no "Cover" text:
// every 1996 cover sets PSP_USEHICON and PSP_USETITLE with an empty title.
// (Wine's property sheet ignores both, for the originals too, and shows the
// dialog caption.)
class CoverPage : public CPropertyPage {
public:
    // The picture a bitmap in the original kit.
    CoverPage(UINT dialog_id, UINT bitmap_id, UINT tab_icon_id, const ClassicArt& art);
    // The picture a file in the game's Main Directory (Score.bmp, ...),
    // centred in the page's picture frame (control 1140), as the 1996 kits'
    // covers drew it.
    CoverPage(UINT dialog_id, const char* picture_file, UINT tab_icon_id);
    ~CoverPage() override;

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()

private:
    void use_tab_icon(UINT tab_icon_id);

    HICON tab_icon_ = nullptr;

    UINT bitmap_id_ = 0;
    const ClassicArt* art_ = nullptr;  // set when the picture is a bitmap in it
    CString picture_file_;
    PaletteBitmap bitmap_;
};

// Loads a string-table entry (empty when missing).
CString load_string(UINT id);

} // namespace c1kitshell
