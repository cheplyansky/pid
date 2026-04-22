#pragma once

#include <string>
#include "schema.h"

struct GeneratedFiles {
    std::string headerName;
    std::string sourceName;
    std::string headerText;
    std::string sourceText;
};

GeneratedFiles generateCCode(const Schema& schema, const std::string& modelName);