#include "parser.h"

#include <tinyxml2.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace tinyxml2;

namespace {

std::string getAttr(XMLElement* elem, const char* name)
{
    if (!elem) return {};
    const char* v = elem->Attribute(name);
    return v ? std::string(v) : std::string{};
}

std::string getText(XMLElement* elem)
{
    if (!elem) return {};
    const char* t = elem->GetText();
    return t ? std::string(t) : std::string{};
}

std::string trim(const std::string& s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
        ++b;
    }

    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
        --e;
    }

    return s.substr(b, e - b);
}

int toInt(const std::string& s, const std::string& what)
{
    std::string t = trim(s);
    if (t.empty()) {
        throw std::runtime_error("Missing integer value for " + what);
    }

    try {
        size_t pos = 0;
        int v = std::stoi(t, &pos);
        if (pos != t.size()) {
            throw std::runtime_error("");
        }
        return v;
    } catch (...) {
        throw std::runtime_error("Invalid integer for " + what + ": " + s);
    }
}

std::string getChildPValue(XMLElement* parent, const char* pname)
{
    if (!parent) return {};

    for (XMLElement* p = parent->FirstChildElement("P");
         p != nullptr;
         p = p->NextSiblingElement("P"))
    {
        const char* attr = p->Attribute("Name");
        if (attr && std::string(attr) == pname) {
            return getText(p);
        }
    }
    return {};
}

std::vector<int> parsePosition(const std::string& text)
{
    std::vector<int> result;
    std::string s = trim(text);

    if (s.empty()) {
        return result;
    }

    if (!s.empty() && s.front() == '[') s.erase(s.begin());
    if (!s.empty() && s.back() == ']') s.pop_back();

    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, ',')) {
        item = trim(item);
        if (!item.empty()) {
            result.push_back(toInt(item, "Position"));
        }
    }

    return result;
}

Endpoint parseEndpoint(const std::string& text)
{
    Endpoint ep{};

    std::string s = trim(text);
    if (s.empty()) {
        throw std::runtime_error("Empty endpoint");
    }

    auto hashPos = s.find('#');
    if (hashPos == std::string::npos) {
        throw std::runtime_error("Endpoint missing '#': " + s);
    }

    ep.sid = toInt(s.substr(0, hashPos), "endpoint sid");

    std::string rest = s.substr(hashPos + 1);
    auto colonPos = rest.find(':');

    if (colonPos == std::string::npos) {
        ep.portDir = trim(rest);
        ep.portNum = 1;
        return ep;
    }

    ep.portDir = trim(rest.substr(0, colonPos));
    ep.portNum = toInt(rest.substr(colonPos + 1), "endpoint port");

    return ep;
}

Port parsePort(XMLElement* portElem)
{
    Port p{};

    p.name = getAttr(portElem, "Name");

    std::string num = getAttr(portElem, "PortNumber");
    if (num.empty()) {
        num = getAttr(portElem, "Port");
    }
    if (!num.empty()) {
        p.portNumber = toInt(num, "Port.PortNumber");
    }

    return p;
}

Branch parseBranch(XMLElement* branchElem)
{
    Branch br{};

    std::string dstText = getChildPValue(branchElem, "Dst");
    if (!dstText.empty()) {
        br.dst = parseEndpoint(dstText);
    }

    return br;
}

void parseBlockInto(XMLElement* elem, Schema& schema)
{
    Block b{};

    b.sid       = toInt(getAttr(elem, "SID"), "Block SID");
    b.name      = getAttr(elem, "Name");
    b.blockType = getAttr(elem, "BlockType");

    b.gain       = getChildPValue(elem, "Gain");
    b.inputs     = getChildPValue(elem, "Inputs");
    b.iconShape  = getChildPValue(elem, "IconShape");
    b.ports      = getChildPValue(elem, "Ports");
    b.sampleTime = getChildPValue(elem, "SampleTime");

    std::string pos = getChildPValue(elem, "Position");
    if (!pos.empty()) {
        b.position = parsePosition(pos);
    }

    if ((b.blockType == "Inport" || b.blockType == "Outport")) {
        std::string portIndexText = getChildPValue(elem, "Port");
        if (!portIndexText.empty()) {
            b.portIndex = toInt(portIndexText, "Block Port");
        }
    }

    for (XMLElement* child = elem->FirstChildElement("Port");
         child != nullptr;
         child = child->NextSiblingElement("Port"))
    {
        b.portDefs.push_back(parsePort(child));
    }

    schema.blocks.push_back(std::move(b));
}

void parseLineInto(XMLElement* elem, Schema& schema)
{
    Line line{};

    line.name = getAttr(elem, "Name");

    std::string srcText = getChildPValue(elem, "Src");
    std::string dstText = getChildPValue(elem, "Dst");

    if (!srcText.empty()) {
        line.src = parseEndpoint(srcText);
    }
    if (!dstText.empty()) {
        line.dst = parseEndpoint(dstText);
    }

    for (XMLElement* br = elem->FirstChildElement("Branch");
         br != nullptr;
         br = br->NextSiblingElement("Branch"))
    {
        line.branches.push_back(parseBranch(br));
    }

    schema.lines.push_back(std::move(line));
}

void parseContainerChildren(XMLElement* parent, Schema& schema)
{
    if (!parent) return;

    for (XMLElement* elem = parent->FirstChildElement();
         elem != nullptr;
         elem = elem->NextSiblingElement())
    {
        std::string tag = elem->Name();

        if (tag == "Block") {
            parseBlockInto(elem, schema);
        } else if (tag == "Line") {
            parseLineInto(elem, schema);
        } else if (tag == "System") {
            parseContainerChildren(elem, schema);
        }
    }
}

XMLElement* findModelOrSystemRoot(XMLDocument& doc)
{
    XMLElement* root = doc.RootElement();
    if (!root) {
        return nullptr;
    }

    if (std::string(root->Name()) == "Model" || std::string(root->Name()) == "System") {
        return root;
    }

    for (XMLElement* e = root->FirstChildElement(); e != nullptr; e = e->NextSiblingElement()) {
        std::string tag = e->Name();
        if (tag == "Model" || tag == "System") {
            return e;
        }
    }

    return root;
}

} // namespace

Schema parseSchema(const std::string& filename)
{
    // Чтение файла
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string xmlContent = buffer.str();
    file.close();
    
    if (xmlContent.empty()) {
        throw std::runtime_error("File is empty: " + filename);
    }
    
    // Парсинг XML
    XMLDocument doc;
    XMLError err = doc.Parse(xmlContent.c_str());
    
    if (err != XML_SUCCESS) {
        throw std::runtime_error("XML parse error in file: " + filename + 
                                " (error code: " + std::to_string(err) + ")");
    }
    
    XMLElement* root = findModelOrSystemRoot(doc);
    if (!root) {
        throw std::runtime_error("No root XML element (Model or System) found in: " + filename);
    }
    
    Schema schema;
    parseContainerChildren(root, schema);
    
    return schema;
}