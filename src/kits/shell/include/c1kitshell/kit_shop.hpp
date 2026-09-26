#pragma once

// The shop page the Health Kit ("Doctor's page", stock file "Health") and
// the Breeder's Kit ("Aphrodisiac page", "Aphro") share.  The 1996 kits
// showed one item at a time in a picture frame with arrows, its name, how
// many are left and what it does; the middle button ran the item's CAOS and
// took one off (CAddObjectPage, in both).  Here every item is in a list, the
// selected one drawn large with its details, and "Put one in the world"
// does the same.

#include "c1kitshell/kit_art.hpp"
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
    afx_msg void OnSelectionChanged();
    afx_msg void OnAddToWorld();
    DECLARE_MESSAGE_MAP()

private:
    void fill_list();
    void show_selected();
    void draw_picture(CDC& dc, const CRect& rect);

    ShopHost& host_;
    CListBox items_;
    PaintedView picture_;
    CButton add_;
    CStatic status_;
    Canvas canvas_;
};

} // namespace c1kitshell
