#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <fstream>

struct Entry{
    int id;
    int level;
    std::string name;
    std::string type;
    std::string directParent;
    std::vector<std::string> inlineValues;
    std::string value;
};

int brackets = 0;

std::string replaceSingleQuotes(const std::string& input) {
    std::string output = input;
    for (size_t i = 0; i < output.size(); ++i) {
        if (output[i] == '\'') {
            output[i] = '"';
        }
    }
    return output;
}

std::string normalizeAttr(std::string input)
{
    input = replaceSingleQuotes(input);

    //  SystemTime="2025-10-20T13:35:38.8040065Z"/  ->  "SystemTime":"2025-10-20T13:35:38.8040065Z"
    int delimiter = input.find('=');
    std::string modified = "\"";

    if (delimiter == std::string::npos) {
        return input;
    }

    modified += input.replace(delimiter, 1, "\":\"");  // replace '=' with "':"

    if (!modified.empty() && modified.back() == '/') {
        modified = modified.substr(0, modified.size() - 1);
    }

    modified += "\"";

    // std::cout << "Modified output: " << modified << std::endl;

    return modified;
}

std::vector<std::string> fetchInlineValues(std::string currentKey) 
{
    std::vector<std::string> values;
    std::vector<std::string> returnV;
    std::string tmp;
    bool open = false;

    for (const char chr : currentKey) {
        if (chr == '/') continue;
        
        if (chr == ' ' && !open) {
            if (tmp.size() > 1) {
                values.push_back(tmp);
                tmp.clear();
            }
        } else if (chr == '\'' && !open) {
            open = true;
        } else if (chr == '\'' && open) {
            values.push_back(tmp);
            tmp.clear();
            open = false;
        } else {
            tmp += chr;
        }
    }

    if (!tmp.empty()) {
        values.push_back(tmp);
    }

    for (int i = 2; i < values.size(); i++) {
        std::string value = values[i];
        returnV.push_back(normalizeAttr("_" + values[i]));
    }

    return returnV;
}

std::string lastValue(std::vector<std::string> collection, int currentCounter) 
{
    std::string lastValue;
    for (int l = currentCounter - 1; l > 0; l--) {
        if (collection[l][0] == 'o') {
            lastValue = collection[l]; 
            break;
        };
    }

    // if no parent element is found, root is set as parent
    if (lastValue.empty()) lastValue = collection[0];

    return lastValue;
}

std::vector<Entry> findKeys(std::string xmlStr)
{
    std::vector<Entry> finding;
    std::vector<std::string> collection;

    std::string currentKey;
    std::string value;
    std::string lastOpened;
    
    int level = 1;
    bool start = false;
    
    for (int i = 0; i < xmlStr.size(); i++) {
        char chr = xmlStr[i];
        
        if (chr == '<') {
            if (!value.empty()) {
                collection.push_back("v: " + value);
            }
            value.clear();
            start = true;
            continue;

        // closing bracket, so key ends
        } else if (chr == '>') {
            if (!currentKey.empty()) {
                // Self-closing tag: <Tag/>
                if (currentKey.back() == '/') {
                    collection.push_back("s: " + currentKey);
                    lastOpened.clear();
                }
                // Closing tag: </Tag>
                else if (currentKey[0] == '/') {
                    collection.push_back("c: " + currentKey);
                    lastOpened.clear();
                }
                // Opening tag: <Tag>
                else {
                    lastOpened = currentKey;
                    collection.push_back("o: " + currentKey);
                }
            }
            
            currentKey.clear();
            start = false;
            continue;
        }

        if (start == true) {
            currentKey += chr;
        }

        if (start == false) {
            value += chr;
        }
    }

    for (int i = 0; i < collection.size(); i++) {
        std::string element = collection[i];
        char identifier = element[0];
        
        // o = opening; c = closing; s = selfclosing; v = value
        if (identifier == 's') {
            std::vector<std::string> inlineValues = fetchInlineValues(element);
            std::string parent = lastValue(collection, i).substr(3);
            std::string cleanedChild;

            if (element[element.size() - 1] == '/') {
                cleanedChild = element.substr(3, element.size() - 1);
            } else {
                cleanedChild = element.substr(3);
            }

            if (cleanedChild.find(' ') != std::string::npos) {
                cleanedChild = cleanedChild.substr(0, cleanedChild.find(' '));
            }

            finding.push_back({i, level, cleanedChild, "sc", parent, inlineValues, ""});
        } else if (identifier == 'o') {
            std::string value = "";
            std::string parent = lastValue(collection, i).substr(3);
            std::string cleanedChild = element.substr(3);

            if (collection[i + 1][0] == 'v') {
                value = collection[i + 1].substr(3);
            } else {
                value = "{";
                brackets++;
            }

            if (cleanedChild.find(' ') != std::string::npos) { 
                cleanedChild = cleanedChild.substr(0, cleanedChild.find(' '));
            }

            finding.push_back({i, level, cleanedChild, "op", parent, fetchInlineValues(element), value});

            level++;
        } else if (identifier == 'c') {
            std::string parent = lastValue(collection, i).substr(3);

            finding.push_back({i, level, element, "cl", parent, fetchInlineValues(element), ""});

            level--;
        }
        
    }

    // for (const auto line : finding) {
    //     std::cout << "ID: " << line.id << 
    //     std::endl << "Level: " << line.level << 
    //     std::endl << "DirectParent: " << line.directParent << 
    //     std::endl << "Name: " << line.name << 
    //     std::endl << "Value: " << line.value << 
    //     std::endl << "Type: " << line.type << std::endl;
    //     if (line.inlineValues.size() > 0) {
    //         std::cout << "inline values: " << std::endl;
    //         for (const auto val : line.inlineValues) {
    //             std::cout << val << std::endl;
    //         } 
    //     }
    //     std::cout << std::endl;
    // }

    return finding;
}

