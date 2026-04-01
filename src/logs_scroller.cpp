#include "logs_scroller.h"

#include "scroller.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

using namespace ftxui;

namespace NTPCC {

Component LogsScroller(TLogCapture& logCapture) {
    return Scroller(Renderer([&] {
        Elements logElements;

        auto lines = logCapture.GetLines();
        for (const auto& line : lines) {
            auto elem = paragraph(line);

            if (line.find("error") != std::string::npos || line.find("ERROR") != std::string::npos) {
                elem = elem | color(Color::Red);
            } else if (line.find("warn") != std::string::npos || line.find("WARN") != std::string::npos) {
                elem = elem | color(Color::Yellow);
            }

            logElements.push_back(elem);
        }

        auto logsContent = vbox(logElements) | flex;
        return logsContent;
    }), "Logs");
}

} // namespace NTPCC
