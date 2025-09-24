#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <windows.h>
#include <vector>
#include <cstdlib>

#define IDENTIFYBYTES "CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC"

// 使用常量代替魔术数字，提高代码可读性
const std::string SCRIPT_TAG_START = "<AssemblerScript>";
const std::string SCRIPT_TAG_END = "</AssemblerScript>";
const std::string ALLOC_FUNC = "alloc(";
const std::string DEALLOC_FUNC = "dealloc(";
const std::string FAKE_DISABLE_TAG = "'[DISABLE]'";
const std::string DISABLE_TAG = "[DISABLE]";

// 内存对齐常量
const int MEMORY_ALIGNMENT = 16;

std::string StartAddress = "794000";

std::string DecIntToHexString(int theDec) {
    int a;
    std::string b;
    std::string result;
    for (int i = 0; i < 8; i++) {
        a = theDec % 16;
        if (a < 10) {
            b = a + '0';
        }
        else {
            b = a - 10 + 'A';
        }
        result.insert(0, b);
        theDec -= a;
        if (theDec == 0) {
            break;
        }
        theDec = theDec / 16;
    }
    return result;
}

int RoundUpTo1024_bit(int n) {
    return (n + 1023) & ~1023;
}

std::string ReadFromClipboard() {
    std::string clipboardText;

    if (!OpenClipboard(NULL)) {
        return clipboardText;
    }

    HGLOBAL clipbuffer = GetClipboardData(CF_TEXT);
    if (clipbuffer == NULL) {
        CloseClipboard();
        return clipboardText;
    }

    char* buffer = (char*)GlobalLock(clipbuffer);
    size_t length = strlen(buffer);

    clipboardText.assign(buffer, length);

    GlobalUnlock(clipbuffer);

    CloseClipboard();

    return clipboardText;
}

std::wstring NarrowToWide(const std::string& narrowStr) {
    int wcharCount = MultiByteToWideChar(CP_ACP, 0, narrowStr.c_str(), -1, NULL, 0);
    std::wstring wideStr(wcharCount, 0);
    MultiByteToWideChar(CP_ACP, 0, narrowStr.c_str(), -1, &wideStr[0], wcharCount);
    return wideStr;
}

bool SetClipboardText(const std::string& text) {
    std::wstring wideText = NarrowToWide(text);

    if (!OpenClipboard(NULL)) {
        return false;
    }
    EmptyClipboard();
    size_t bufferSize = (wideText.length() + 1) * sizeof(wchar_t);
    HGLOBAL clipBuffer = GlobalAlloc(GMEM_DDESHARE, bufferSize);
    if (!clipBuffer) {
        CloseClipboard();
        return false;
    }
    wchar_t* buffer = (wchar_t*)GlobalLock(clipBuffer);
    if (!buffer) {
        GlobalFree(clipBuffer);
        CloseClipboard();
        return false;
    }
    wcscpy_s(buffer, bufferSize / sizeof(wchar_t), wideText.c_str());
    GlobalUnlock(clipBuffer);
    SetClipboardData(CF_UNICODETEXT, clipBuffer);
    CloseClipboard();
    return true;
}

std::string NewCheatEntry(const int theID, const std::string& name, const std::string& context) {
    std::string head =  "<CheatEntry>\n"
                        "<ID>" + std::to_string(theID) + "</ID>\n"
                        "<Description>\"" + name + "\"</Description>\n"
                        "<LastState/>\n"
                        "<VariableType>Auto Assembler Script</VariableType>\n"
                        "<AssemblerScript>\n"
                        "[ENABLE]\n";
    std::string tail =  "[DISABLE]\n"
                        "</AssemblerScript>\n"
                        "</CheatEntry>";
	return head + context + tail;
}

