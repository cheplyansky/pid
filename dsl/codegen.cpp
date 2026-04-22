#include "codegen.h"

#include <algorithm>
#include <cctype>
#include <queue>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace {

struct Edge
{
    int srcSid = 0;
    int srcPort = 1;
    int dstSid = 0;
    int dstPort = 1;
};

static std::string sanitizeName(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    for (char ch : s) {
        unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) || ch == '_')
            out += ch;
        else
            out += '_';
    }

    if (out.empty())
        out = "unnamed";

    if (std::isdigit(static_cast<unsigned char>(out[0])))
        out = "_" + out;

    return out;
}

static std::string upperName(const std::string& s)
{
    std::string out = s;
    for (char& ch : out)
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return out;
}

static std::string modelStructName(const std::string& modelName)
{
    return sanitizeName(modelName) + "_Model";
}

static std::string extPortTypeName(const std::string& modelName)
{
    return sanitizeName(modelName) + "_ExtPort";
}

static std::string functionPrefix(const std::string& modelName)
{
    return sanitizeName(modelName);
}

static std::string blockVarName(const Block& b)
{
    if (!b.name.empty())
        return sanitizeName(b.name);
    return "sid_" + std::to_string(b.sid);
}

static const Block* findBlockBySid(const Schema& schema, int sid)
{
    for (const auto& b : schema.blocks) {
        if (b.sid == sid)
            return &b;
    }
    return nullptr;
}

static bool isInport(const Block& b)
{
    return b.blockType == "Inport";
}

static bool isOutport(const Block& b)
{
    return b.blockType == "Outport";
}

static bool isGain(const Block& b)
{
    return b.blockType == "Gain";
}

static bool isSum(const Block& b)
{
    return b.blockType == "Sum";
}

static bool isUnitDelay(const Block& b)
{
    return b.blockType == "UnitDelay";
}

static bool isComputationalBlock(const Block& b)
{
    return isGain(b) || isSum(b);
}

static void addEdge(std::vector<Edge>& edges, int srcSid, int srcPort, int dstSid, int dstPort)
{
    Edge e;
    e.srcSid = srcSid;
    e.srcPort = srcPort;
    e.dstSid = dstSid;
    e.dstPort = dstPort;
    edges.push_back(e);
}

static std::vector<Edge> flattenEdges(const Schema& schema)
{
    std::vector<Edge> edges;

    for (const auto& line : schema.lines) {
        if (!line.branches.empty()) {
            for (const auto& br : line.branches) {
                addEdge(edges, line.src.sid, line.src.portNum, br.dst.sid, br.dst.portNum);
            }
        } else if (line.dst.sid != 0) {
            addEdge(edges, line.src.sid, line.src.portNum, line.dst.sid, line.dst.portNum);
        }
    }

    return edges;
}

static const Edge* findInputEdge(const std::vector<Edge>& edges, int dstSid, int dstPort)
{
    for (const auto& e : edges) {
        if (e.dstSid == dstSid && e.dstPort == dstPort)
            return &e;
    }
    return nullptr;
}

static std::string exprForBlockOutput(const Block& srcBlock, const std::string& instName)
{
    return instName + "." + blockVarName(srcBlock);
}

static std::string inputExpr(const Schema& schema,
                             const std::vector<Edge>& edges,
                             const Block& block,
                             int inputPort,
                             const std::string& instName)
{
    const Edge* e = findInputEdge(edges, block.sid, inputPort);
    if (!e)
        return "0.0";

    const Block* src = findBlockBySid(schema, e->srcSid);
    if (!src)
        return "0.0";

    return exprForBlockOutput(*src, instName);
}

static int sumInputCount(const Block& b, const std::vector<Edge>& edges)
{
    if (!b.inputs.empty())
        return static_cast<int>(b.inputs.size());

    int maxPort = 0;
    for (const auto& e : edges) {
        if (e.dstSid == b.sid)
            maxPort = std::max(maxPort, e.dstPort);
    }

    return std::max(1, maxPort);
}

