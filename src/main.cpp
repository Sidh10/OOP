#include <cstdio>
#include <memory>

#include "quant/data/CurlHttpClient.hpp"
#include "quant/ui/MenuApp.hpp"

int main(int argc, char** argv) {
    try {
        return quant::runCli(argc, argv, std::make_shared<quant::CurlHttpClient>());
    } catch (const std::exception& e) {  // e.g. curl handle could not be created
        std::fprintf(stderr, "Error: %s\n", e.what());
        return 1;
    }
}
