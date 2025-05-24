#include<iostream>
#include<vector>
#include<string>
#include<map>
#include<set>
#include<unordered_map>
#include<algorithm>
#include<fstream>
#include<regex>
using namespace std;
using namespace std::regex_constants;

string StripWhite(const string& src) // Ignore whitespace from the end
{
    string result;
    size_t i = 0;
    size_t j = src.size() - 1;
    while(i < src.size() and isspace(src[i])) { i++; }
    while(j > i and isspace(src[j])) { j--; }
    result = src.substr(i, j - i + 1);
    if(result.find("//") != string::npos)
    {
        size_t pos = result.find("//");
        result = result.substr(0, pos);
    }
    return result;
}

string StripComment(const string& src) // Ignore comment from the end
{
    string result = src;
    if(result.find("//") != string::npos)
    {
        size_t pos = result.find("//");
        result = result.substr(0, pos);
    }
    return result;
}


// Read the format file and parse it into a format object
// The format file should be in the following format:
// define segment names
// symbol: name ; comment(optional)
// state: type(begin/accept) name CategoryName(for accept state)
// transfer: nowState nextState RE
// Extras : For additional process: Differ Key and ID

class Format
{
    public:
        struct Category
        {
            string symbol;
            string comment;
        };
        struct State
        {
            string name;
            enum StateType
            {
                BEGIN,
                ACCEPT,
                TEMP,
            };
            StateType stateType;
            string symbol;
        };
        struct Transition
        {
            string nowState;
            string nextState;
            string TransforBy; // Accept regex
        };
    private:
        unordered_map<string, size_t> segment;
        void Expect(const string& A, const string& B, size_t line)
        {
            if(A != B)
            {
                string error = "Format: Expect " + B + ", but got " + A + " at line " + to_string(line);
                throw runtime_error(error);
            }
        }
        string BeginState;
    public:
        vector<string> rawLine;
        vector<Category> categoryList;
        vector<State> stateList;
        vector<Transition> transitionList;
        unordered_map<string, vector<pair<string, string>>> Extras;

        Format(ifstream& fin)
        {
            string line;
            while (getline(fin, line))
            {
                rawLine.push_back(line);
            }
            Init();
        }

