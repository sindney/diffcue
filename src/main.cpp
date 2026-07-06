// diffcue entry point.
// Task 4.5: parse args, exit early on help/version/error before GUI init.
// Section 9: construct App + run the loop for the Ok case.
#include <cstdlib>
#include <iostream>

#include "cli/args.h"
#include "diffcue/version.h"
#include "app/app.h"

int main(int argc, char** argv) {
    using namespace diffcue::cli;

    const ParsedArgs parsed = parse_args(argc, argv);

    switch (parsed.status) {
        case ParseStatus::Help:
            std::cout << usage_text();
            return 0;
        case ParseStatus::Version:
            std::cout << "diffcue " << DIFFCUE_VERSION << "\n";
            return 0;
        case ParseStatus::Error:
            std::cerr << "diffcue: " << parsed.error << "\n\n";
            std::cerr << usage_text();
            return 2;
        case ParseStatus::Ok:
            break;
    }

    // Section 9: launch the GUI.
    diffcue::App app(*parsed.folder);
    return app.run();
}
