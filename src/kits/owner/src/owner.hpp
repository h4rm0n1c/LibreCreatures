#pragma once

// The Owner's Kit (Tool slot 2): register a creature's birth with its owner's
// details, keep a photo album of it, and show its birth certificate.  The
// register and the albums are files in the world's folder, which the
// Funeral Kit reads too.
//
// Rebuilt from /C1 Kits/Owners_Kit.exe; ../ORIGINAL.md describes the
// original's behaviour and numbers its bugs.  This build keeps the original's
// protocol, files and art, drops its cover page and sound, and fixes those
// bugs; each fix is marked "Fix (bug N)".

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kit/conversation.hpp"
#include "c1kit/owner_files.hpp"

#include <memory>
#include <string>
#include <vector>

namespace owner {

class OwnerSheet;

// The creature the kit is looking after: the one selected in the game when
// the kit took it on.
struct Subject {
    bool present = false;
    std::string moniker;
    std::string name;
    c1kit::OwnerRecord history;  // the game's record (`dde: getb data`)
    bool has_history = false;
    int sex = 0;                 // 1 male, 2 female, 0 unknown
    std::string age;             // "h:mm"
};

// ---------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------

class RegisterPage : public CPropertyPage {
public:
    explicit RegisterPage(OwnerSheet& sheet);
    void show();  // refresh from the sheet

protected:
    BOOL OnInitDialog() override;
    BOOL OnSetActive() override;
    afx_msg void OnRegisterBirth();
    afx_msg void OnCloseKit();
    DECLARE_MESSAGE_MAP()

private:
    OwnerSheet& sheet_;
    bool initialized_ = false;
};

class AlbumPage : public CPropertyPage {
public:
    explicit AlbumPage(OwnerSheet& sheet);
    void show();  // refresh from the sheet
    // Stores an edited caption; called before the album changes or saves.
    void commit_caption();

protected:
    BOOL OnInitDialog() override;
    BOOL OnSetActive() override;
    BOOL OnKillActive() override;
    afx_msg void OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw);
    afx_msg void OnSaveAs();
    afx_msg void OnDeletePhoto();
    afx_msg void OnTakePhoto();
    afx_msg void OnPreviousPhoto();
    afx_msg void OnNextPhoto();
    afx_msg void OnCaptionKillFocus();
    afx_msg void OnCloseKit();
    DECLARE_MESSAGE_MAP()

private:
    void draw_picture();

    OwnerSheet& sheet_;
    CBitmapButton save_as_;
    CBitmapButton delete_;
    CBitmapButton camera_;
    CBitmapButton previous_;
    CBitmapButton next_;
    c1kitshell::Canvas picture_;
    bool initialized_ = false;
};

class CertificatePage : public CPropertyPage {
public:
    explicit CertificatePage(OwnerSheet& sheet);
    void show();

protected:
    BOOL OnInitDialog() override;
    BOOL OnSetActive() override;
    afx_msg void OnDrawItem(int control_id, LPDRAWITEMSTRUCT draw);
    afx_msg void OnCloseKit();
    DECLARE_MESSAGE_MAP()

private:
    OwnerSheet& sheet_;
    c1kitshell::Canvas picture_;
    CFont font_;
    bool initialized_ = false;
};

// ---------------------------------------------------------------------------
// The sheet
// ---------------------------------------------------------------------------

class OwnerSheet : public c1kitshell::KitSheet {
public:
    explicit OwnerSheet(CFont& default_font);
    ~OwnerSheet() override;

    bool create_window();

    const Subject& subject() const { return subject_; }
    const std::vector<c1kit::OwnerRecord>& register_records() const {
        return register_;
    }
    const c1kit::OwnerRecord* registered_record() const;
    // A creature's name from the register, or "Unknown".
    CString name_for_moniker(const std::string& moniker) const;

    std::vector<c1kit::Photo>& photos() { return photos_; }
    int selected_photo() const { return selected_photo_; }
    void select_photo(int index);
    const c1kitshell::GamePalette& palette() const { return palette_; }

    // Register Birth: store the record on the creature and in the register.
    bool register_birth(const c1kit::OwnerRecord& record);
    // Take photo: returns false (with a message shown) if it failed.
    bool take_photo();
    void delete_photo(int index);
    // Writes the album file now.
    bool save_album();

protected:
    BOOL OnInitDialog() override;
    void on_control_state(std::uint8_t state) override;
    void before_game_quit() override;
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnTimer(UINT_PTR timer_id);
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnSysCommand(UINT id, LPARAM lparam);
    DECLARE_MESSAGE_MAP()

private:
    void load_preferences();
    void save_preferences();
    void connect();
    bool query(const char* script, std::string& reply);
    void take_subject();       // `putv ownr`, then everything about it
    void refresh_subject();    // name, record, sex and age
    void refresh_age();
    void load_register();
    bool save_register();
    void load_album();
    void show_all();
    void update_title();
    bool paused() const { return game_paused_ || minimised_; }
    void set_always_on_top(bool on);
    std::string world_file(const std::string& name) const;

    CFont& default_font_;
    RegisterPage register_page_;
    AlbumPage album_page_;
    CertificatePage certificate_page_;
    c1kit::KitSettings* registry_ = nullptr;
    std::unique_ptr<c1kit::MacroConversation> conversation_;
    c1kitshell::GamePalette palette_;
    Subject subject_;
    std::vector<c1kit::OwnerRecord> register_;
    std::vector<c1kit::Photo> photos_;
    int selected_photo_ = 0;
    std::uint32_t saved_photo_ = 0;
    std::uint32_t always_on_top_ = 0;
    int saved_page_ = 0;
    bool connected_ = false;
    bool game_paused_ = false;
    bool minimised_ = false;
};

} // namespace owner
