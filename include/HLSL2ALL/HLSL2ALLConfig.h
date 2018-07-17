#include <vector>
#include <string>
#include <tuple>

//only windows need dllexport/dllimport keywords
#ifdef _WIN32
	#define HLSL2ALL_DLL_EXPORT __declspec(dllexport)
	#define HLSL2ALL_DLL_IMPORT __declspec(dllimport)
#else 
	#define HLSL2ALL_DLL_EXPORT 
	#define HLSL2ALL_DLL_IMPORT 
#endif

// import/export
#if defined(HLSL2ALL_LIBRARY_EXPORT)
	// inside DLL
	#define HLSL2ALL_API   HLSL2ALL_DLL_EXPORT
#elif defined(HLSL2ALL_LIBRARY_IMPORT)
	// outside DLL
	#define HLSL2ALL_API   HLSL2ALL_DLL_IMPORT
#else 
	// static
	#define HLSL2ALL_API
#endif