        void Init()
        {
            // Assume First Line: define segment names
            string line = rawLine[0];
            stringstream ss(line);
            string name;
            ss >> name;
            Expect(name, "define", 0);
            unsigned int res = 0;
            size_t i = 0;
            while (ss >> name)
            {
                if(name == "symbol") res |= 1;
                if(name == "state") res |= 2;
                if(name == "transfer") res |= 4;
                segment[name] = i++;
            }
            if(res != 7)
            {
                string error = "Format: Expect symbol, state, transfer, but something misses. ";
                throw runtime_error(error);
            }
            for(size_t i = 1; i < rawLine.size(); i++)
            {
                auto&& line = rawLine[i];
                if(line.empty()) { continue; }
                if(line == "symbol")
                {
                    i++;
                    while(i < rawLine.size() and rawLine[i] != "end symbol")
                    {
                        if(rawLine[i].empty()) { i++; continue; }
                        if(i == rawLine.size() - 1)
                        {
                            string error = "Format: Expect end symbol, but got EOF at line " + to_string(i);
                            throw runtime_error(error);
                        }
                        stringstream ss(rawLine[i]);
                        string name;
                        ss >> name;
                        string comment;
                        getline(ss, comment);
                        categoryList.emplace_back(Category{name, comment});
                        i++;
                    }
                }
                else if(line == "state")
                {
                    i++;
                    while(i < rawLine.size() and rawLine[i] != "end state")
                    {
                        if(rawLine[i].empty()) { i++; continue; }
                        if(i == rawLine.size() - 1)
                        {
                            string error = "Format: Expect end state, but got EOF at line " + to_string(i);
                            throw runtime_error(error);
                        }
                        stringstream ss(rawLine[i]);
                        string name;
                        ss >> name;
                        if(name == "begin")
                        {
                            ss >> name;
                            BeginState = name;
                            stateList.emplace_back(name, State::BEGIN);
                        }
                        else if(name == "accept")
                        {
                            ss >> name;
                            string type;
                            ss >> type;
                            stateList.emplace_back(name, State::ACCEPT, type);
                        }
                        else
                        {
                            stateList.emplace_back(name, State::TEMP);
                        }
                        i++;
                    }
                }
                else if(line == "transfer")
                {
                    i++;
                    while(i < rawLine.size() and rawLine[i] != "end transfer")
                    {
                        if(rawLine[i].empty()) { i++; continue; }
                        if(i == rawLine.size() - 1)
                        {
                            string error = "Format: Expect end transfer, but got EOF at line " + to_string(i);
                            throw runtime_error(error);
                        }
                        stringstream ss(rawLine[i]);
                        string nowState;
                        ss >> nowState;
                        string nextState;
                        ss >> nextState;
                        string regexStr;
                        getline(ss, regexStr);
                        regexStr = StripWhite(regexStr);
                        transitionList.emplace_back(Transition{nowState, nextState, regexStr});
                        i++;
                    }
                }
                else
                {
                    if(line == "") { continue; }
                    stringstream ss(line);
                    string name;
                    ss >> name;
                    if(segment.find(name) == segment.end())
                    {
                        string error = "Format: Undefined segment name " + name + " at line " + to_string(i);
                        throw runtime_error(error);
                    }
                    // Assert no error
                    i++;
                    vector<pair<string, string>> temp;
                    while(i < rawLine.size() and rawLine[i] != "end " + name)
                    {
                        if(rawLine[i].empty()) { i++; continue; }
                        if(i == rawLine.size() - 1)
                        {
                            string error = "Format: Expect end " + name + ", but got EOF at line " + to_string(i);
                            throw runtime_error(error);
                        }
                        stringstream ss(rawLine[i]);
                        string symbol;
                        ss >> symbol;
                        string regexStr;
                        getline(ss, regexStr);
                        regexStr = StripWhite(regexStr);
                        temp.emplace_back(symbol, regexStr);
                        i++;
                    }
                    Extras.insert({name, temp});
                }
            }
        }
        string GetBeginState() const { return BeginState; }
        void Print()
        {
            cout << "Category List: " << endl;
            for(auto&& category : categoryList)
            {
                cout << category.symbol << endl;
            }
            cout << "State List: " << endl;
            for(auto&& state : stateList)
            {
                cout << state.name << " " << state.stateType << endl;
            }
            cout << "Transition List: " << endl;
            for(auto&& transition : transitionList)
            {
                cout << transition.nowState << " -> " << transition.nextState << " with \"" << transition.TransforBy << "\"" << endl;
            }
            cout << "Extras: " << endl;
            for(auto&& extra : Extras)
            {
                cout << extra.first << ": " << endl;
                for(auto&& pair : extra.second)
                {
                    cout << pair.first << " \"" << pair.second << "\"" << endl;
                }
            }
        }
};

bool checkUnclosedStrLit(const string& str)
{
    auto it = str.begin();
    bool quoteCount = 0;
    while(it != str.end())
    {
        if(*it == '\\')
        {
            it++;
            if(it == str.end()) { return quoteCount; }
            it++;
            continue;
        }
        if(*it == '"')
        {
            quoteCount = !quoteCount;
        }
        it++;
    }
    return !quoteCount;
}

/*
1, Read the source file and parse it into a string
2, Remove the comment and whitespace from the end of each line
3, Join the lines into a single string, spearated by space
*/
string ProcessText(ifstream& fin)
{
    int cnt = 0;
    string line;
    string result;
    while(getline(fin, line))
    {
        string str;
        LOGIC_CONNECT:
        cnt++;
        str += StripWhite(line);
        if(str.empty()) { continue; }  
        if(str.ends_with('\\'))
        {
            str = str.substr(0, str.size() - 1);
            getline(fin, line);
            goto LOGIC_CONNECT;
        }
        if(!checkUnclosedStrLit(str))
        {
            string error = "Preprocessor: Unclosed string literal at line " + to_string(cnt) + "\n\tDetail: " + str;
            cerr << error << endl;
            exit(1);
        }
        str = StripComment(str);
        result += str;
        result += " ";
    }
    cout<<result<<endl;
    return result;
}

