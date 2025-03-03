#pragma once
#include <stdio.h>
#include <iostream>
#include <map>
#include <unordered_map>
#include <vector>
#include <string>
#include <queue>
#include <memory>
#include <sentencepiece/sentencepiece_processor.h>

struct FileBuffer {
    FILE *f;

    FileBuffer (const std::string &fileName) {
        f = fopen(fileName.c_str(), "rb");
    }

    int ReadInt() {
        int v;
        // fread(buf,1,sizeof(buf),fp)：表示每个数据的大小为1，读了4次，一共4b，返回值为实际读取的数据个数即4
        if (fread(&v, 1, 4, f) != 4) { 
            std::cout << "FileBuffer.ReadInt error." << "\n";
        };
        return v;
    }

    float ReadFloat() {
        float v;
        if (fread(&v, 1, 4, f) != 4) {
            std::cout << "FileBuffer.ReadFloat error." << "\n";
        };
        return v;
    }

    std::string ReadString() {
        int len = ReadInt();
        std::string ret = "";
        char *v = new char[len + 5];
        v[len] = 0;
        if (fread(v, 1, len, f) != len) {
            std::cout << "FileBuffer.ReadString error." << "\n";
        }
        return v;
    }

    void ReadBytes(uint8_t *buffer, uint64_t bytes) {
        if (fread(buffer, 1, bytes, f) != bytes) {
            std::cout << "FileBuffer.ReadBytes error." << "\n";
        }
    }

    ~FileBuffer() {
        fclose(f);
    }
};

struct Tokenizer {
    std::unique_ptr<sentencepiece::SentencePieceProcessor> spProcessor;
    std::unordered_map<int, std::string> tokenToStringDict;
    std::unordered_map<std::string, int> stringToTokenDict;
    
    // 空格的特殊表示
    std::string blank;

    Tokenizer() {
        spProcessor = std::make_unique<sentencepiece::SentencePieceProcessor>();
        // 初始化blank字符串 (226,150,129)
        blank = "";
        blank += 226;
        blank += 150;
        blank += 129;
    }

    ~Tokenizer() {
        // SentencePiece会自行处理内存释放
        tokenToStringDict.clear();
        stringToTokenDict.clear();
    }

    void Initialize(std::string file) {
        // 先尝试直接加载SentencePiece模型
        auto status = spProcessor->Load(file);
        
        // 如果直接加载失败，则使用原始的文件格式进行加载
        if (!status.ok()) {
            std::cout << "尝试以旧格式加载词汇表" << std::endl;
            FileBuffer buffer(file);
            int versionId = buffer.ReadInt();

            if (versionId >= 1) {
                // 处理key-value表
                int keyValueLen = buffer.ReadInt();
                for (int i = 0; i < keyValueLen; i++) {
                    std::string key = buffer.ReadString();
                    std::string value = buffer.ReadString();
                }
            }

            // 加载词汇表
            int vocabLen = buffer.ReadInt();
            for (int i = 0; i < vocabLen; i++) {
                int len = buffer.ReadInt();
                std::string x = "";
                for (int j = 0; j < len; j++) {
                    x += buffer.ReadInt();
                }
                int id = buffer.ReadInt();
                float score = buffer.ReadFloat();
                
                // 存储token和id的映射关系
                tokenToStringDict[id] = x;
                stringToTokenDict[x] = id;
            }
        } else {
            // SentencePiece模型加载成功，构建映射字典
            for (int i = 0; i < spProcessor->GetPieceSize(); i++) {
                auto piece = spProcessor->IdToPiece(i);
                tokenToStringDict[i] = piece;
                stringToTokenDict[piece] = i;
            }
        }
    }

