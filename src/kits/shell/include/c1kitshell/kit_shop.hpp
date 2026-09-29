#pragma once

// The shop page the Health Kit ("Doctor's page", stock file "Health") and
// the Breeder's Kit ("Aphrodisiac page", "Aphro") share.  The 1996 kits
// showed one item at a time in a picture frame with arrows, its name, how
// many are left and what it does; the middle button ran the item's CAOS and
// took one off (CAddObjectPage, in both).  Here the whole stock is on a
// shelf at once: a card per item with its picture, name, what it does and
// how many are left, and its own "Put one in the world" button that does
// the same.

#include "c1kitshell/kit_art.hpp"
#include "c1kitshell/kit_shell.hpp"
#include "c1kitshell/kit_widgets.hpp"
#include "c1kit/health_files.hpp"

#include <string>
#include <vector>

namespace c1kitshell {

// What the page needs from its kit.
class ShopHost {
public:
    virtual ~ShopHost() = default;
    virtual std::vector<c1kit::ShopItem>& shop_items() = 0;
    // Runs an item's CAOS in the game.
    virtual bool run_shop_command(const std::string& script) = 0;
    // Writes the stock file now.
    virtual bool save_shop() = 0;
    virtual const GamePalette& shop_palette() const = 0;
    // The stock file's name, for messages.
    virtual const char* shop_file_name() const = 0;
};

class ShopPage : public LayoutPage {
public:
    ShopPage(KitSheet& sheet, ShopHost& host, UINT blank_dialog, UINT title_string);
    // Refreshes the list and the picture (call when the stock was reloaded).
    void refresh();

protected:
    void create_controls() override;
    void layout(int width, int height) override;

private:
    // Lays the cards out across the shelf's width: as many columns as fit,
    // every card as tall as the tallest.  Rectangles are in shelf content
    // coordinates (before scrolling).
    void arrange_cards();
    void draw_shelf(CDC& dc, const CRect& rect);
    void draw_card(CDC& dc, std::size_t index, const CRect& card);
    void on_mouse(CPoint point, bool clicked);
    // SubmitSelectedHealthValue: run the item's CAOS, take one off, save.
    void put_one_in_world(std::size_t index);
    // The picture's whole-number scale on a card.
    int picture_scale(const c1kit::ShopItem& item) const;

    ShopHost& host_;
    PaintedView shelf_;
    CStatic status_;
    Canvas canvas_;
    CFont name_font_;
    std::vector<CRect> cards_;
    std::vector<CRect> buttons_;
    int card_width_ = 0;
    int picture_height_ = 0;  // the tallest scaled picture's well
    int hover_ = -1;          // the button under the mouse
};

// The classic look's shop page: the 1996 CAddObjectPage itself (dialog 142
// in both kits).  One item at a time, drawn at its own size in the middle
// of the board -- the Health Kit's Black.bmp, or the Breeder's Shop.bmp with
// Addbgd.bmp behind the item (RenderHealthValueBitmap @ 0x00405a60) -- with
// its name, how many are left and what it does in the template's boxes,
// the arrows and the Earth button (their faces the original's PREV, NEXT
// and EARTH bitmaps, from the original itself), and Close.  The Earth button
// does what the modern page's "Put one in the world" does.
class ClassicShopPage : public CPropertyPage {
public:
    ClassicShopPage(KitSheet& sheet, ShopHost& host, const ClassicArt& art, UINT dialog,
                    UINT title_string, const char* board, bool item_backdrop);
    void refresh();

protected:
    BOOL OnInitDialog() override;
    afx_msg void OnPrevious();
    afx_msg void OnNext();
    afx_msg void OnEarth();
    afx_msg void OnCloseKit();
    afx_msg void OnDrawItem(int id, LPDRAWITEMSTRUCT draw);
    afx_msg HBRUSH OnCtlColor(CDC* dc, CWnd* control, UINT type);
    DECLARE_MESSAGE_MAP()

private:
    void show();
    void draw_board(CDC& dc, const CRect& rect);
    HBITMAP face(const CString& caption, TCHAR state) const;

    KitSheet& kit_sheet_;
    ShopHost& host_;
    const ClassicArt& art_;
    CString title_;
    std::string board_;
    bool item_backdrop_ = false;
    int selected_ = 0;
    PaintedView board_view_;
    Canvas canvas_;
    CFont title_font_;
    CFont notes_font_;
    CBrush white_;
};

} // namespace c1kitshell