std::string ProcessScript(const std::string& input) {
    std::string result = input;
    std::string ScriptName;
    std::string replacement;
	std::string AddtionalScripts;
    int n = 0;
    int TotalPageLength = 0;
    size_t startPos = 0;
    size_t endPos = 0;
    int OriginalScriptSize = 0;
    int previousPageLength = 0;

    startPos = result.find(SCRIPT_TAG_START);
    while (startPos != std::string::npos) {
        endPos = result.find(SCRIPT_TAG_END, startPos);
        if (endPos == std::string::npos) {
            break;
        }

        std::string scriptContent = result.substr(startPos + SCRIPT_TAG_START.length(), endPos - startPos - SCRIPT_TAG_START.length());
        bool hasalloc = false;
        size_t allocPos = scriptContent.find(ALLOC_FUNC);
        

        if (allocPos != std::string::npos) {
            size_t commaPos = scriptContent.find(',', allocPos);
            size_t allocendPos = scriptContent.find(')', allocPos);
            ScriptName = scriptContent.substr(allocPos + ALLOC_FUNC.length(), commaPos - allocPos - ALLOC_FUNC.length());
            OriginalScriptSize = std::stoi(scriptContent.substr(commaPos + 1, allocendPos - commaPos));

            size_t deallocPos;
            if ((deallocPos = scriptContent.find(DEALLOC_FUNC + ScriptName + ")")) != std::string::npos) {
                scriptContent.erase(deallocPos, ScriptName.length() + DEALLOC_FUNC.length() + 1);
            }
            replacement = "define(" + ScriptName + ",Page_" + std::to_string(n) + ")";
            scriptContent.replace(allocPos, allocendPos - allocPos + 1, replacement);
            hasalloc = true;
            //allocPos = scriptContent.find(ALLOC_FUNC, allocendPos);
        }

        int ScriptLength = endPos - startPos - SCRIPT_TAG_START.length();
        result.replace(startPos + SCRIPT_TAG_START.length(), ScriptLength, scriptContent);

        if (hasalloc) {
            size_t disablePos;
            size_t fakedisablePos = scriptContent.find(FAKE_DISABLE_TAG);
            if (fakedisablePos == std::string::npos) {
                disablePos = scriptContent.find(DISABLE_TAG);
            }
            else {
                disablePos = scriptContent.find(DISABLE_TAG, fakedisablePos + FAKE_DISABLE_TAG.length());
            }
            if (disablePos != std::string::npos) {
                size_t ScriptNameStartPos = scriptContent.find(ScriptName + ":");
                if (ScriptNameStartPos != std::string::npos) {
                    ScriptNameStartPos += ScriptName.length() + 1;
                    int PageLength = ((int(disablePos - ScriptNameStartPos) < OriginalScriptSize) ? int(disablePos - ScriptNameStartPos) : OriginalScriptSize);
                    int alignedPageLength = ((PageLength % MEMORY_ALIGNMENT) == 0) ? PageLength : PageLength + (MEMORY_ALIGNMENT - (PageLength % MEMORY_ALIGNMENT));

                    if (n == 0) {
                        /*PageList += "define(Page_" + std::to_string(n) + "," + StartAddress + ")\n"
                                  + "registersymbol(Page_" + std::to_string(n) + ")\n";*/
                        AddtionalScripts += NewCheatEntry(  100000 + n, "内存页定义脚本_" + std::to_string(n), 
                                                            "define(Page_" + std::to_string(n) + "," + StartAddress + ")\n" + 
                                                            "registersymbol(Page_" + std::to_string(n) + ")\n" +
                                                            "Page_" + std::to_string(n) + ":\n" +
                                                            "align " + std::to_string(RoundUpTo1024_bit(TotalPageLength)) + ",CC\n");
                    }
                    else {
                        std::string hexLength = DecIntToHexString(previousPageLength);
                        //PageList += "define(Page_" + std::to_string(n) + ",Page_" + std::to_string(n - 1) + "+" + hexLength + ")\nregistersymbol(Page_" + std::to_string(n) + ")\n";
                       /* PageList += "aobscanregion(Page_" + std::to_string(n) +
                                                ", Page_" + std::to_string(n - 1) +
                                                ", Page_" + std::to_string(n - 1) + "+" + hexLength +
                                                ", " + IDENTIFYBYTES + ")\nregistersymbol(Page_" + std::to_string(n) + ")\n";*/
                        //aobscanregion(thisPage, LastPage, LastPage+PredictSize, CC x 20)
                        AddtionalScripts += NewCheatEntry(  100000 + n, "内存页定义脚本_" + std::to_string(n), 
                                                            "aobscanregion(Page_" + std::to_string(n) + 
                                                            ", Page_" + std::to_string(n - 1) + 
                                                            ", Page_" + std::to_string(n - 1) + "+" + hexLength + 
						                                	", " + IDENTIFYBYTES + ")\nregistersymbol(Page_" + std::to_string(n) + ")\n");
                    }

                    previousPageLength = alignedPageLength;
                    TotalPageLength += alignedPageLength;
                    n++;
                }
            }
        }

        startPos = result.find(SCRIPT_TAG_START, endPos);
    }

    // 在PageList的开头插入总内存大小注释
    //PageList += "Page_0:\nalign " + std::to_string(RoundUpTo1024_bit(TotalPageLength)) + ",CC\n";
    //PageList.insert(0, "//总内存大小应大于：" + std::to_string(TotalPageLength) + "字节\n");
    // 查找 </AssemblerScript> 的结束位置
    std::size_t scriptEndPos = result.rfind("</AssemblerScript>");

    // 如果找到了 </AssemblerScript>
    if (scriptEndPos != std::string::npos)
    {
        // 计算插入的起始位置，即 </AssemblerScript> 之后
        std::size_t insertStartPos = scriptEndPos + std::string("</AssemblerScript>").length();

        // 找到 </CheatEntry> 的位置，从 scriptEndPos 之后开始查找
        std::string EntryEnd = "</CheatEntry>";
        std::size_t cheatEntryEndPos = result.find(EntryEnd, insertStartPos);

        // 如果找到了 </CheatEntry>
        if (cheatEntryEndPos != std::string::npos)
        {
            // 插入 AddtionalScripts
			cheatEntryEndPos += EntryEnd.length();
            result.insert(cheatEntryEndPos, AddtionalScripts);
        }
    }
    return result;
}