    std::vector<int> Encode(const std::string &ori) {
        // 如果SentencePiece处理器可用，则使用它
        if (spProcessor->IsReady()) {
            std::vector<int> ids;
            
            // 处理特殊token
            if (15 < ori.size() && ori.substr(0, 15) == "<FLM_FIX_TOKEN_") {
                int pos = 15;
                int now = 0;
                while (pos < ori.size() && ori[pos] >= '0' && ori[pos] <= '9') {
                    now = now * 10 + ori[pos] - '0';
                    pos++;
                }
                ids.push_back(now);
                return ids;
            }
            
            // 使用SentencePiece编码
            spProcessor->Encode(ori, &ids);
            return ids;
        }
        
        // 回退到原始实现的空格处理逻辑
        std::string s = blank;
        if (15 < ori.size() && ori.substr(0, 15) == "<FLM_FIX_TOKEN_") {
            s = "";
        }
        
        for (int i = 0; i < ori.size(); i++) {
            if (ori[i] == ' ') {
                if (i != 0 && ori[i - 1] != ' ') {
                    s += blank;
                }
            } else {
                s += ori[i];
            }
        }
        
        // 直接查表将字符转为token
        std::vector<int> result;
        // 处理特殊token
        if (s.find("<FLM_FIX_TOKEN_") == 0) {
            int pos = 15;
            int now = 0;
            while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
                now = now * 10 + s[pos] - '0';
                pos++;
            }
            result.push_back(now);
            return result;
        }
        
        // 简单的字符级分词，按字符查找token表
        for (size_t i = 0; i < s.size(); i++) {
            std::string charStr(1, s[i]);
            if (stringToTokenDict.find(charStr) != stringToTokenDict.end()) {
                result.push_back(stringToTokenDict[charStr]);
            } else {
                // 处理未识别的字符
                uint8_t c = (uint8_t)(s[i]);
                std::string now = "<0x00>";
                now[3] = (c / 16 > 9 ? ('A' + c / 16 - 10) : ('0' + c / 16));
                now[4] = (c % 16 > 9 ? ('A' + c % 16 - 10) : ('0' + c % 16));
                if (stringToTokenDict.find(now) != stringToTokenDict.end()) {
                    result.push_back(stringToTokenDict[now]);
                }
            }
        }
        
        return result;
    }

    std::string Decode(std::vector<int> ret) {
        std::vector<int> tokens;
        for (int i = 0; i < ret.size(); i++) {
            tokens.push_back((int)ret.data()[i]);
        }
        return DecodeTokens(tokens);
    }

    std::string DecodeTokens(const std::vector<int> &tokens) {
        // 如果SentencePiece处理器可用，则使用它进行基础解码
        std::string ret;
        if (spProcessor->IsReady()) {
            spProcessor->Decode(tokens, &ret);
        } else {
            // 回退到查表解码
            for (int i = 0; i < tokens.size(); i++) {
                if (tokenToStringDict.find(tokens[i]) != tokenToStringDict.end()) {
                    ret += tokenToStringDict[tokens[i]];
                }
            }
        }
        
        // 处理特殊标记，与原实现保持一致
        std::string result = "";
        size_t pos = 0;
        
        while (pos < ret.size()) {
            // 处理特殊标记
            if (pos + 6 <= ret.size() && ret.substr(pos, 3) == "<0x" && ret[pos + 5] == '>') {
                int c = 0;
                for (int i = pos + 3; i < pos + 5; i++) {
                    c *= 16;
                    if (ret[i] >= '0' && ret[i] <= '9') {
                        c += (ret[i] - '0');
                    } else {
                        c += (ret[i] - 'A' + 10);
                    }
                }
                result += (char)c;
                pos += 6;
            } else if (pos + 3 <= ret.size() && ret.substr(pos, 3) == "<n>") {
                result += "\n";
                pos += 3;
            } else if (pos + 7 <= ret.size() && ret.substr(pos, 7) == "<|tab|>") {
                result += "\t";
                pos += 7;
            } else {
                result += ret[pos];
                pos++;
            }
        }
        
        // 替换特殊的空格表示
        while (true) {
            std::string::size_type blankPos(0);
            if ((blankPos = result.find(blank)) != std::string::npos)
                result.replace(blankPos, blank.length(), " ");
            else break;
        }
        
        // 处理<|blank_n|>
        int blankPos = result.find("<|blank_");
        if (blankPos != -1) {
            int space_num = atoi(result.substr(blankPos + 8, result.size() - blankPos - 10).c_str());
            return std::string(space_num, ' ');
        }
        
        return result;
    }
};