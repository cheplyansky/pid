#pragma once
#include <string>
#include <vector>

struct Port {
    int    portNumber = 0;
    std::string name;
};

struct Block {
    std::string              blockType;
    std::string              name;
    int                      sid = 0;
    std::vector<int>         position;   // [x1, y1, x2, y2]

    // Специфичные параметры — храним как key→value
    std::string              gain;       // для Gain
    std::string              inputs;     // для Sum ("+-", "++" и т.п.)
    std::string              iconShape;  // для Sum
    std::string              ports;      // "[2,1]" и т.п.
    std::string              sampleTime; // для UnitDelay
    int                      portIndex = 1; // для Inport/Outport

    std::vector<Port>        portDefs;   // дочерние <Port>
};

// Конечная точка соединения: SID блока + номер порта
struct Endpoint {
    int         sid      = 0;
    std::string portDir;   // "in" или "out"
    int         portNum  = 1;
};

struct Branch {
    Endpoint dst;
    // points не нужны для генерации кода — пропускаем
};

struct Line {
    std::string         name;
    Endpoint            src;
    Endpoint            dst;           // если нет веток
    std::vector<Branch> branches;      // если есть ветвление
};

struct Schema {
    std::vector<Block> blocks;
    std::vector<Line>  lines;
};