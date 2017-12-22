#include <unordered_set>
#include <vector>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stack>
using namespace std;

#include "defines.h"
#include "sourceGen.h"
#include "headerGen.h"
#include "string.h"
#include "file.h"
#include "line.h"
#include "extensionHandler.h"
#include "dependenciesMapper.h"
#include "firstReplaces.h"

FILE * input;
FILE * output;

int i, j;

stack<int> offSets;
stack<bool> closeWithLineEnding;
bool multiLineComment;
char buff[LINESZ];
char buff2[LINESZ];
int buffLen;

char buffPrevious[LINESZ];
int buffPreviousLen;

bool structTypedef;
char structName[LINESZ];
vector<unordered_set<string> > seenWords;

void addPahrenthesis(char * s){	
	int offset = 0;
	for(offset = 0; stringContainsChar(" \n\t", s[offset]); offset++)
		if(s[offset+1] == '/' && s[offset+2] == '/')
			while(s[offset] != '\n')
				offset++;
	
	if(string_isWord(s, "struct") >= 0)
		return;
	
	int len = strlen(s);
	
	if(s[len-2] == '{'){
		int lookStart = max(max(max(
			string_isWord(s, "if"),
			string_isWord(s, "for")),
			string_isWord(s, "while")),
			string_isWord(s, "switch"));
		
		if(lookStart == -1){
			if(string_isWord(s, "do") == -1 &&
			   string_isWord(s, "else") == -1 &&
			   string_isWord(s, "class") == -1 &&
			   string_isWord(s, "struct") == -1)
				implyFunctionParametersType(s);
		}
		else{
			int i = lookStart;
			while(stringContainsChar(" \t\n", s[i]))
				i++;
			
			bool imply = true;
			
			while(!stringContainsChar(" \t\n(", s[i]) && s[i] != '\0') i++;
			while(stringContainsChar(" \t\n", s[i])) i++;
			
			int parenthesisCount = 0;
			for(; s[i] != '\0'; i++){
				if(s[i] == '(')
					parenthesisCount++;
				else if(s[i] == ')')
					parenthesisCount--;
				
				if(parenthesisCount == 0){
					if(s[i] == ')')
						i++;
					while(stringContainsChar(" \t\n{;", s[i]))
						i++;
					imply = !(s[i] == '\0');
					break;
				}
			}
			
			if(imply){
				for(i = lookStart; s[i] != '\0'; i++){
					if(s[i] == ' ' || s[i] == '\t'){
						s[i] = '(';
						break;
					}
				}
				
				s[len-2] = ')';
				s[len-1] = '{';
				s[len] = '\n';
				s[len+1] = '\0';
			}
		}
	}
}

void implyFunctionParametersType(char * s){	
	char lastType[LINESZ] = "\0";
	
	int typeStartIndex = 0;
	
	int i = 0;
	while(s[i] == ' ' || s[i] == '\t') i++;
	
	int wordCount = 0;
	while(s[i] != '(' && s[i] != '\0'){
		if(s[i] == ' ' || s[i] == '\t'){
			wordCount++;
			while(s[i] == ' ' || s[i] == '\t') i++;
			i--;
		}
		i++;
	}
	
	if(wordCount == 0){
		if(string_isWord(s, "main") != -1)
			stringInsert(s, "int ", 0);
		else
			stringInsert(s, "auto ", 0);
	}	
	
	if(s[i] == '(') i++;
	
	while(s[i] == ' ' || s[i] == '\t') i++;
	
	int wordStartIndex = i;
	int wordEndIndex = i;
	int lastWordEndIndex = i;
	
	wordCount = 0;
	while(s[i] != '\0' && s[i] != '\n'){
		wordCount++;
		while(s[i] != ' ' && s[i] != '\t' && s[i] != '\0' && s[i] != '\n' &&  s[i] != ',' &&  s[i] != '(' &&  s[i] != '=') i++;
		
		lastWordEndIndex = wordEndIndex;
		wordEndIndex = i;
		
		while(s[i] == ' ' || s[i] == '\t') i++;
		
		if(s[i] == '=') while(!(s[i] == ',' || s[i] == '\n' || s[i] == '\0')) i++;
			
		
		if(s[i] == '(' || s[i] == ',' || s[i] == '\n' || s[i] == '\0'){
			if(wordCount == 1){
				strInsert(s, lastType, wordStartIndex);
				while(!(s[i] == '(' || s[i] == ',' || s[i] == '\n' || s[i] == '\0')) i++;
			} else if (wordCount > 1) {
				int j;
				for(j = wordStartIndex; j < lastWordEndIndex; j++){
					lastType[j-wordStartIndex] = s[j];
				}
				lastType[j-wordStartIndex] = ' ';
				lastType[j-wordStartIndex+1] = '\0';
			}
			wordCount = 0;
			i++;
			while(s[i] == ' ' || s[i] == '\t') i++;
			wordStartIndex = i;
		}
	}
}

