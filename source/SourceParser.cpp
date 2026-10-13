//
//  SourceParser.cpp
//  HLSL2ALL
//
//  Created by Gabriele Di Bari.
//  Copyright © 2018 Gabriele Di Bari. All rights reserved.
//
#include "SourceParser.h"
#include <cctype>
#include <cstddef>
#include <cstring>

namespace HLSL2ALL
{
namespace Parser
{
namespace Aux
{
	//////////////////////////////////////////////////////
	// SOURCE SCAN UTILS (pstr: current char, pstr_end: end of the source; at least the chars
	// tested are left before reading them)
	static inline bool is_identifier(char c)
	{
		return std::isalnum((unsigned char)c) || c == '_';
	}

	static inline bool is_start_line_comment(const char* pstr, const char* pstr_end)
	{
		return (pstr_end - pstr) >= 2 && pstr[0] == '/' && pstr[1] == '/';
	}

	static inline bool is_start_multy_line_comment(const char* pstr, const char* pstr_end)
	{
		return (pstr_end - pstr) >= 2 && pstr[0] == '/' && pstr[1] == '*';
	}

	static inline bool is_end_multy_line_comment(const char* pstr, const char* pstr_end)
	{
		return (pstr_end - pstr) >= 2 && pstr[0] == '*' && pstr[1] == '/';
	}

	static inline bool is_start_directive(const char* pstr, const char* pstr_end)
	{
		return pstr < pstr_end && pstr[0] == '#';
	}

	static inline bool is_end_line(const char* pstr, const char* pstr_end)
	{
		return pstr < pstr_end && pstr[0] == '\n';
	}

	//size of a '\' at the end of a line ("\\\n" or "\\\r\n"), 0 if it is not
	static inline size_t line_continuation_size(const char* pstr, const char* pstr_end)
	{
		const ptrdiff_t left = pstr_end - pstr;
		if (left >= 2 && pstr[0] == '\\' && pstr[1] == '\n') return 2;
		if (left >= 3 && pstr[0] == '\\' && pstr[1] == '\r' && pstr[2] == '\n') return 3;
		return 0;
	}

	//a keyword (not the start of a longer identifier)
	static inline bool is_keyword(const char* pstr, const char* pstr_end, const char* keyword)
	{
		const size_t size = std::strlen(keyword);
		if (size_t(pstr_end - pstr) < size || std::strncmp(pstr, keyword, size) != 0) return false;
		return (pstr + size) == pstr_end || !is_identifier(pstr[size]);
	}

	static inline void skip_line_space(const char*& pstr, const char* pstr_end)
	{
		while (pstr < pstr_end && (*pstr == ' ' || *pstr == '\t')) ++pstr;
	}

	static inline void skip_space(const char*& pstr, const char* pstr_end)
	{
		while (pstr < pstr_end && std::isspace((unsigned char)*pstr)) ++pstr;
	}

	//jump an identifier, true if it is the name
	static inline bool skip_identifier_is(const char*& pstr, const char* pstr_end, const std::string& name)
	{
		const char* pstr_start = pstr;
		while (pstr < pstr_end && is_identifier(*pstr)) ++pstr;
		return size_t(pstr - pstr_start) == name.size() && std::strncmp(pstr_start, name.c_str(), name.size()) == 0;
	}
}

	//see SourceParser.h: a small state machine, comments and directives are skipped
	bool source_may_have_function(const std::string& source, const std::string& name)
	{
		//states of the scan
		enum SourceState
		{
			SS_CODE,               //HLSL code
			SS_LINE_COMMENT,       //from // to the end of the line
			SS_MULTY_LINE_COMMENT, //from /* to */
			SS_DEFINE,             //from #define to the end of the line ('\' goes on)
			SS_DIRECTIVE           //the other directives (#include, #if...), to the end of the line
		};
		//helpers
		using namespace Aux;
		//test
		if (name.empty()) return false;
		//scan
		SourceState state = SS_CODE;
		const char* pstr = source.data();
		const char* pstr_end = source.data() + source.size();
		while (pstr < pstr_end)
		{
			switch (state)
			{
			case SS_CODE:
				//comments
				if (is_start_line_comment(pstr, pstr_end))
				{
					state = SS_LINE_COMMENT;
					pstr += 2;
				}
				else if (is_start_multy_line_comment(pstr, pstr_end))
				{
					state = SS_MULTY_LINE_COMMENT;
					pstr += 2;
				}
				//directive: #define or another one
				else if (is_start_directive(pstr, pstr_end))
				{
					++pstr;
					skip_line_space(pstr, pstr_end);
					if (is_keyword(pstr, pstr_end, "define"))
					{
						state = SS_DEFINE;
						pstr += 6;
					}
					else
					{
						state = SS_DIRECTIVE;
					}
				}
				//identifier: a call or a declaration of the function
				else if (is_identifier(*pstr))
				{
					if (skip_identifier_is(pstr, pstr_end, name))
					{
						const char* pstr_next = pstr;
						skip_space(pstr_next, pstr_end);
						if (pstr_next < pstr_end && *pstr_next == '(') return true;
					}
				}
				else
				{
					++pstr;
				}
			break;
			case SS_LINE_COMMENT:
				if (is_end_line(pstr, pstr_end)) state = SS_CODE;
				++pstr;
			break;
			case SS_MULTY_LINE_COMMENT:
				if (is_end_multy_line_comment(pstr, pstr_end))
				{
					state = SS_CODE;
					pstr += 2;
				}
				else
				{
					++pstr;
				}
			break;
			case SS_DEFINE:
				//goes on the next line
				if (size_t size = line_continuation_size(pstr, pstr_end))
				{
					pstr += size;
				}
				//end of the define
				else if (is_end_line(pstr, pstr_end))
				{
					state = SS_CODE;
					++pstr;
				}
				//comment to the end of the line
				else if (is_start_line_comment(pstr, pstr_end))
				{
					state = SS_LINE_COMMENT;
					pstr += 2;
				}
				//the name in a macro
				else if (is_identifier(*pstr))
				{
					if (skip_identifier_is(pstr, pstr_end, name)) return true;
				}
				else
				{
					++pstr;
				}
			break;
			case SS_DIRECTIVE:
				//goes on the next line
				if (size_t size = line_continuation_size(pstr, pstr_end))
				{
					pstr += size;
				}
				//end of the directive
				else if (is_end_line(pstr, pstr_end))
				{
					state = SS_CODE;
					++pstr;
				}
				else
				{
					++pstr;
				}
			break;
			}
		}
		return false;
	}
}
}