static std::string generateSumExpr(const Schema& schema,
                                   const std::vector<Edge>& edges,
                                   const Block& block,
                                   const std::string& instName)
{
    int n = sumInputCount(block, edges);
    std::ostringstream out;

    for (int port = 1; port <= n; ++port) {
        std::string expr = inputExpr(schema, edges, block, port, instName);

        char sign = '+';
        if (!block.inputs.empty() && port - 1 < static_cast<int>(block.inputs.size()))
            sign = block.inputs[port - 1];

        if (port == 1) {
            if (sign == '-')
                out << "(-" << expr << ")";
            else
                out << expr;
        } else {
            if (sign == '-')
                out << " - " << expr;
            else
                out << " + " << expr;
        }
    }

    return out.str();
}

static std::vector<const Block*> topoSortComputationalBlocks(const Schema& schema,
                                                             const std::vector<Edge>& edges)
{
    std::unordered_map<int, const Block*> compBlocks;
    for (const auto& b : schema.blocks) {
        if (isComputationalBlock(b))
            compBlocks[b.sid] = &b;
    }

    std::unordered_map<int, int> indegree;
    std::unordered_map<int, std::vector<int>> adj;

    for (const auto& kv : compBlocks)
        indegree[kv.first] = 0;

    for (const auto& e : edges) {
        auto itDst = compBlocks.find(e.dstSid);
        if (itDst == compBlocks.end())
            continue;

        const Block* srcBlock = findBlockBySid(schema, e.srcSid);
        if (!srcBlock)
            continue;

        if (isComputationalBlock(*srcBlock)) {
            adj[srcBlock->sid].push_back(e.dstSid);
            indegree[e.dstSid]++;
        }
    }

    std::queue<int> q;
    for (const auto& kv : indegree) {
        if (kv.second == 0)
            q.push(kv.first);
    }

    std::vector<const Block*> result;
    while (!q.empty()) {
        int sid = q.front();
        q.pop();

        result.push_back(compBlocks[sid]);

        for (int to : adj[sid]) {
            indegree[to]--;
            if (indegree[to] == 0)
                q.push(to);
        }
    }

    if (result.size() != compBlocks.size()) {
        for (const auto& b : schema.blocks) {
            if (isComputationalBlock(b)) {
                bool found = false;
                for (const Block* rb : result) {
                    if (rb->sid == b.sid) {
                        found = true;
                        break;
                    }
                }
                if (!found)
                    result.push_back(&b);
            }
        }
    }

    return result;
}

static std::vector<const Block*> collectBlocks(const Schema& schema, bool (*pred)(const Block&))
{
    std::vector<const Block*> result;
    for (const auto& b : schema.blocks) {
        if (pred(b))
            result.push_back(&b);
    }
    return result;
}

static std::string headerGuard(const std::string& modelName)
{
    return upperName(sanitizeName(modelName)) + "_GENERATED_H";
}

static std::string makeHeaderText(const Schema& schema, const std::string& modelName)
{
    const std::string guard = headerGuard(modelName);
    const std::string structName = modelStructName(modelName);
    const std::string extPortName = extPortTypeName(modelName);
    const std::string prefix = functionPrefix(modelName);

    std::ostringstream out;

    out << "#pragma once\n";
    out << "#ifndef " << guard << "\n";
    out << "#define " << guard << "\n\n";

    out << "#ifdef __cplusplus\n";
    out << "extern \"C\" {\n";
    out << "#endif\n\n";

    out << "typedef struct {\n";
    out << "    const char* name;\n";
    out << "    double* ptr;\n";
    out << "    int dir; /* 0=out, 1=in */\n";
    out << "} " << extPortName << ";\n\n";

    out << "typedef struct " << structName << " " << structName << ";\n\n";

    out << "struct " << structName << " {\n";
    for (const auto& b : schema.blocks) {
        if (isInport(b) || isComputationalBlock(b) || isUnitDelay(b))
            out << "    double " << blockVarName(b) << ";\n";
    }
    out << "};\n\n";

    out << "void " << prefix << "_init(" << structName << "* model);\n";
    out << "void " << prefix << "_step(" << structName << "* model);\n";
    out << "const " << extPortName << "* " << prefix << "_get_ext_ports(" << structName << "* model);\n\n";

    out << "#ifdef __cplusplus\n";
    out << "}\n";
    out << "#endif\n\n";

    out << "#endif\n";

    return out.str();
}