int main()
{
    // 欢迎并提示用户操作
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "       CT脚本内存地址自动处理工具         " << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    std::cout << "请将待处理的CT脚本（包含<AssemblerScript>标签）" << std::endl;
    std::cout << "复制到剪贴板，然后按任意键继续..." << std::endl;
    std::cout << "注意：如果剪贴板内不是CT脚本，程序将无法正常工作。" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    system("pause");

    // 从剪贴板读取数据
    std::string input = ReadFromClipboard();

    // 检查剪贴板内容是否有效
    if (input.empty()) {
        std::cerr << "错误：剪贴板中没有文本数据，请确保已复制脚本。" << std::endl;
        system("pause");
        return 1;
    }
    if (input.find(SCRIPT_TAG_START) == std::string::npos) {
        std::cerr << "错误：剪贴板内容不是有效的CT脚本，未找到" << SCRIPT_TAG_START << "标签。" << std::endl;
        system("pause");
        return 1;
    }

    std::cout << "正在处理脚本，请稍候..." << std::endl;

    std::string output = ProcessScript(input);

    // 将处理后的脚本写回剪贴板
    bool Iscompelete = SetClipboardText(output);

    // 打印结果并提供清晰的反馈
    std::cout << "------------------------------------------" << std::endl;
    if (Iscompelete) {
        std::cout << "处理完成！" << std::endl;
        std::cout << "修改后的完整脚本已复制到剪贴板，您可以直接粘贴使用。" << std::endl;
    }
    else {
        std::cerr << "处理完成，但无法将结果复制到剪贴板。" << std::endl;
    }
    /*std::cout << std::endl;
    std::cout << PageList << std::endl;
    std::cout << "以上是生成的地址定义，按任意键复制到剪贴板：" << std::endl;
    std::cout << "------------------------------------------" << std::endl;
    system("pause");
    Iscompelete = SetClipboardText(PageList);*/
    if (Iscompelete) {
        std::cout << "处理完成！" << std::endl;
    }
    else {
        std::cerr << "处理完成，但无法将结果复制到剪贴板。" << std::endl;
    }
    return 0;
}