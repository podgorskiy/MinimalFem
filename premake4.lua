#!lua
solution "MinimalFEM"
	configurations {
		"Debug",
		"Release",
	}

	language "C++"
	
	location ("projects/" .. _ACTION)
	
	startproject "MinimalFEM"
	
	flags { "StaticRuntime",  "FloatFast"}	
	
	configuration {"Debug"}	
		flags { "Symbols" }
	configuration {"Release"}
		flags { "Optimize" }	

project "MinimalFEM"	
	kind "ConsoleApp"	
	language "C++"	
	files { "main.cpp" }

	targetdir("")	

	includedirs {
		"eigen"
	}	
