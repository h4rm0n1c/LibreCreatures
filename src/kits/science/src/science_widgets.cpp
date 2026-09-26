// The page base the Science Kit's pages share.

#include "science.hpp"
#include "science_ids.hpp"

namespace science {

SciencePage::SciencePage(ScienceSheet& sheet, UINT title_string)
    : LayoutPage(sheet, kDialogPage, title_string), sheet_(sheet) {}

} // namespace science