std::string escapeOutput(std::string input)
{
    std::regex trailingCommaPattern(",(\\s*})");
    std::regex newLine("\n");
    std::regex backslash(R"(\\)");

    input = std::regex_replace(input, trailingCommaPattern, "$1");
    input = std::regex_replace(input, newLine, "$1");
    input = std::regex_replace(input, backslash, "\\\\");

    return input;
}

std::string findSameParams(std::string input) 
{
    int pos1 = input.find(':');

    if (pos1 == std::string::npos) return "";

    return input.substr(0, pos1);
}

std::string buildString(std::vector<Entry> objects) 
{
    std::string jsonString;
    int squareBrackets = 0;
    int highest = 0;

    for (int i = 0; i < objects.size(); i++) {
        Entry line = objects[i];

        if (line.id == 0) {
            jsonString += "{\"" + line.name + "\": " + line.value + line.inlineValues[0] + ", ";
            continue;
        }

        int previous = objects[i - 1].level;

        if (line.type == "sc") {
            if (line.name[line.name.size() - 1] == '/') {
                jsonString += "\"" + line.name + "\": {},\n";
                continue;
            } else {
                jsonString += "\"" + line.name + "\": {";
                for (const auto attr : line.inlineValues) {
                    jsonString += attr + ",";
                }
                // to remove trailing comma ,
                jsonString = jsonString.substr(0, jsonString.length() - 1) + "},\n";
            }


        } else if (line.type == "op") {
            std::string valuePart;
            if (line.inlineValues.size() > 0 && line.value != "" && line.value != "{") {
                // if inlinearg and value
                // inline arg has the name provided and the value has the name _value

                if (line.inlineValues.size() == 1) {
                    // Check if we can safely access objects[i + 2]
                    if (i + 2 < objects.size()) {
                        Entry next = objects[i + 2];
                        
                        // Check if we can safely access objects[i - 2]
                        if (i >= 2) {  // i-2 is safe when i >= 2
                            Entry last = objects[i - 2];

                            if (next.inlineValues.size() != 0 && 
                                findSameParams(line.inlineValues[0]) == findSameParams(next.inlineValues[0])) {
                                    if (jsonString[jsonString.size() - 2] == '{') jsonString = jsonString.substr(0, jsonString.size() - 2) + "[";
                                    jsonString += "{" + line.inlineValues[0] + ",\"_value\":\"" + line.value + "\"},"; 
                            } else if (!last.inlineValues.empty() && !last.inlineValues[0].empty()) {
                                if (findSameParams(line.inlineValues[0]) == findSameParams(last.inlineValues[0])) {
                                    jsonString += "{" + line.inlineValues[0] + ",\"_value\":\"" + line.value + "\"}]"; 
                                }
                            }
                        }
                    }
                }           
            } else {
                if (line.value == "{") {
                    valuePart = "\": " + line.value + "\n";
                } else {
                    valuePart = "\": \"" + line.value + "\",\n";
                }
                jsonString += "\"" + line.name + valuePart;
            }
        } 
        
        if (line.type == "op" && objects[i + 1].level < previous) {
            jsonString += "}";
        }


        if (i != objects.size() - 1) {
            if (line.type == "cl" && objects[i + 1].level < previous) {
                jsonString += "},\n";
            }
        } 

        if (line.level > highest) highest = line.level;
    }

    int length = objects[objects.size() - 1].level;

    for (int i = 0; i < length - squareBrackets; i++) {
        jsonString += "}";
    }

    return escapeOutput(jsonString);
}

int main()
{

    std::string rawData = "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='Service Control Manager' Guid='{555908d1-a6d7-4695-8e1e-26931d2012f4}' EventSourceName='Service Control Manager'/><EventID Qualifiers='16384'>7040</EventID><Version>0</Version><Level>4</Level><Task>0</Task><Opcode>0</Opcode><Keywords>0x8080000000000000</Keywords><TimeCreated SystemTime='2025-10-22T15:21:51.1582989Z'/><EventRecordID>38311</EventRecordID><Correlation ActivityID='{8fbecd7e-8ba9-451e-ae90-5edfd451499b}'/><Execution ProcessID='2252' ThreadID='33304'/><Channel>System</Channel><Computer>DESKTOP-SV73V84</Computer><Security UserID='S-1-5-18'/></System><EventData><Data Name='param1'>Background Intelligent Transfer Service</Data><Data Name='param2'>Automatisch starten</Data><Data Name='param3'>Manuell starten</Data><Data Name='param4'>BITS</Data></EventData></Event>";

    std::string rawData = "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='nhi'/><EventID Qualifiers='16388'>9007</EventID><Version>0</Version><Level>4</Level><Task>0</Task><Opcode>0</Opcode><Keywords>0x80000000000000</Keywords><TimeCreated SystemTime='2025-10-21T06:12:26.5170813Z'/><EventRecordID>37275</EventRecordID><Correlation/><Execution ProcessID='4' ThreadID='440'/><Channel>System</Channel><Computer>DESKTOP-SV73V84</Computer><Security/></System><EventData><Data></Data><Binary>0000000001000000000000002F230440000000000000000000000000000000000000000000000000</Binary></EventData></Event>";


    std::vector<Entry> collection = findKeys(rawData);

    std::string json = buildString(collection);

    std::cout << json << std::endl;

    
    return 0;
}