bool wordSeen(string word){
	for(unordered_set<string> set : seenWords)
		if(set.count(word))
			return true;
	return false;
}

void implyVariablesType(char * line){
	vector<string> firstSplit = smartSplitWords(line, "", " \t\r\n()[]{},.;!@#%^&*-+/:\'\\", true);
	vector<vector<string> > wordGroups;
	wordGroups.push_back(vector<string>());
	for(int i = 1; i < firstSplit.size(); i += 2){
		if(!isWhitespace(firstSplit[i-1], " <>[]-"))
			wordGroups.push_back(vector<string>());
		wordGroups.back().push_back(firstSplit[i]);
	}
	
	line[0] = '\0';
	
	int whiteSpaceID = 0;
	for(vector<string> words : wordGroups){
		
		if(words.size() >= 3 && words[1] == "=" && !wordSeen(words[0]))
			words[0] = "auto " + words[0];
		
		for(string word : words){
			strcat(line, firstSplit[whiteSpaceID].c_str());
			strcat(line, word.c_str());
			seenWords.back().insert(word);
			whiteSpaceID += 2;
		}
	}
	if(whiteSpaceID < firstSplit.size())
		strcat(line, firstSplit[whiteSpaceID].c_str());
}

int closeKeys(int offset, bool beauty){
	int closeAmount = 0;
	while(offset < offSets.top()){
		closeAmount++;
		bool closeLineEnding = closeWithLineEnding.top();
		closeWithLineEnding.pop();
		offSets.pop();				
		int retOffset = offSets.top();
		
		buffPreviousLen = strlen(buffPrevious);
		if(buffPrevious[buffPreviousLen-1] == '\n'){
			if(beauty){
				for(i = 0; i < retOffset/tabSpaces; i++)
					buffPrevious[buffPreviousLen++] = '\t';
				for(j = 0; j < retOffset%tabSpaces; j++)
					buffPrevious[buffPreviousLen++] = ' ';
				buffPrevious[buffPreviousLen++] = '}';
				buffPrevious[buffPreviousLen++] = '\n';
				buffPrevious[buffPreviousLen] = '\0';
			}
			else {
				buffPrevious[buffPreviousLen-1] = '}';
				buffPrevious[buffPreviousLen++] = '\n';
				buffPrevious[buffPreviousLen] = '\0';
			}
		}
		else {
			if(beauty){
				buffPrevious[buffPreviousLen++] = '\n';
				for(i = 0; i < retOffset/tabSpaces; i++)
					buffPrevious[buffPreviousLen++] = '\t';
				for(j = 0; j < retOffset%tabSpaces; j++)
					buffPrevious[buffPreviousLen++] = ' ';
				buffPrevious[buffPreviousLen++] = '}';
				buffPrevious[buffPreviousLen++] = '\n';
				buffPrevious[buffPreviousLen] = '\0';
			} else {				
				buffPrevious[buffPreviousLen++] = '}';
				buffPrevious[buffPreviousLen] = '\0';
			}
		}
		
		if(closeLineEnding){
			if(structTypedef){
				structTypedef = false;
				i = 0;
				buffPrevious[buffPreviousLen-1] = ' ';
				while(structName[i] != '\0')
					buffPrevious[buffPreviousLen++] = structName[i++];
				buffPreviousLen++;
			}
			buffPrevious[buffPreviousLen-1] = ';';
			buffPrevious[buffPreviousLen++] = '\n';
			buffPrevious[buffPreviousLen] = '\0';
		}
	}
	return closeAmount;
}

int placeLineEnding(char * line){
	int len = strlen(line);
	
	int last = len-1;
	while(last >= 0 && stringContainsChar(" \t\n", line[last])) last--;
	
	if(last != -1 && line[last] != ';'){
		line[++last] = ';';
		line[++last] = '\n';
		line[++last] = '\0';
	}
	return last;
}

