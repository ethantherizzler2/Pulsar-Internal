#pragma once
#include <string>

class Decompiler
{
public:
	static std::string DecompileLocalScript(uintptr_t LocalScript);

	static std::string DecompileModuleScript(uintptr_t ModuleScript);
};