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

std::string escapeOutput(std::string input)
{
    std::regex leadingCommaAfterBrace(R"(\{,)");
    std::regex backslash(R"(\\)");
    std::regex trailingCommaPattern(",(\\s*})");
    std::regex newLine("\n");

    input = std::regex_replace(input, trailingCommaPattern, "$1");
    input = std::regex_replace(input, newLine, "$1");
    input = std::regex_replace(input, backslash, "\\");
    input = std::regex_replace(input, leadingCommaAfterBrace, "{");

    return input;
}

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

    return modified;
}

std::vector<std::string> fetchInlineValues(std::string currentKey) 
{
    std::vector<std::string> values;
    std::vector<std::string> returnV;
    std::string tmp;
    bool open = false;

    for (const char chr : currentKey) {
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

    if (tmp.size() > 1) {
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

    for (int i = 0; i < finding.size(); i++) {
        if (finding[i].type == "cl") {
            if (i > 0 && finding[i - 1].value == "{") {
                finding[i - 1].value = "";
            }
        }

        Entry line = finding[i];

        std::cout << "ID: " << line.id << std::endl
                    << "Level: " << line.level << std::endl
                    << "DirectParent: " << line.directParent << std::endl
                    << "Name: " << line.name << std::endl
                    << "Value: " << line.value << std::endl
                    << "Type: " << line.type << std::endl;
            if (!line.inlineValues.empty()) {
                std::cout << "inline values: " << std::endl;
                for (const auto& val : line.inlineValues) {
                    std::cout << val << std::endl;
                } 
            }
            std::cout << std::endl;
    }

    return finding;
}

std::string findSameParams(std::string input) 
{
    int pos1 = input.find(':');

    if (pos1 == std::string::npos) return "";

    return input.substr(0, pos1);
}

std::string buildString(std::vector<Entry> objects) 
{
    std::cout << "Starting build" << std::endl;
    std::string jsonString;
    int squareBrackets = 0;
    int highest = 0;

    for (int i = 0; i < objects.size(); i++) {
        Entry line = objects[i];


        if (line.id == 0) {
            jsonString += "{\"" + line.name + "\": " + line.value + line.inlineValues[0] + ", ";
            std::cout << "ROOT: " << line.name << std::endl;
            continue;
        } 

        int previous = objects[i - 1].level;

        if (line.type == "sc") {
            if (line.inlineValues.empty()) {
                jsonString += "\"" + line.name.substr(0, line.name.size() - 1) + "\": \"\",";
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
                        if (i >= 2) {
                            Entry last = objects[i - 2];

                            if (next.inlineValues.size() != 0 && findSameParams(line.inlineValues[0]) == findSameParams(next.inlineValues[0])) {
                                std::cout << "tack" << std::endl;
                                    if (jsonString[jsonString.size() - 2] == '{') jsonString = jsonString.substr(0, jsonString.size() - 2) + "[";
                                    jsonString += "{" + line.inlineValues[0] + ",\"_Value\":\"" + line.value + "\"},"; 
                            } else if (!last.inlineValues.empty() && !last.inlineValues[0].empty()) {
                                if (findSameParams(line.inlineValues[0]) == findSameParams(last.inlineValues[0])) {
                                    jsonString += "{" + line.inlineValues[0] + ",\"_Value\":\"" + line.value + "\"}]"; 
                                    squareBrackets++;
                                }
                            } else {
                                std::cout << "tick" << std::endl;
                                jsonString += "\"" + line.name + "\": {";
                                jsonString += "\"_Value\":\"" + line.value + "\"";
                                if (!line.inlineValues.empty()) {
                                    jsonString += ",";
                                    for (int j = 0; j < line.inlineValues.size(); j++) {
                                        if (j != line.inlineValues.size() - 1) {
                                            jsonString += line.inlineValues[j] + ",";
                                        } else {
                                            jsonString += line.inlineValues[j];
                                        }
                                    }
                                }
                                jsonString += "},";
                            }
                        }
                    }
                }           
            } else {
                std::string firstPart = "\"" + line.name;
                if (line.value == "{") {
                    valuePart = firstPart + "\": " + line.value + "\n";
                } else {
                    if (i >= 2 && i + 2 < objects.size() & !line.inlineValues.empty()) {
                        Entry last = objects[i - 2];
                        Entry next = objects[i + 2];
                        if (next.inlineValues.size() != 0 && findSameParams(last.inlineValues[0]) == findSameParams(next.inlineValues[0]) && findSameParams(next.inlineValues[0]) != findSameParams(line.inlineValues[0])) {
                            valuePart.clear();
                            continue;
                        }
                    } else {
                        std::cout << line.name << line.value << std::endl;
                        valuePart = firstPart + "\": \"" + line.value + "\",\n";
                    }
                   
                }
                jsonString += valuePart;
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
    // std::vector<std::string> payloads;

    // payloads.push_back("<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='Service Control Manager' Guid='{555908d1-a6d7-4695-8e1e-26931d2012f4}' EventSourceName='Service Control Manager'/><EventID Qualifiers='16384'>7040</EventID><Version>0</Version><Level>4</Level><Task>0</Task><Opcode>0</Opcode><Keywords>0x8080000000000000</Keywords><TimeCreated SystemTime='2025-10-22T15:21:51.1582989Z'/><EventRecordID>38311</EventRecordID><Correlation ActivityID='{8fbecd7e-8ba9-451e-ae90-5edfd451499b}'/><Execution ProcessID='2252' ThreadID='33304'/><Channel>System</Channel><Computer>DESKTOP-SV73V84</Computer><Security UserID='S-1-5-18'/></System><EventData><Data Name='param1'>Background Intelligent Transfer Service</Data><Data Name='param2'>Automatisch starten</Data><Data Name='param3'>Manuell starten</Data><Data Name='param4'>BITS</Data></EventData></Event>");
    std::string rawData = "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='nhi'/><EventID Qualifiers='16388'>9007</EventID><Version>0</Version><Level>4</Level><Task>0</Task><Opcode>0</Opcode><Keywords>0x80000000000000</Keywords><TimeCreated SystemTime='2025-10-21T06:12:26.5170813Z'/><EventRecordID>37275</EventRecordID><Correlation/><Execution ProcessID='4' ThreadID='440'/><Channel>System</Channel><Computer>DESKTOP-SV73V84</Computer><Security/></System><EventData><Data></Data><Binary>0000000001000000000000002F230440000000000000000000000000000000000000000000000000</Binary></EventData></Event>";

    // std::string rawData = "<Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='Microsoft-Windows-Hyper-V-VmSwitch' Guid='{67dc0d66-3695-47c0-9642-33f76f7bd7ad}'/><EventID>291</EventID><Version>0</Version><Level>4</Level><Task>0</Task><Opcode>0</Opcode><Keywords>0x8000000000000080</Keywords><TimeCreated SystemTime='2025-10-22T06:16:55.8917757Z'/><EventRecordID>38075</EventRecordID><Correlation/><Execution ProcessID='28312' ThreadID='27376'/><Channel>System</Channel><Computer>DESKTOP-SV73V84</Computer><Security UserID='S-1-5-83-1-3073874882-1258444484-1201153155-2920543076'/></System><EventData><Data Name='NicName'>B7379BC2-56C4-4B02-8324-984764F313AE--4B480FFB-2CD7-4F0F-B6AD-B956C0D19EDE</Data><Data Name='NicFName'></Data><Data Name='OldNicIPv4RscEnabled'>0</Data><Data Name='OldNicIPv6RscEnabled'>0</Data><Data Name='NewNicIPv4RscEnabled'>0</Data><Data Name='NewNicIPv6RscEnabled'>0</Data><Data Name='RscStateModifiedReason'>3</Data></EventData></Event>";


    std::ifstream input("logs\\SV73V84\\2025-10-22_17-36-20_System.json");
    std::ofstream output("logs\\output\\test.json");
    std::string line;

    // while(std::getline(input, line)) {
    //     int start = line.find("<Event");
    //     int end = line.find("/Event>");
    //     // std::cout << line.substr(start, end - 1) << std::endl << std::endl;

    //     std::string rawData = line.substr(start, end - 1);

    // for (const auto line : payloads) {
    // std::string rawData = "Event xmlns='http://schemas.microsoft.com/win/2004/08/events/event'><System><Provider Name='Microsoft-Windows-Hyper-V-VmSwitch' Guid='{67dc0d66-3695-47c0-9642-33f76f7bd7ad}'/><EventID>234</EventID><Version>0</Version><Level>4</Level><Task>0</Task><Opcode>0</Opcode><Keywords>0x8000000000000080</Keywords><TimeCreated SystemTime='2025-10-21T14:43:13.2670995Z'/><EventRecordID>37728</EventRecordID><Correlation/><Execution ProcessID='4772' ThreadID='26040'/><Channel>System</Channel><Computer>DESKTOP-SV73V84</Computer><Security UserID='S-1-5-18'/></System><EventData><Data Name='NicNameLen'>74</Data><Data Name='NicName'>2915845D-987A-433E-8089-F92B39E12867--2260765B-C457-4E9B-A8E5-5461FB720DD8</Data><Data Name='PortNameLen'>1</Data><Data Name='PortName'></Data></EventData></Event>";

    std::vector<Entry> collection = findKeys(rawData);

        std::string json = buildString(collection);

        std::cout << json << std::endl;
        output << json << std::endl;
    // }
        

    
    return 0;
}