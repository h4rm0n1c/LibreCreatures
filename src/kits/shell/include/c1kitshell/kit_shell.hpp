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

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------

class KitApp : public CWinApp, private c1kit::KitEvents {
public:
    KitApp();

    BOOL InitInstance() override;
    int ExitInstance() override;

    CFont& default_font() { return default_font_; }

private:
    bool on_communicate(std::int32_t header, std::int32_t payload) override;

    c1kit::KitServer* server_ = nullptr;
    CFont default_font_;
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

    // The classic look's continuous sound: the game's Sounds\<sound>.wav,
    // looped at `volume` (hundredths of a decibel below full) while the kit
    // is open, with a "Mute ambient sound" checkbox under the pages whose
    // setting is kept as "Mute Ambient" in `settings`.  For a fixed-size
    // sheet; call it from OnInitDialog.  A kit with a menu passes
    // `checkbox` false and offers set_ambience_muted there instead.
    void enable_ambience(c1kit::KitSettings* settings, const char* sound, int volume,
                         bool checkbox = true);
    bool ambience_muted() const { return ambience_muted_; }
    void set_ambience_muted(bool muted);

    BOOL OnCommand(WPARAM wparam, LPARAM lparam) override;
    BOOL OnNotify(WPARAM wparam, LPARAM lparam, LRESULT* result) override;
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* info);
    afx_msg LRESULT OnDeferredLayout(WPARAM, LPARAM);
    DECLARE_MESSAGE_MAP()

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
    void add_mute_checkbox();
    std::unique_ptr<KitSound> ambience_;
    c1kit::KitSettings* ambience_settings_ = nullptr;
    std::string ambience_sound_;
    int ambience_volume_ = 0;
    int ambience_channel_ = -1;
    bool ambience_muted_ = false;
    CButton mute_check_;
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

// A kit can wear its 1996 interface.  It does when the player's original
// kit sits beside it renamed "<name>.old" (or "<name>.exe.old") -- the 1996
// release, the later one, or GOG's -- and holds the art the classic pages
// use.  The art is read from that file as data (nothing in it runs), so
// this kit carries none of it.  There is no other switch: the file being
// there is the choice.
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

// The 1996 kits' first page: a picture filling the page, and the kit's icon
// on its tab.
class CoverPage : public CPropertyPage {
public:
    CoverPage(UINT dialog_id, UINT bitmap_id, UINT tab_icon_id, const ClassicArt& art);
    // The picture a file in the game's Main Directory (Score.bmp, ...),
    // centred in the page's picture frame (control 1140), as the 1996 kits'
    // covers drew it; no tab icon.
    CoverPage(UINT dialog_id, const char* picture_file);

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnPaint();
    DECLARE_MESSAGE_MAP()

private:
    UINT bitmap_id_ = 0;
    const ClassicArt* art_ = nullptr;
    CString picture_file_;
    PaletteBitmap bitmap_;
};

// Loads a string-table entry (empty when missing).
CString load_string(UINT id);

} // namespace c1kitshell
