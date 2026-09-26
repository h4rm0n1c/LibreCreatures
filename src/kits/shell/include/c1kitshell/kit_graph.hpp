#pragma once

// A graph of chemical levels over time, which the Science Kit and the
// Biochemistry Kit share.  It is drawn as the Biochemistry Kit v1.2's graph
// was (CMonitorPage, DrawGraphAxesAndLabels @ 0x00403c00 and friends): a
// light-grey 16-pixel grid that scrolls with the samples, black axes with
// ticks every 16 pixels, 0/64/128/192/255 up the side, how long ago along
// the bottom every 16 samples, and four pixels a sample.  Beyond that, a
// legend in a strip below the plot, and where the pointer is, a line and
// every level at that moment.

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
    long long total_samples_ = 0;  // keeps the grid moving with the samples
};

// "Hunger" or "Chemical 73".
std::string chemical_display_name(const std::vector<std::string>& names, int chemical);

} // namespace c1kitshell
