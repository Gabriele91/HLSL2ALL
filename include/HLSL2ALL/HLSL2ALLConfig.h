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
// The containers of the interface of HLSL2ALL (its sources, its outputs): the ones of the STL by
// default; a library may give its own (e.g. with its allocator) by HLSL2ALL_CUSTOM_CONFIGURATION_FILE,
// a header that defines HLSL2ALL::String and HLSL2ALL::Vector<T> (as std::string, std::vector<T>)
#ifdef HLSL2ALL_CUSTOM_CONFIGURATION_FILE
	#include HLSL2ALL_CUSTOM_CONFIGURATION_FILE
#else
namespace HLSL2ALL
{
	using String = std::string;
	template < class T >
	using Vector = std::vector<T>;
}
#endif