static std::string makeSourceText(const Schema& schema, const std::string& modelName)
{
    const std::string headerFile = sanitizeName(modelName) + "_generated.h";
    const std::string structName = modelStructName(modelName);
    const std::string extPortName = extPortTypeName(modelName);
    const std::string prefix = functionPrefix(modelName);

    const auto edges = flattenEdges(schema);
    const auto compBlocks = topoSortComputationalBlocks(schema, edges);
    const auto unitDelayBlocks = collectBlocks(schema, isUnitDelay);
    const auto outportBlocks = collectBlocks(schema, isOutport);
    const auto inportBlocks = collectBlocks(schema, isInport);

    std::ostringstream out;

    out << "#include \"" << headerFile << "\"\n\n";

    out << "void " << prefix << "_init(" << structName << "* model)\n";
    out << "{\n";
    out << "    if (!model) return;\n";

    for (const auto& b : schema.blocks) {
        if (isInport(b) || isComputationalBlock(b) || isUnitDelay(b))
            out << "    model->" << blockVarName(b) << " = 0.0;\n";
    }

    out << "}\n\n";

    out << "void " << prefix << "_step(" << structName << "* model)\n";
    out << "{\n";
    out << "    if (!model) return;\n\n";

    for (const Block* b : compBlocks) {
        const std::string lhs = "model->" + blockVarName(*b);

        if (isGain(*b)) {
            std::string in = inputExpr(schema, edges, *b, 1, "(*model)");
            std::string gain = b->gain.empty() ? "1.0" : b->gain;
            out << "    " << lhs << " = " << in << " * (" << gain << ");\n";
        } else if (isSum(*b)) {
            std::string expr = generateSumExpr(schema, edges, *b, "(*model)");
            out << "    " << lhs << " = " << expr << ";\n";
        }
    }

    if (!unitDelayBlocks.empty()) {
        out << "\n";
        for (const Block* b : unitDelayBlocks) {
            std::string in = inputExpr(schema, edges, *b, 1, "(*model)");
            out << "    model->" << blockVarName(*b) << " = " << in << ";\n";
        }
    }

    out << "}\n\n";

    out << "const " << extPortName << "* " << prefix << "_get_ext_ports(" << structName << "* model)\n";
    out << "{\n";
    out << "    static " << extPortName << " ports[" << (inportBlocks.size() + outportBlocks.size() + 1) << "];\n";
    out << "    int i = 0;\n\n";

    for (const Block* b : inportBlocks) {
        out << "    ports[i].name = \"" << (b->name.empty() ? ("sid_" + std::to_string(b->sid)) : b->name) << "\";\n";
        out << "    ports[i].ptr = model ? &model->" << blockVarName(*b) << " : 0;\n";
        out << "    ports[i].dir = 1;\n";
        out << "    ++i;\n\n";
    }

    for (const Block* b : outportBlocks) {
        const Edge* e = findInputEdge(edges, b->sid, 1);
        const Block* src = e ? findBlockBySid(schema, e->srcSid) : nullptr;

        out << "    ports[i].name = \"" << (b->name.empty() ? ("sid_" + std::to_string(b->sid)) : b->name) << "\";\n";
        if (src)
            out << "    ports[i].ptr = model ? &model->" << blockVarName(*src) << " : 0;\n";
        else
            out << "    ports[i].ptr = 0;\n";
        out << "    ports[i].dir = 0;\n";
        out << "    ++i;\n\n";
    }

    out << "    ports[i].name = 0;\n";
    out << "    ports[i].ptr = 0;\n";
    out << "    ports[i].dir = 0;\n";
    out << "    return ports;\n";
    out << "}\n";

    return out.str();
}

} // namespace

GeneratedFiles generateCCode(const Schema& schema, const std::string& modelName)
{
    GeneratedFiles files;
    files.headerName = sanitizeName(modelName) + "_generated.h";
    files.sourceName = sanitizeName(modelName) + "_generated.c";
    files.headerText = makeHeaderText(schema, modelName);
    files.sourceText = makeSourceText(schema, modelName);
    return files;
}