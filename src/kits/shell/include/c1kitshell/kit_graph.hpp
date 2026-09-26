#pragma once

// A graph of chemical levels over time, which the Science Kit and the
// Biochemistry Kit share: 0-255 up the side, the newest sample at the right,
// each chemical in its own colour with a legend, and, where the pointer is,
// a line and every chemical's level at that moment.

#include <afxwin.h>

#include <deque>
#include <string>
#include <vector>

namespace c1kitshell {

class ChemicalGraph {
public:
    struct Series {
        int chemical = 0;
        std::deque<int> values;  // newest last
    };

    explicit ChemicalGraph(std::size_t max_samples = 4000, int milliseconds_per_sample = 1000)
        : max_samples_(max_samples), milliseconds_per_sample_(milliseconds_per_sample) {}

    const std::vector<Series>& series() const { return series_; }
    std::vector<int> chemicals() const;
    int index_of(int chemical) const;
    // Follows exactly these chemicals, in this order, keeping the history of
    // any already followed.
    void set_chemicals(const std::vector<int>& chemicals);
    // One level per followed chemical, in order.
    void add_sample(const std::vector<int>& levels);
    void clear();
    // Where the pointer is, in the view's coordinates (x < 0: nowhere).
    void set_pointer(int x) { pointer_x_ = x; }

    // `names` are allchemicals.str's; `empty_message` is shown with nothing
    // to draw.
    void draw(CDC& dc, const CRect& rect, const std::vector<std::string>& names,
              const CString& empty_message) const;

private:
    std::vector<Series> series_;
    std::size_t max_samples_;
    int milliseconds_per_sample_;
    int pointer_x_ = -1;
};

// "Hunger" or "Chemical 73".
std::string chemical_display_name(const std::vector<std::string>& names, int chemical);

} // namespace c1kitshell
