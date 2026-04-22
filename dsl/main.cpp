#include <fstream>
#include <iostream>
#include <string>

#include "schema.h"
#include "codegen.h"
#include "parser.h"

static bool writeTextFile(const std::string& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << text;
    return out.good();
}

int main(int argc, char* argv[])
{
    std::string inputFile = "schema.xml";
    std::string modelName = "model";

    if (argc >= 2) {
        inputFile = argv[1];
    }
    if (argc >= 3) {
        modelName = argv[2];
    }

    try {
        Schema schema = parseSchema(inputFile);
        GeneratedFiles files = generateCCode(schema, modelName);

        if (!writeTextFile(files.headerName, files.headerText)) {
            std::cerr << "Failed to write file: " << files.headerName << "\n";
            return 1;
        }

        if (!writeTextFile(files.sourceName, files.sourceText)) {
            std::cerr << "Failed to write file: " << files.sourceName << "\n";
            return 1;
        }

        std::cout << "Generated:\n";
        std::cout << "  " << files.headerName << "\n";
        std::cout << "  " << files.sourceName << "\n";
    }
    catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}