#pragma once

// Widgets for kit pages laid out in code: a page base whose controls are
// made and placed in code (so it can grow with the window), and a
// flicker-free view drawn by a callback.  Not in the 1996 kits, whose pages
// were fixed dialog templates.

#include "c1kitshell/kit_shell.hpp"

#include <functional>

namespace c1kitshell {

// A child window drawn by a callback into an off-screen bitmap, so it never
// flickers; mouse movement (and leaving, as (-1, -1)) and clicks are passed
// on.
class PaintedView : public CWnd {
public:
    using Painter = std::function<void(CDC&, const CRect&)>;
    using MouseHandler = std::function<void(CPoint, bool clicked)>;

    bool create(CWnd& parent, UINT id, Painter painter);
    void set_mouse_handler(MouseHandler handler) { mouse_ = std::move(handler); }
    void set_double_click_handler(std::function<void(CPoint)> handler) {
        double_click_ = std::move(handler);
    }
    void redraw() { if (GetSafeHwnd() != nullptr) Invalidate(FALSE); }

protected:
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnLButtonDblClk(UINT flags, CPoint point);
    afx_msg void OnMouseLeave();
    DECLARE_MESSAGE_MAP()

private:
    Painter painter_;
    MouseHandler mouse_;
    std::function<void(CPoint)> double_click_;
    bool tracking_ = false;
};

// Distinct colours: for plotted lines (the 1996 kits' red, blue, green and
// purple first), and one per standard brain lobe.
COLORREF series_colour(int index);
COLORREF lobe_colour(int lobe);
// `b_percent` of b over a.
COLORREF blend(COLORREF a, COLORREF b, int b_percent);

// A property page on a blank dialog template, whose controls are made in
// create_controls() and placed in layout() whenever the page changes size.
// It has a Close button (IDCLOSE) that asks the game to close the kit.
class LayoutPage : public CPropertyPage {
public:
    LayoutPage(KitSheet& sheet, UINT blank_dialog, UINT title_string);

protected:
    BOOL OnInitDialog() override;
    BOOL OnSetActive() override;
    virtual void create_controls() = 0;
    virtual void layout(int width, int height) = 0;
    afx_msg void OnSize(UINT type, int cx, int cy);
    afx_msg void OnCloseKit();
    DECLARE_MESSAGE_MAP()

    // Makes a control with the page's font.
    CWnd* make(CWnd& control, LPCTSTR window_class, LPCTSTR text, DWORD style,
               UINT id, DWORD ex_style = 0);
    void place(CWnd& control, int x, int y, int width, int height);
    // A line of text in the page's font, plus a little.
    int text_height() const { return text_height_; }
    // The usual gap and button height.
    static constexpr int kMargin = 7;
    int button_height() const { return text_height_ + 8; }
    // The Close button, bottom right.
    void place_close(int width, int height);

    KitSheet& kit_sheet_;
    CString title_;
    CButton close_;
    bool created_ = false;

private:
    int text_height_ = 13;
};

} // namespace c1kitshell