class Scanner
{
    private:
        string src;
        const Format& fmt;
        vector<pair<string, string>> tokens; // Result
        string currentState; // State of DFA
        string currentToken; // MaxPrefix so far
        size_t currentIndex; // Current index of the source string, assume its prefix has been scanned
        bool isEnd;
    public:
        Scanner(const string& src, const Format& fmt): src(src), fmt(fmt)
        {
            currentState = fmt.GetBeginState();
            currentToken = "";
            currentIndex = 0;
            isEnd = false;
        }
        bool Scan() // One char at a time
        {
            if(currentIndex >= src.size())
            {
                isEnd = true;
                return false;
            }
            char currentChar = src[currentIndex];
            // Try to tranfer
            // if(!isspace(currentChar))
            for(auto&& transition : fmt.transitionList)
            {
                if(transition.nowState == currentState)
                {
                    regex re(transition.TransforBy);
                    string str(1, currentChar);
                    if(regex_match(str, re))
                    {
                        currentState = transition.nextState;
                        currentToken += currentChar;
                        currentIndex++;
                        return true;
                    }
                }
            }
            // DFA denied, check if current token is valid
            for(auto&& state : fmt.stateList)
            {
                if(state.name == currentState and state.stateType == Format::State::ACCEPT)
                {
                    // Check which category it belongs to
                    for(auto&& category : fmt.categoryList)
                    {
                        if(state.symbol == category.symbol)
                        {
                            tokens.emplace_back(currentToken, state.symbol);
                            currentToken = "";
                            currentState = fmt.GetBeginState();
                            return true;
                        }
                    }
                    // Additional process, differ key and id
                    try
                    {  
                        auto&& extras = fmt.Extras.at(state.symbol);

                        for(auto&& extra : extras)
                        {
                            if(extra.second == "")
                            {
                                tokens.emplace_back(currentToken, extra.first);
                                currentToken = "";
                                currentState = fmt.GetBeginState();
                                return true;
                            }
                            regex re(extra.second);
                            if(regex_match(currentToken, re))
                            {
                                tokens.emplace_back(currentToken, extra.first);
                                currentToken = "";
                                currentState = fmt.GetBeginState();
                                return true;
                            }
                        }
                    }
                    catch(...) // Potentially out of range, ignore
                    {

                    }
                }
            }
            // DFA failed, state not accepted -> syntax error
            if(!isspace(currentChar))
            {
                string Error = "Scanner: No match found for " + currentToken + " at index " + to_string(currentIndex) + ", try to skip this.";
                cerr << Error << endl;
            }
            currentToken = "";
            currentState = fmt.GetBeginState();
            currentIndex++;
            return false;
        }

        void reset()
        {
            currentIndex = 0;
            currentState = fmt.GetBeginState();
            currentToken = "";
            isEnd = false;
            tokens.clear();
        }

        auto getTokens() const
        {
            return tokens;
        }

        void scanall()
        {
            while(!isEnd)
            {
                Scan();
            }
        }
};

int main(int argc, char *argv[])
{
    // Read the format from Format.txt
    ifstream fin("Format.txt");
    if(!fin)
    {
        cerr << "Error: Cannot open file Format.txt" << endl;
        return 1;
    }
    Format format(fin);
    // format.Print();
    fin.close();

    // Read the source from Source.txt
    ifstream fin2("Source.txt");
    if(!fin2)
    {
        cerr << "Error: Cannot open file Source.txt" << endl;
        return 1;
    }
    string src = ProcessText(fin2);
    fin2.close();
    Scanner scanner(src, format);
    scanner.scanall();
    ofstream fout("Tokens.txt");
    auto tokens = scanner.getTokens();
    for(auto&& token : tokens)
    {
        fout << token.second << " " << token.first << endl;
    }
    return 0;
}