void lineImplications(char * line){
	addPahrenthesis(line);
	implyVariablesType(line);
}

void generateSource(char * inputFile, char * outputFile, bool beauty){
	input = fopen(inputFile, "r");
	output = fopen(outputFile, "w+");
	
	if(input == NULL){
		printf("File not found\n");
		printf("USAGE: cmp SourceCode Flags");
		exit(0);
	}
	offSets.push(0);
	closeWithLineEnding.push(false);
	
	seenWords.clear();
	seenWords.push_back(unordered_set<string>());
	
	multiLineComment = false;
	
	structTypedef = false;
	
	strcpy(buffPrevious, "");
	
	char emptyLinesBuffPrevious[LINESZ] = "";
	char emptyLinesBuff[LINESZ] = "";
	
	while (fgets (buff, LINESZ, input)) {
		//Skip empty lines
		emptyLinesBuff[0] = '\0';
		bool empty = isEmptyLine(buff);
		if(empty)
			strcpy(emptyLinesBuff, buff);
		
		while(empty){
			if(!fgets (buff2, LINESZ, input))
				break;
			
			empty = isEmptyLine(buff2);
			if(empty)
				strcat(emptyLinesBuff, buff2);
			else
				strcpy(buff, buff2);
		}
		
		//Read tab spaces
		int buffLen = strlen(buff);
		int offset = 0;
		for(i = 0; i < buffLen; i++){
			if(buff[i] == '\t')
				offset += tabSpaces;
			else if(buff[i] == ' ')
				offset++;
			else if(buff[i] == '\n')
				offset = 0;
			else if(buff[i] == '/' && buff[i+1] == '/'){
				while(buff[i] != '\n' && buff[i] != '\0')
					i++;
				offset = 0;
			}
			else
				break;
		}
		bool oneLineComment = false;
		bool cPreCompilerTag = false;
		bool multiLineCommentEnd = false;
		
		//Interpret comments & precompiler flags
		if(buff[i] == '#')
			cPreCompilerTag = true;
		else{
			buffLen = strlen(buff);
			for(; i < buffLen-1; i++){
				if(buff[i] == '/'){
					if(buff[i+1] == '/')
						oneLineComment = true;
					if(buff[i+1] == '*'){
						multiLineComment = true;
					}
				}
				
				if(buff[i] == '*' && buff[i+1] == '/'){
					multiLineComment = false;
					multiLineCommentEnd = true;
				}
			}
		}
		
		int outScopeAmount = 0;
		if(!multiLineComment && !multiLineCommentEnd && !cPreCompilerTag && !oneLineComment){
			buffLen = placeLineEnding(buff);
			
			if(offset > offSets.top()){
				seenWords.push_back(unordered_set<string>());
				
				offSets.push(offset);
				closeWithLineEnding.push(false);
				
				if(string_isWord(buffPrevious, "struct") >= 0){
					closeWithLineEnding.top() = true;
					if(string_isWord(buffPrevious, "typedef") >= 0){
						structTypedef = true;
						getStructName(buffPrevious, structName);
					}
					else structTypedef = false;
				}
				
				if(string_isWord(buffPrevious, "class") >= 0){
					closeWithLineEnding.top() = true;
				}
				
				if(buffPrevious[buffPreviousLen-1] == '\n')
					buffPrevious[buffPreviousLen-2] = '{';
				else 
					buffPrevious[buffPreviousLen-1] = '{';
			} 
			
			outScopeAmount = closeKeys(offset, beauty);			
		}
		lineImplications(buffPrevious);
		while(outScopeAmount--)
			seenWords.pop_back();
		fputs(emptyLinesBuffPrevious, output);
		fputs(buffPrevious, output);
		
		strcpy(buffPrevious, buff);
		strcpy(emptyLinesBuffPrevious, emptyLinesBuff);
		
		buffPreviousLen = buffLen;
    }
	
	int outScopeAmount = closeKeys(0, beauty);
	lineImplications(buffPrevious);
	while(outScopeAmount--)
		seenWords.pop_back();
	fputs(emptyLinesBuffPrevious, output);
	fputs(buffPrevious, output);
	fclose(input);
	fclose(output);
}